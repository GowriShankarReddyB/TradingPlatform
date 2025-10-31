#include "pulseexec/OrderManager.hpp"
#include "pulseexec/DBWriter.hpp"
#include "pulseexec/Logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace pulseexec {

OrderManager::OrderManager(std::shared_ptr<Logger> logger, std::shared_ptr<DBWriter> db_writer)
    : logger_(logger), db_writer_(db_writer) {}

OrderManager::~OrderManager() = default;

std::string OrderManager::generate_client_order_id() {
  auto counter = order_counter_.fetch_add(1, std::memory_order_relaxed);
  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

  std::ostringstream oss;
  oss << "ORDER_" << now_ms << "_" << counter;
  return oss.str();
}

std::string OrderManager::create_order(const OrderRequest& request) {
  // Generate client order ID if not provided
  std::string client_order_id =
      request.client_order_id.empty() ? generate_client_order_id() : request.client_order_id;

  // Check for duplicate
  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    if (orders_by_client_id_.find(client_order_id) != orders_by_client_id_.end()) {
      if (logger_) {
        logger_->log_error("OrderManager", "Duplicate client_order_id: " + client_order_id);
      }
      return ""; // Return empty string on error
    }
  }

  // Create order with timestamp
  auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();

  Order order(client_order_id, request, now_us);

  // Insert into map
  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    orders_by_client_id_[client_order_id] = std::make_unique<OrderEntry>(order);
  }

  // Log creation
  if (logger_) {
    logger_->log_info("OrderManager",
                      "Created order: " + client_order_id + " for " + request.symbol);
  }

  // Persist to database
  if (db_writer_) {
    db_writer_->write_order(order);
  }

  // Notify callbacks
  notify_update(order);

  return client_order_id;
}

bool OrderManager::update_order(const std::string& client_order_id, OrderState new_state,
                                 const std::string& exchange_order_id, double filled_amount,
                                 const std::string& error_msg) {
  OrderEntry* entry = nullptr;

  // Get entry pointer (under map lock)
  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = orders_by_client_id_.find(client_order_id);
    if (it == orders_by_client_id_.end()) {
      if (logger_) {
        logger_->log_error("OrderManager", "Order not found: " + client_order_id);
      }
      return false;
    }
    entry = it->second.get();
  }

  // Update order (under per-order lock)
  {
    std::lock_guard<std::mutex> lock(entry->mutex);
    Order& order = entry->order;

    // Update state
    order.state = new_state;
    order.last_update_ts_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();

    // Update exchange ID if provided
    if (!exchange_order_id.empty() && order.exchange_order_id.empty()) {
      order.exchange_order_id = exchange_order_id;

      // Add to exchange ID map
      std::lock_guard<std::mutex> map_lock(map_mutex_);
      exchange_id_to_client_id_[exchange_order_id] = client_order_id;
    }

    // Update filled amount
    if (filled_amount > 0.0) {
      order.filled_amount = filled_amount;
    }

    // Update error message
    if (!error_msg.empty()) {
      order.error_message = error_msg;
    }

    // Log update
    if (logger_) {
      logger_->log_info("OrderManager", "Updated order: " + client_order_id + " -> " +
                                             to_string(new_state));
    }

    // Persist update
    if (db_writer_) {
      db_writer_->write_order(order);
    }

    // Notify callbacks
    notify_update(order);
  }

  return true;
}

bool OrderManager::get_order(const std::string& client_order_id, Order& out_order) const {
  OrderEntry* entry = nullptr;

  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = orders_by_client_id_.find(client_order_id);
    if (it == orders_by_client_id_.end()) {
      return false;
    }
    entry = it->second.get();
  }

  {
    std::lock_guard<std::mutex> lock(entry->mutex);
    out_order = entry->order;
  }

  return true;
}

bool OrderManager::get_order_by_exchange_id(const std::string& exchange_order_id,
                                              Order& out_order) const {
  std::string client_order_id;

  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = exchange_id_to_client_id_.find(exchange_order_id);
    if (it == exchange_id_to_client_id_.end()) {
      return false;
    }
    client_order_id = it->second;
  }

  return get_order(client_order_id, out_order);
}

bool OrderManager::has_order(const std::string& client_order_id) const {
  std::lock_guard<std::mutex> lock(map_mutex_);
  return orders_by_client_id_.find(client_order_id) != orders_by_client_id_.end();
}

void OrderManager::register_update_callback(OrderUpdateCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  update_callbacks_.push_back(std::move(callback));
}

std::vector<Order> OrderManager::get_active_orders() const {
  std::vector<Order> active_orders;

  std::lock_guard<std::mutex> lock(map_mutex_);
  for (const auto& [client_id, entry] : orders_by_client_id_) {
    std::lock_guard<std::mutex> order_lock(entry->mutex);
    if (entry->order.is_active()) {
      active_orders.push_back(entry->order);
    }
  }

  return active_orders;
}

std::vector<Order> OrderManager::get_all_orders() const {
  std::vector<Order> all_orders;

  std::lock_guard<std::mutex> lock(map_mutex_);
  for (const auto& [client_id, entry] : orders_by_client_id_) {
    std::lock_guard<std::mutex> order_lock(entry->mutex);
    all_orders.push_back(entry->order);
  }

  return all_orders;
}

bool OrderManager::mark_for_cancel(const std::string& client_order_id) {
  Order order;
  if (!get_order(client_order_id, order)) {
    return false;
  }

  if (!order.is_active()) {
    if (logger_) {
      logger_->log_warning("OrderManager",
                           "Cannot cancel inactive order: " + client_order_id);
    }
    return false;
  }

  // Note: Actual cancellation will be done by ExecutionGateway
  // This just validates that the order can be canceled
  return true;
}

void OrderManager::notify_update(const Order& order) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  for (const auto& callback : update_callbacks_) {
    callback(order);
  }
}

} // namespace pulseexec

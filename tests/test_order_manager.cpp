#include <catch2/catch_test_macros.hpp>
#include "pulseexec/OrderManager.hpp"
#include "pulseexec/Logger.hpp"
#include "pulseexec/DBWriter.hpp"
#include <memory>
#include <thread>
#include <chrono>

using namespace pulseexec;

TEST_CASE("OrderManager basic operations", "[order_manager]") {
  // Create logger and db_writer (can be nullptr for basic tests)
  auto logger = std::make_shared<Logger>();
  auto db_writer = std::make_shared<DBWriter>(":memory:", logger);
  
  db_writer->start();
  logger->start();

  OrderManager manager(logger, db_writer);

  SECTION("Create order") {
    OrderRequest req("BTC-PERPETUAL", Side::BUY, 50000.0, 1.0, OrderType::LIMIT);
    std::string client_id = manager.create_order(req);

    REQUIRE_FALSE(client_id.empty());
    REQUIRE(manager.has_order(client_id));

    Order order;
    REQUIRE(manager.get_order(client_id, order));
    REQUIRE(order.client_order_id == client_id);
    REQUIRE(order.request.symbol == "BTC-PERPETUAL");
    REQUIRE(order.state == OrderState::PENDING);
  }

  SECTION("Create order with custom client ID") {
    OrderRequest req("ETH-PERPETUAL", Side::SELL, 3000.0, 2.0, OrderType::LIMIT, "my_order_123");
    std::string client_id = manager.create_order(req);

    REQUIRE(client_id == "my_order_123");
    REQUIRE(manager.has_order("my_order_123"));
  }

  SECTION("Duplicate client ID prevention") {
    OrderRequest req("BTC-PERPETUAL", Side::BUY, 50000.0, 1.0, OrderType::LIMIT, "dup_test");
    std::string first = manager.create_order(req);
    REQUIRE(first == "dup_test");

    // Try to create duplicate
    std::string second = manager.create_order(req);
    REQUIRE(second.empty()); // Should fail
  }

  SECTION("Update order state") {
    OrderRequest req("BTC-PERPETUAL", Side::BUY, 50000.0, 1.0, OrderType::LIMIT);
    std::string client_id = manager.create_order(req);

    // Update to OPEN
    REQUIRE(manager.update_order(client_id, OrderState::OPEN, "exchange_123"));

    Order order;
    REQUIRE(manager.get_order(client_id, order));
    REQUIRE(order.state == OrderState::OPEN);
    REQUIRE(order.exchange_order_id == "exchange_123");

    // Update to FILLED
    REQUIRE(manager.update_order(client_id, OrderState::FILLED, "", 1.0));
    REQUIRE(manager.get_order(client_id, order));
    REQUIRE(order.state == OrderState::FILLED);
    REQUIRE(order.filled_amount == 1.0);
  }

  SECTION("Get order by exchange ID") {
    OrderRequest req("BTC-PERPETUAL", Side::BUY, 50000.0, 1.0, OrderType::LIMIT);
    std::string client_id = manager.create_order(req);

    manager.update_order(client_id, OrderState::OPEN, "exchange_456");

    Order order;
    REQUIRE(manager.get_order_by_exchange_id("exchange_456", order));
    REQUIRE(order.client_order_id == client_id);
    REQUIRE(order.exchange_order_id == "exchange_456");
  }

  SECTION("Get active orders") {
    // Create orders with different states
    OrderRequest req1("BTC-PERPETUAL", Side::BUY, 50000.0, 1.0, OrderType::LIMIT, "order1");
    OrderRequest req2("ETH-PERPETUAL", Side::BUY, 3000.0, 2.0, OrderType::LIMIT, "order2");
    OrderRequest req3("BTC-PERPETUAL", Side::SELL, 51000.0, 1.5, OrderType::LIMIT, "order3");

    manager.create_order(req1);
    manager.create_order(req2);
    manager.create_order(req3);

    manager.update_order("order1", OrderState::OPEN);
    manager.update_order("order2", OrderState::PARTIAL, "", 1.0);
    manager.update_order("order3", OrderState::FILLED, "", 1.5);

    auto active = manager.get_active_orders();
    REQUIRE(active.size() == 2); // order1 and order2 are active

    bool has_order1 = false;
    bool has_order2 = false;
    for (const auto& order : active) {
      if (order.client_order_id == "order1") has_order1 = true;
      if (order.client_order_id == "order2") has_order2 = true;
    }
    REQUIRE(has_order1);
    REQUIRE(has_order2);
  }

  SECTION("Mark for cancel") {
    OrderRequest req("BTC-PERPETUAL", Side::BUY, 50000.0, 1.0, OrderType::LIMIT);
    std::string client_id = manager.create_order(req);

    // Can't cancel pending order
    REQUIRE_FALSE(manager.mark_for_cancel(client_id));

    // Make it active
    manager.update_order(client_id, OrderState::OPEN);
    REQUIRE(manager.mark_for_cancel(client_id));

    // Can't cancel filled order
    manager.update_order(client_id, OrderState::FILLED);
    REQUIRE_FALSE(manager.mark_for_cancel(client_id));
  }

  SECTION("Order update callbacks") {
    bool callback_called = false;
    std::string callback_order_id;

    manager.register_update_callback([&](const Order& order) {
      callback_called = true;
      callback_order_id = order.client_order_id;
    });

    OrderRequest req("BTC-PERPETUAL", Side::BUY, 50000.0, 1.0, OrderType::LIMIT);
    std::string client_id = manager.create_order(req);

    // Give callback a moment to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    REQUIRE(callback_called);
    REQUIRE(callback_order_id == client_id);
  }

  logger->stop();
  db_writer->stop();
}

TEST_CASE("OrderManager concurrency", "[order_manager][concurrency]") {
  auto logger = std::make_shared<Logger>();
  auto db_writer = std::make_shared<DBWriter>(":memory:", logger);
  
  db_writer->start();
  logger->start();

  OrderManager manager(logger, db_writer);

  SECTION("Concurrent order creation") {
    const int num_threads = 4;
    const int orders_per_thread = 25;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
      threads.emplace_back([&manager, t, orders_per_thread]() {
        for (int i = 0; i < orders_per_thread; ++i) {
          OrderRequest req("BTC-PERPETUAL", Side::BUY, 50000.0, 1.0, OrderType::LIMIT);
          manager.create_order(req);
        }
      });
    }

    for (auto& thread : threads) {
      thread.join();
    }

    auto all_orders = manager.get_all_orders();
    REQUIRE(all_orders.size() == num_threads * orders_per_thread);
  }

  logger->stop();
  db_writer->stop();
}

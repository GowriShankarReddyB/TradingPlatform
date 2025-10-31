#include "pulseexec/Logger.hpp"
#include "pulseexec/DBWriter.hpp"
#include "pulseexec/OrderManager.hpp"
#include "pulseexec/ExecutionGateway.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <signal.h>

using namespace pulseexec;

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signal_handler(int signal) {
  std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
  g_running.store(false);
}

int main(int argc, char* argv[]) {
  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  std::cout << "PulseExec - High-Performance Order Execution & Management System" << std::endl;
  std::cout << "================================================================" << std::endl;

  // Load configuration from environment
  const char* api_key = std::getenv("DERIBIT_KEY");
  const char* api_secret = std::getenv("DERIBIT_SECRET");
  const char* rest_url = std::getenv("DERIBIT_REST_URL");
  const char* db_path = std::getenv("DB_PATH");

  if (!api_key || !api_secret) {
    std::cerr << "Error: DERIBIT_KEY and DERIBIT_SECRET must be set" << std::endl;
    std::cerr << "Copy .env.example to .env and configure your credentials" << std::endl;
    return 1;
  }

  std::string rest_url_str = rest_url ? rest_url : "https://test.deribit.com";
  std::string db_path_str = db_path ? db_path : "./pulseexec.db";

  std::cout << "Using Deribit REST URL: " << rest_url_str << std::endl;
  std::cout << "Using database: " << db_path_str << std::endl;

  // Initialize components
  auto logger = std::make_shared<Logger>("./logs/pulseexec.log");
  logger->set_min_level(LogLevel::INFO);
  logger->start();
  logger->log_info("Main", "PulseExec starting...");

  auto db_writer = std::make_shared<DBWriter>(db_path_str, logger);
  db_writer->start();

  auto order_manager = std::make_shared<OrderManager>(logger, db_writer);

  auto execution_gateway =
      std::make_shared<ExecutionGateway>(api_key, api_secret, rest_url_str, logger);

  // Register order update callback
  order_manager->register_update_callback([&logger](const Order& order) {
    logger->log_info("OrderUpdate", "Order " + order.client_order_id + " -> " +
                                         to_string(order.state));
  });

  std::cout << "\nPulseExec is running. Press Ctrl+C to stop." << std::endl;
  std::cout << "Ready to accept orders..." << std::endl;

  // Demo: Create a test order (commented out for safety)
  /*
  OrderRequest demo_req("BTC-PERPETUAL", Side::BUY, 50000.0, 0.001, OrderType::LIMIT);
  std::string order_id = order_manager->create_order(demo_req);
  
  if (!order_id.empty()) {
    logger->log_info("Main", "Created demo order: " + order_id);
    
    // Try to place it on the exchange
    auto result = execution_gateway->place_order(demo_req);
    if (result.success) {
      order_manager->update_order(order_id, OrderState::OPEN, result.exchange_order_id);
      logger->log_info("Main", "Order placed successfully: " + result.exchange_order_id);
    } else {
      order_manager->update_order(order_id, OrderState::REJECTED, "", 0.0, result.error_message);
      logger->log_error("Main", "Order placement failed: " + result.error_message);
    }
  }
  */

  // Main loop
  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Periodically log active orders
    static int counter = 0;
    if (++counter % 10 == 0) {
      auto active_orders = order_manager->get_active_orders();
      logger->log_info("Main", "Active orders: " + std::to_string(active_orders.size()));
    }
  }

  // Cleanup
  std::cout << "\nShutting down..." << std::endl;
  logger->log_info("Main", "Shutting down PulseExec");

  db_writer->stop();
  logger->stop();

  std::cout << "PulseExec stopped." << std::endl;
  return 0;
}

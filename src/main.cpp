#include "pulseexec/DBWriter.hpp"
#include "pulseexec/ExecutionGateway.hpp"
#include "pulseexec/Logger.hpp"
#include "pulseexec/OrderManager.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

using namespace pulseexec;

// Helper function to print usage
void print_usage(const char* program_name) {
  std::cout << "\n";
  std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║         PulseExec - Order Management CLI                    ║\n";
  std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

  std::cout << "USAGE:\n";
  std::cout << "  " << program_name << " <command> [options]\n\n";

  std::cout << "COMMANDS:\n\n";

  std::cout << "  place-order       Place a new order\n";
  std::cout << "    --symbol <SYM>    Symbol (e.g., BTC-PERPETUAL)\n";
  std::cout << "    --side <SIDE>     BUY or SELL\n";
  std::cout << "    --price <PRICE>   Limit price\n";
  std::cout << "    --amount <AMT>    Order amount\n";
  std::cout << "    --type <TYPE>     LIMIT or MARKET (default: LIMIT)\n";
  std::cout << "    --client-id <ID>  Optional client order ID\n";
  std::cout << "    Example: " << program_name
            << " place-order --symbol BTC-PERPETUAL --side BUY --price 50000 --amount 0.001\n\n";

  std::cout << "  cancel-order      Cancel an existing order\n";
  std::cout << "    --order-id <ID>   Order ID to cancel\n";
  std::cout << "    Example: " << program_name << " cancel-order --order-id ORDER_123456\n\n";

  std::cout << "  modify-order      Modify an existing order\n";
  std::cout << "    --order-id <ID>   Order ID to modify\n";
  std::cout << "    --price <PRICE>   New price\n";
  std::cout << "    --amount <AMT>    New amount\n";
  std::cout << "    Example: " << program_name
            << " modify-order --order-id ORDER_123 --price 51000 --amount 0.002\n\n";

  std::cout << "  list-orders       List all orders\n";
  std::cout << "    --active          Show only active orders (default: all)\n";
  std::cout << "    --symbol <SYM>    Filter by symbol\n";
  std::cout << "    Example: " << program_name << " list-orders --active\n\n";

  std::cout << "  get-order         Get details of a specific order\n";
  std::cout << "    --order-id <ID>   Order ID\n";
  std::cout << "    Example: " << program_name << " get-order --order-id ORDER_123456\n\n";

  std::cout << "  get-orderbook     Get orderbook snapshot\n";
  std::cout << "    --symbol <SYM>    Symbol (e.g., BTC-PERPETUAL)\n";
  std::cout << "    Example: " << program_name << " get-orderbook --symbol BTC-PERPETUAL\n\n";

  std::cout << "  interactive       Start interactive mode\n";
  std::cout << "    Example: " << program_name << " interactive\n\n";

  std::cout << "  help, -h, --help  Show this help message\n\n";

  std::cout << "ENVIRONMENT VARIABLES:\n";
  std::cout << "  DERIBIT_KEY       API key (required)\n";
  std::cout << "  DERIBIT_SECRET    API secret (required)\n";
  std::cout << "  DERIBIT_REST_URL  REST API URL (default: https://test.deribit.com)\n";
  std::cout << "  DB_PATH           Database path (default: ./pulseexec.db)\n";
  std::cout << "  LOG_FILE          Log file path (default: ./logs/pulseexec.log)\n\n";

  std::cout << "EXAMPLES:\n";
  std::cout << "  # Place a BUY order\n";
  std::cout << "  " << program_name
            << " place-order --symbol BTC-PERPETUAL --side BUY --price 50000 --amount 0.001\n\n";

  std::cout << "  # List all active orders\n";
  std::cout << "  " << program_name << " list-orders --active\n\n";

  std::cout << "  # Get orderbook\n";
  std::cout << "  " << program_name << " get-orderbook --symbol BTC-PERPETUAL\n\n";

  std::cout << "  # Interactive mode\n";
  std::cout << "  " << program_name << " interactive\n\n";
}

// Helper to get command line argument value
std::string get_arg(int argc, char* argv[], const std::string& option,
                    const std::string& default_val = "") {
  for (int i = 1; i < argc - 1; ++i) {
    if (std::string(argv[i]) == option) {
      return argv[i + 1];
    }
  }
  return default_val;
}

bool has_arg(int argc, char* argv[], const std::string& option) {
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == option) {
      return true;
    }
  }
  return false;
}

// Print order in a nice format
void print_order(const Order& order) {
  std::cout << "┌────────────────────────────────────────────────────────────┐\n";
  std::cout << "│ Client Order ID: " << std::left << std::setw(40)
            << order.client_order_id << "│\n";
  if (!order.exchange_order_id.empty()) {
    std::cout << "│ Exchange ID: " << std::setw(44) << order.exchange_order_id << "│\n";
  }
  std::cout << "├────────────────────────────────────────────────────────────┤\n";
  std::cout << "│ Symbol: " << std::setw(49) << order.request.symbol << "│\n";
  std::cout << "│ Side: " << std::setw(51) << to_string(order.request.side) << "│\n";
  std::cout << "│ Type: " << std::setw(51) << to_string(order.request.type) << "│\n";
  std::cout << "│ Price: " << std::setw(50) << order.request.price << "│\n";
  std::cout << "│ Amount: " << std::setw(49) << order.request.amount << "│\n";
  std::cout << "│ Filled: " << std::setw(49) << order.filled_amount << "│\n";
  std::cout << "├────────────────────────────────────────────────────────────┤\n";
  std::cout << "│ State: " << std::setw(50) << to_string(order.state) << "│\n";

  if (!order.error_message.empty()) {
    std::string truncated = order.error_message.substr(0, 49);
    std::cout << "│ Error: " << std::setw(50) << truncated << "│\n";
  }
  std::cout << "└────────────────────────────────────────────────────────────┘\n";
}

// Print orderbook
void print_orderbook(const OrderBook& book) {
  std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║  OrderBook: " << std::left << std::setw(46) << book.symbol << "║\n";
  std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

  std::cout << "ASKS (Sell Orders)\n";
  std::cout << "┌──────────────────┬──────────────────┐\n";
  std::cout << "│ Price            │ Amount           │\n";
  std::cout << "├──────────────────┼──────────────────┤\n";

  // Print asks in reverse (highest first)
  for (auto it = book.asks.rbegin(); it != book.asks.rend() && it != book.asks.rbegin() + 5;
       ++it) {
    std::cout << "│ " << std::setw(16) << std::fixed << std::setprecision(2) << it->price << " │ "
              << std::setw(16) << std::setprecision(8) << it->amount << " │\n";
  }

  std::cout << "╞══════════════════╪══════════════════╡\n";
  std::cout << "│ Spread: " << std::setw(8) << std::setprecision(2) << book.spread()
            << " │ Mid: " << std::setw(11) << book.mid_price() << " │\n";
  std::cout << "╞══════════════════╪══════════════════╡\n";

  std::cout << "│ Price            │ Amount           │\n";
  std::cout << "├──────────────────┼──────────────────┤\n";
  for (size_t i = 0; i < std::min(size_t(5), book.bids.size()); ++i) {
    const auto& bid = book.bids[i];
    std::cout << "│ " << std::setw(16) << std::fixed << std::setprecision(2) << bid.price << " │ "
              << std::setw(16) << std::setprecision(8) << bid.amount << " │\n";
  }
  std::cout << "└──────────────────┴──────────────────┘\n";
  std::cout << "BIDS (Buy Orders)\n\n";
}

// Interactive mode
void interactive_mode(std::shared_ptr<OrderManager> order_manager,
                      std::shared_ptr<ExecutionGateway> gateway,
                      std::shared_ptr<Logger> logger) {

  std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║           PulseExec Interactive Mode                         ║\n";
  std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

  while (true) {
    std::cout << "\n┌─────────────────────────────────────┐\n";
    std::cout << "│ 1. Place Order                      │\n";
    std::cout << "│ 2. Cancel Order                     │\n";
    std::cout << "│ 3. List Active Orders               │\n";
    std::cout << "│ 4. List All Orders                  │\n";
    std::cout << "│ 5. Get Order Details                │\n";
    std::cout << "│ 6. Get OrderBook                    │\n";
    std::cout << "│ 0. Exit                             │\n";
    std::cout << "└─────────────────────────────────────┘\n";
    std::cout << "Choice: ";

    int choice;
    std::cin >> choice;
    std::cin.ignore();

    try {
      switch (choice) {
      case 1: {
        // Place Order
        std::string symbol, side_str, type_str;
        double price, amount;

        std::cout << "\nSymbol (e.g., BTC-PERPETUAL): ";
        std::getline(std::cin, symbol);

        std::cout << "Side (BUY/SELL): ";
        std::getline(std::cin, side_str);
        std::transform(side_str.begin(), side_str.end(), side_str.begin(), ::toupper);

        std::cout << "Type (LIMIT/MARKET) [LIMIT]: ";
        std::getline(std::cin, type_str);
        if (type_str.empty())
          type_str = "LIMIT";
        std::transform(type_str.begin(), type_str.end(), type_str.begin(), ::toupper);

        std::cout << "Price: ";
        std::cin >> price;

        std::cout << "Amount: ";
        std::cin >> amount;
        std::cin.ignore();

        Side side = parse_side(side_str);
        OrderType type = parse_order_type(type_str);

        OrderRequest req(symbol, side, price, amount, type);
        std::string order_id = order_manager->create_order(req);

        std::cout << "\n✅ Order created locally: " << order_id << "\n";
        std::cout << "📡 Submitting to exchange...\n";

        auto result = gateway->place_order(req);

        if (result.success) {
          std::cout << "✅ Order placed on exchange: " << result.exchange_order_id << "\n";
          order_manager->update_order(order_id, OrderState::OPEN, result.exchange_order_id);
        } else {
          std::cout << "❌ Order rejected: " << result.error_message << "\n";
          order_manager->update_order(order_id, OrderState::REJECTED, "", 0.0,
                                       result.error_message);
        }
        break;
      }

      case 2: {
        // Cancel Order
        std::string order_id;
        std::cout << "\nOrder ID to cancel: ";
        std::getline(std::cin, order_id);

        Order order;
        if (!order_manager->get_order(order_id, order)) {
          std::cout << "❌ Order not found: " << order_id << "\n";
          break;
        }

        if (!order.exchange_order_id.empty()) {
          std::cout << "📡 Canceling on exchange...\n";
          auto result = gateway->cancel_order(order.exchange_order_id);

          if (result.success) {
            std::cout << "✅ Order canceled on exchange\n";
            order_manager->update_order(order_id, OrderState::CANCELED);
          } else {
            std::cout << "❌ Cancel failed: " << result.error_message << "\n";
          }
        } else {
          std::cout << "⚠️  Order not yet on exchange, canceling locally\n";
          order_manager->update_order(order_id, OrderState::CANCELED);
        }
        break;
      }

      case 3: {
        // List Active Orders
        auto orders = order_manager->get_active_orders();
        std::cout << "\n📋 Active Orders (" << orders.size() << ")\n\n";

        if (orders.empty()) {
          std::cout << "No active orders.\n";
        } else {
          for (const auto& order : orders) {
            std::cout << "• " << order.client_order_id << " | " << order.request.symbol << " | "
                      << to_string(order.request.side) << " | " << order.request.price << " x "
                      << order.request.amount << " | " << to_string(order.state) << "\n";
          }
        }
        break;
      }

      case 4: {
        // List All Orders
        auto orders = order_manager->get_all_orders();
        std::cout << "\n📋 All Orders (" << orders.size() << ")\n\n";

        if (orders.empty()) {
          std::cout << "No orders found.\n";
        } else {
          for (const auto& order : orders) {
            std::cout << "• " << order.client_order_id << " | " << order.request.symbol << " | "
                      << to_string(order.request.side) << " | " << order.request.price << " x "
                      << order.request.amount << " | " << to_string(order.state) << "\n";
          }
        }
        break;
      }

      case 5: {
        // Get Order Details
        std::string order_id;
        std::cout << "\nOrder ID: ";
        std::getline(std::cin, order_id);

        Order order;
        if (order_manager->get_order(order_id, order)) {
          std::cout << "\n";
          print_order(order);
        } else {
          std::cout << "❌ Order not found: " << order_id << "\n";
        }
        break;
      }

      case 6: {
        // Get OrderBook
        std::string symbol;

        std::cout << "\nSymbol (e.g., BTC-PERPETUAL): ";
        std::getline(std::cin, symbol);

        std::cout << "📡 Fetching orderbook...\n";
        OrderBook book;
        auto result = gateway->get_orderbook(symbol, book);

        if (result.success) {
          print_orderbook(book);
        } else {
          std::cout << "❌ Failed to fetch orderbook: " << result.error_message << "\n";
        }
        break;
      }

      case 0:
        std::cout << "\n👋 Goodbye!\n";
        return;

      default:
        std::cout << "❌ Invalid choice. Try again.\n";
      }
    } catch (const std::exception& e) {
      std::cout << "❌ Error: " << e.what() << "\n";
    }
  }
}

int main(int argc, char* argv[]) {
  // Check for help or no arguments
  if (argc < 2 || has_arg(argc, argv, "help") || has_arg(argc, argv, "--help") ||
      has_arg(argc, argv, "-h")) {
    print_usage(argv[0]);
    return 0;
  }

  // Get environment variables
  const char* api_key_env = std::getenv("DERIBIT_KEY");
  const char* api_secret_env = std::getenv("DERIBIT_SECRET");
  const char* rest_url_env = std::getenv("DERIBIT_REST_URL");
  const char* db_path_env = std::getenv("DB_PATH");
  const char* log_file_env = std::getenv("LOG_FILE");

  if (!api_key_env || !api_secret_env) {
    std::cerr << "❌ Error: DERIBIT_KEY and DERIBIT_SECRET must be set in environment.\n";
    std::cerr << "   Run: export $(cat .env | grep -v '^#' | xargs)\n";
    std::cerr << "   Or: ./run.sh <command>\n";
    return 1;
  }

  std::string api_key = api_key_env;
  std::string api_secret = api_secret_env;
  std::string rest_url = rest_url_env ? rest_url_env : "https://test.deribit.com";
  std::string db_path = db_path_env ? db_path_env : "./pulseexec.db";
  std::string log_file = log_file_env ? log_file_env : "./logs/pulseexec.log";

  // Initialize components
  auto logger = std::make_shared<Logger>(log_file, 10000);
  logger->set_min_level(LogLevel::INFO);
  auto db_writer = std::make_shared<DBWriter>(db_path, logger);
  auto order_manager = std::make_shared<OrderManager>(logger, db_writer);
  auto gateway = std::make_shared<ExecutionGateway>(api_key, api_secret, rest_url, logger);

  logger->start();
  db_writer->start();

  std::string command = argv[1];

  try {
    if (command == "place-order") {
      std::string symbol = get_arg(argc, argv, "--symbol");
      std::string side_str = get_arg(argc, argv, "--side");
      std::string price_str = get_arg(argc, argv, "--price");
      std::string amount_str = get_arg(argc, argv, "--amount");
      std::string type_str = get_arg(argc, argv, "--type", "LIMIT");
      std::string client_id = get_arg(argc, argv, "--client-id");

      if (symbol.empty() || side_str.empty() || price_str.empty() || amount_str.empty()) {
        std::cerr << "❌ Missing required arguments for place-order\n";
        print_usage(argv[0]);
        return 1;
      }

      std::transform(side_str.begin(), side_str.end(), side_str.begin(), ::toupper);
      std::transform(type_str.begin(), type_str.end(), type_str.begin(), ::toupper);

      Side side = parse_side(side_str);
      OrderType type = parse_order_type(type_str);
      double price = std::stod(price_str);
      double amount = std::stod(amount_str);

      OrderRequest req(symbol, side, price, amount, type, client_id);

      std::string order_id = order_manager->create_order(req);
      std::cout << "✅ Order created locally: " << order_id << "\n";
      std::cout << "📡 Submitting to exchange...\n";

      auto result = gateway->place_order(req);

      if (result.success) {
        std::cout << "✅ Order placed successfully!\n";
        std::cout << "   Exchange Order ID: " << result.exchange_order_id << "\n";
        order_manager->update_order(order_id, OrderState::OPEN, result.exchange_order_id);

        Order order;
        if (order_manager->get_order(order_id, order)) {
          std::cout << "\n";
          print_order(order);
        }
      } else {
        std::cout << "❌ Order rejected by exchange\n";
        std::cout << "   Error: " << result.error_message << "\n";
        order_manager->update_order(order_id, OrderState::REJECTED, "", 0.0, result.error_message);
      }

    } else if (command == "cancel-order") {
      std::string order_id = get_arg(argc, argv, "--order-id");

      if (order_id.empty()) {
        std::cerr << "❌ Missing required argument: --order-id\n";
        return 1;
      }

      Order order;
      if (!order_manager->get_order(order_id, order)) {
        std::cout << "❌ Order not found: " << order_id << "\n";
        return 1;
      }

      if (order.exchange_order_id.empty()) {
        std::cout << "⚠️  Order not yet on exchange, canceling locally\n";
        order_manager->update_order(order_id, OrderState::CANCELED);
        std::cout << "✅ Order canceled locally\n";
      } else {
        std::cout << "📡 Canceling order on exchange...\n";
        auto result = gateway->cancel_order(order.exchange_order_id);

        if (result.success) {
          std::cout << "✅ Order canceled successfully\n";
          order_manager->update_order(order_id, OrderState::CANCELED);
        } else {
          std::cout << "❌ Cancel failed: " << result.error_message << "\n";
        }
      }

    } else if (command == "modify-order") {
      std::string order_id = get_arg(argc, argv, "--order-id");
      std::string price_str = get_arg(argc, argv, "--price");
      std::string amount_str = get_arg(argc, argv, "--amount");

      if (order_id.empty() || (price_str.empty() && amount_str.empty())) {
        std::cerr << "❌ Missing required arguments for modify-order\n";
        return 1;
      }

      Order order;
      if (!order_manager->get_order(order_id, order)) {
        std::cout << "❌ Order not found: " << order_id << "\n";
        return 1;
      }

      double new_price = price_str.empty() ? order.request.price : std::stod(price_str);
      double new_amount = amount_str.empty() ? order.request.amount : std::stod(amount_str);

      if (order.exchange_order_id.empty()) {
        std::cout << "⚠️  Order not yet on exchange, cannot modify\n";
        return 1;
      }

      std::cout << "📡 Modifying order on exchange...\n";
      auto result = gateway->modify_order(order.exchange_order_id, new_price, new_amount);

      if (result.success) {
        std::cout << "✅ Order modified successfully\n";
      } else {
        std::cout << "❌ Modify failed: " << result.error_message << "\n";
      }

    } else if (command == "list-orders") {
      bool active_only = has_arg(argc, argv, "--active");
      std::string symbol_filter = get_arg(argc, argv, "--symbol");

      auto orders = active_only ? order_manager->get_active_orders() : order_manager->get_all_orders();

      // Filter by symbol if specified
      if (!symbol_filter.empty()) {
        orders.erase(std::remove_if(orders.begin(), orders.end(),
                                     [&symbol_filter](const Order& o) {
                                       return o.request.symbol != symbol_filter;
                                     }),
                     orders.end());
      }

      std::cout << "\n📋 " << (active_only ? "Active" : "All") << " Orders";
      if (!symbol_filter.empty()) {
        std::cout << " (" << symbol_filter << ")";
      }
      std::cout << " - Total: " << orders.size() << "\n\n";

      if (orders.empty()) {
        std::cout << "No orders found.\n";
      } else {
        std::cout << "┌────────────────────┬───────────────┬──────┬─────────┬─────────┬───────────┐\n";
        std::cout << "│ Order ID           │ Symbol        │ Side │ Price   │ Amount  │ State     │\n";
        std::cout << "├────────────────────┼───────────────┼──────┼─────────┼─────────┼───────────┤\n";

        for (const auto& order : orders) {
          std::cout << "│ " << std::left << std::setw(18)
                    << order.client_order_id.substr(0, 18) << " │ " << std::setw(13)
                    << order.request.symbol.substr(0, 13) << " │ " << std::setw(4)
                    << to_string(order.request.side).substr(0, 4) << " │ " << std::setw(7)
                    << std::fixed << std::setprecision(2) << order.request.price << " │ "
                    << std::setw(7) << std::setprecision(4) << order.request.amount << " │ "
                    << std::setw(9) << to_string(order.state).substr(0, 9) << " │\n";
        }
        std::cout << "└────────────────────┴───────────────┴──────┴─────────┴─────────┴───────────┘\n";
      }

    } else if (command == "get-order") {
      std::string order_id = get_arg(argc, argv, "--order-id");

      if (order_id.empty()) {
        std::cerr << "❌ Missing required argument: --order-id\n";
        return 1;
      }

      Order order;
      if (order_manager->get_order(order_id, order)) {
        std::cout << "\n";
        print_order(order);
      } else {
        std::cout << "❌ Order not found: " << order_id << "\n";
        return 1;
      }

    } else if (command == "get-orderbook") {
      std::string symbol = get_arg(argc, argv, "--symbol");

      if (symbol.empty()) {
        std::cerr << "❌ Missing required argument: --symbol\n";
        return 1;
      }

      std::cout << "📡 Fetching orderbook for " << symbol << "...\n";
      OrderBook book;
      auto result = gateway->get_orderbook(symbol, book);

      if (result.success) {
        print_orderbook(book);
      } else {
        std::cout << "❌ Failed to fetch orderbook: " << result.error_message << "\n";
        return 1;
      }

    } else if (command == "interactive") {
      interactive_mode(order_manager, gateway, logger);

    } else {
      std::cerr << "❌ Unknown command: " << command << "\n";
      print_usage(argv[0]);
      return 1;
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Error: " << e.what() << "\n";
    return 1;
  }

  // Graceful shutdown
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  logger->stop();
  db_writer->stop();

  return 0;
}

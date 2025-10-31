# Getting Started with PulseExec

## Quick Start (5 minutes)

### 1. Install Dependencies

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    clang-format
```

### 2. Build the Project

```bash
# Use the quick build script
./build.sh

# Or manually:
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### 3. Run Tests

```bash
cd build
./test_runner --success
```

Expected output:
```
All tests passed (20 assertions in 10 test cases)
```

### 4. Configure Deribit Test Credentials

```bash
# Copy the environment template
cp .env.example .env

# Edit with your credentials
nano .env
```

Get your Deribit Test API credentials at: https://test.deribit.com/

Required variables:
```bash
DERIBIT_KEY=your_test_api_key_here
DERIBIT_SECRET=your_test_api_secret_here
```

### 5. Run PulseExec

```bash
# Load environment variables
source .env

# Run the application
./pulseexec
```

## Understanding the Code

### Example 1: Creating an Order

```cpp
#include "pulseexec/OrderManager.hpp"
#include "pulseexec/OrderRequest.hpp"

// Create a buy order for BTC-PERPETUAL
OrderRequest req(
    "BTC-PERPETUAL",    // symbol
    Side::BUY,          // side
    50000.0,            // price
    0.001,              // amount (0.001 BTC)
    OrderType::LIMIT    // type
);

// Submit to OrderManager
std::string order_id = order_manager->create_order(req);
std::cout << "Created order: " << order_id << std::endl;
```

### Example 2: Placing Order on Exchange

```cpp
#include "pulseexec/ExecutionGateway.hpp"

// Place order on Deribit Test
auto result = execution_gateway->place_order(req);

if (result.success) {
    std::cout << "Order placed: " << result.exchange_order_id << std::endl;
    
    // Update local order with exchange ID
    order_manager->update_order(
        order_id, 
        OrderState::OPEN, 
        result.exchange_order_id
    );
} else {
    std::cerr << "Failed: " << result.error_message << std::endl;
    
    order_manager->update_order(
        order_id, 
        OrderState::REJECTED, 
        "", 
        0.0, 
        result.error_message
    );
}
```

### Example 3: Monitoring Orders

```cpp
// Register callback for order updates
order_manager->register_update_callback([](const Order& order) {
    std::cout << "Order " << order.client_order_id 
              << " -> " << to_string(order.state) 
              << std::endl;
});

// Query active orders
auto active_orders = order_manager->get_active_orders();
std::cout << "Active orders: " << active_orders.size() << std::endl;

for (const auto& order : active_orders) {
    std::cout << "  " << order.client_order_id 
              << ": " << order.request.symbol
              << " " << to_string(order.request.side)
              << " @ " << order.request.price
              << std::endl;
}
```

### Example 4: Getting OrderBook

```cpp
OrderBook book;
auto result = execution_gateway->get_orderbook("BTC-PERPETUAL", book);

if (result.success) {
    std::cout << "Best bid: " << book.best_bid() << std::endl;
    std::cout << "Best ask: " << book.best_ask() << std::endl;
    std::cout << "Spread: " << book.spread() << std::endl;
    std::cout << "Mid price: " << book.mid_price() << std::endl;
}
```

## Project Structure Walkthrough

```
tradingProject/
│
├── include/pulseexec/       # Public API headers
│   ├── Order.hpp            # → Order data model
│   ├── OrderRequest.hpp     # → Request structure
│   ├── OrderManager.hpp     # → Order lifecycle manager
│   ├── ExecutionGateway.hpp # → REST client for Deribit
│   ├── Logger.hpp           # → Async logging
│   └── DBWriter.hpp         # → SQLite persistence
│
├── src/                     # Implementation
│   ├── main.cpp             # → Application entry point
│   ├── OrderManager.cpp     # → Order management logic
│   ├── ExecutionGateway.cpp # → HTTP client with retry
│   ├── Logger.cpp           # → Background logging thread
│   └── DBWriter.cpp         # → Database writer thread
│
└── tests/                   # Unit tests
    ├── test_order.cpp       # → Order model tests
    └── test_order_manager.cpp # → Manager tests
```

## Common Tasks

### Add a New Order

```bash
# From build directory
./pulseexec

# In the code (main.cpp), uncomment the demo section:
OrderRequest demo_req("BTC-PERPETUAL", Side::BUY, 50000.0, 0.001, OrderType::LIMIT);
std::string order_id = order_manager->create_order(demo_req);
```

### Check Database

```bash
# Install sqlite3
sudo apt-get install sqlite3

# Query orders
sqlite3 pulseexec.db "SELECT * FROM orders;"

# Check schema
sqlite3 pulseexec.db ".schema orders"
```

### View Logs

```bash
# Logs are written to logs/pulseexec.log
tail -f logs/pulseexec.log

# Pretty print JSON logs
cat logs/pulseexec.log | jq '.'
```

### Format Code

```bash
# Format all source files
find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Check formatting without modifying
find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format --dry-run -Werror
```

### Run Specific Tests

```bash
cd build

# Run all tests
./test_runner

# Run only order tests
./test_runner "[order]"

# Run only manager tests
./test_runner "[order_manager]"

# Run concurrency tests
./test_runner "[concurrency]"

# Verbose output
./test_runner --success
```

### Clean Build

```bash
# Remove build directory
rm -rf build

# Remove database and logs
rm -f pulseexec.db pulseexec.db-shm pulseexec.db-wal
rm -rf logs/

# Full clean rebuild
./build.sh
```

## Development Workflow

### 1. Make Changes
```bash
# Edit source files
nano src/OrderManager.cpp
```

### 2. Format Code
```bash
clang-format -i src/OrderManager.cpp
```

### 3. Rebuild
```bash
cd build
make -j$(nproc)
```

### 4. Run Tests
```bash
./test_runner
```

### 5. Test Application
```bash
./pulseexec
```

## Debugging

### Enable Debug Build

```bash
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Run with GDB

```bash
gdb ./pulseexec
(gdb) run
(gdb) backtrace  # on crash
```

### Run with Valgrind

```bash
sudo apt-get install valgrind
valgrind --leak-check=full ./pulseexec
```

### Verbose Logging

```cpp
// In main.cpp
logger->set_min_level(LogLevel::DEBUG);
```

## Performance Tips

### Release Build
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### Profile with perf
```bash
sudo apt-get install linux-tools-generic
perf record ./pulseexec
perf report
```

## Troubleshooting

### "CURL not found"
```bash
sudo apt-get install libcurl4-openssl-dev
```

### "SQLite3 not found"
```bash
sudo apt-get install libsqlite3-dev
```

### "Boost not found"
```bash
sudo apt-get install libboost-all-dev
```

### "Database locked"
```bash
# Kill other instances
killall pulseexec

# Remove lock files
rm -f pulseexec.db-shm pulseexec.db-wal
```

### "Permission denied on build.sh"
```bash
chmod +x build.sh
```

## Next Steps

1. **Explore the code**: Start with `src/main.cpp` and follow the components
2. **Run tests**: Understand how components work via `tests/`
3. **Modify examples**: Try creating different order types
4. **Add features**: Implement position tracking or new order types
5. **Phase 2**: Add WebSocket components (MarketDataFeed, WebSocketServer)

## Resources

- **SPEC.md**: Full technical specification
- **README.md**: Comprehensive documentation
- **docs/PHASE1_SUMMARY.md**: Implementation details
- **Deribit API**: https://docs.deribit.com/

## Getting Help

If you encounter issues:

1. Check the logs: `logs/pulseexec.log`
2. Review test output: `./test_runner --success`
3. Verify dependencies: Run `build.sh` to see missing packages
4. Check database: `sqlite3 pulseexec.db ".tables"`
5. Verify environment: `echo $DERIBIT_KEY`

---

**Happy coding! 🚀**

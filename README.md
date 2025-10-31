# PulseExec

**High-Performance Order Execution & Management System**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

PulseExec is a low-latency Order Execution & Management System (OEMS) implemented in modern C++ that integrates with Deribit Test for cryptocurrency trading.

## Features (MVP)

- ✅ **Order Management**: Place, cancel, and modify orders (Spot & Perpetual contracts)
- ✅ **Order Lifecycle**: Complete order state management with idempotency
- ✅ **Persistence**: SQLite with WAL mode for orders, positions, and metrics
- ✅ **Async Logging**: JSON-formatted event logging with bounded queue
- ✅ **Retry Logic**: Exponential backoff with jitter for transient errors (HTTP 429/5xx)
- ✅ **Concurrency**: Fine-grained per-order locking and thread-safe operations
- ✅ **Testing**: Comprehensive unit tests with Catch2

## Architecture

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  OrderManager   │────▶│ ExecutionGateway │────▶│  Deribit Test   │
│    (OMS Core)   │     │  (REST Client)   │     │   (Exchange)    │
└─────────────────┘     └──────────────────┘     └─────────────────┘
         │                                                
         ├──────────────┬──────────────┐
         ▼              ▼              ▼
┌─────────────┐  ┌───────────┐  ┌──────────┐
│  DBWriter   │  │  Logger   │  │ Callbacks│
│  (SQLite)   │  │  (Async)  │  │          │
└─────────────┘  └───────────┘  └──────────┘
```

## Tech Stack

- **Language**: C++17/C++20
- **Build**: CMake 3.15+
- **HTTP Client**: libcurl
- **JSON**: nlohmann/json
- **Database**: SQLite3 (WAL mode)
- **Testing**: Catch2
- **Formatting**: clang-format (LLVM style)

## Prerequisites

### Ubuntu/Debian
```bash
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

### Compiler Requirements
- GCC 10+ or Clang 13+
- C++17 support required

## Building

### 1. Clone the repository
```bash
git clone <your-repo-url>
cd tradingProject
```

### 2. Configure environment
```bash
cp .env.example .env
# Edit .env and add your Deribit Test credentials
nano .env
```

Get your Deribit Test API credentials at: https://test.deribit.com/

### 3. Build the project
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### 4. Run tests
```bash
./test_runner
```

### 5. Run the application
```bash
# From the build directory
source ../.env  # Load environment variables
./pulseexec
```

## Configuration

All configuration is done via environment variables. See `.env.example` for available options:

| Variable | Description | Default |
|----------|-------------|---------|
| `DERIBIT_KEY` | Deribit Test API key | *Required* |
| `DERIBIT_SECRET` | Deribit Test API secret | *Required* |
| `DERIBIT_REST_URL` | Deribit REST endpoint | `https://test.deribit.com` |
| `DB_PATH` | SQLite database path | `./pulseexec.db` |
| `LOG_LEVEL` | Minimum log level | `INFO` |
| `LOG_FILE` | Log file path | `./logs/pulseexec.log` |

## Project Structure

```
.
├── CMakeLists.txt          # Root build configuration
├── .clang-format           # Code formatting rules
├── .env.example            # Environment template
├── include/
│   └── pulseexec/          # Public headers
│       ├── Order.hpp
│       ├── OrderRequest.hpp
│       ├── OrderBook.hpp
│       ├── OrderManager.hpp
│       ├── ExecutionGateway.hpp
│       ├── Logger.hpp
│       └── DBWriter.hpp
├── src/                    # Implementation files
│   ├── main.cpp
│   ├── OrderManager.cpp
│   ├── ExecutionGateway.cpp
│   ├── Logger.cpp
│   └── DBWriter.cpp
├── tests/                  # Unit tests
│   ├── test_main.cpp
│   ├── test_order.cpp
│   └── test_order_manager.cpp
├── bench/                  # Benchmarks (Phase 2)
└── docs/                   # Documentation
```

## Usage Examples

### Creating an Order

```cpp
#include "pulseexec/OrderManager.hpp"
#include "pulseexec/OrderRequest.hpp"

// Create order request
OrderRequest req(
    "BTC-PERPETUAL",           // symbol
    Side::BUY,                 // side
    50000.0,                   // price
    0.001,                     // amount
    OrderType::LIMIT,          // type
    "my_order_123"             // client_order_id (optional)
);

// Submit to OrderManager
std::string order_id = order_manager->create_order(req);

// Place on exchange via ExecutionGateway
auto result = execution_gateway->place_order(req);
if (result.success) {
    order_manager->update_order(
        order_id, 
        OrderState::OPEN, 
        result.exchange_order_id
    );
}
```

### Querying Orders

```cpp
// Get specific order
Order order;
if (order_manager->get_order("my_order_123", order)) {
    std::cout << "State: " << to_string(order.state) << std::endl;
}

// Get all active orders
auto active = order_manager->get_active_orders();
for (const auto& order : active) {
    std::cout << order.client_order_id << ": " 
              << to_string(order.state) << std::endl;
}
```

### Order Update Callbacks

```cpp
order_manager->register_update_callback([](const Order& order) {
    std::cout << "Order updated: " << order.client_order_id 
              << " -> " << to_string(order.state) << std::endl;
});
```

## Testing

### Run all tests
```bash
cd build
./test_runner
```

### Run specific test cases
```bash
./test_runner "[order]"              # Order model tests
./test_runner "[order_manager]"      # OrderManager tests
./test_runner "[concurrency]"        # Concurrency tests
```

### Integration Tests
Integration tests require Deribit Test credentials:
```bash
export DERIBIT_KEY="your_test_key"
export DERIBIT_SECRET="your_test_secret"
./test_runner "[integration]"
```

## Code Formatting

This project uses clang-format with LLVM style:

```bash
# Format all source files
find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Check formatting
find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format --dry-run -Werror
```

## Database Schema

### Orders Table
```sql
CREATE TABLE orders (
    client_order_id TEXT PRIMARY KEY,
    exchange_order_id TEXT,
    symbol TEXT NOT NULL,
    side TEXT NOT NULL,
    price REAL NOT NULL,
    amount REAL NOT NULL,
    order_type TEXT NOT NULL,
    state TEXT NOT NULL,
    filled_amount REAL DEFAULT 0.0,
    created_ts_us INTEGER NOT NULL,
    last_update_ts_us INTEGER NOT NULL,
    error_message TEXT
);
```

## Performance Considerations

### Current (MVP)
- Per-order fine-grained locking
- Bounded queues for backpressure
- Single-threaded DB writer (WAL mode)
- Async logging with bounded queue

### Future (Phase 2)
- Lock-free queues
- Socket tuning (TCP_NODELAY, SO_REUSEPORT)
- Benchmarking suite (p50/p90/p99 latencies)
- Memory pool allocators
- WebSocket market data feed
- WebSocket server for clients

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Format your code (`clang-format -i <files>`)
4. Run tests (`make test`)
5. Commit your changes (`git commit -m 'Add amazing feature'`)
6. Push to the branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Author

**B Gowri Shankar Reddy**

## Links & References

- [Deribit API Documentation](https://docs.deribit.com/)
- [SQLite WAL Mode](https://sqlite.org/wal.html)
- [Catch2 Documentation](https://github.com/catchorg/Catch2)
- [nlohmann/json](https://github.com/nlohmann/json)

## Roadmap

### Phase 1 - MVP ✅
- [x] Core data models (Order, OrderRequest, OrderBook)
- [x] OrderManager with lifecycle management
- [x] ExecutionGateway with retry logic
- [x] SQLite persistence with WAL
- [x] Async logger
- [x] Unit tests
- [x] Build system (CMake)
- [x] Documentation

### Phase 2 - Performance & Features
- [ ] MarketDataFeed (WebSocket client)
- [ ] WebSocketServer (broadcast updates)
- [ ] Benchmarking suite
- [ ] Performance profiling
- [ ] Socket-level optimizations
- [ ] Lock-free queues
- [ ] Integration tests
- [ ] CI/CD (GitHub Actions)

## Troubleshooting

### Build Issues

**CMake can't find dependencies:**
```bash
# Install missing dependencies
sudo apt-get install libboost-all-dev libcurl4-openssl-dev libsqlite3-dev
```

**Compiler errors:**
```bash
# Ensure C++17 support
g++ --version  # Should be 10+
clang++ --version  # Should be 13+
```

### Runtime Issues

**Database locked:**
- Ensure only one instance is running
- Check file permissions on DB path

**API errors:**
- Verify Deribit Test credentials in `.env`
- Check network connectivity
- Review logs in `./logs/pulseexec.log`

### Testing Issues

**Tests fail to build:**
```bash
# Clean and rebuild
rm -rf build
mkdir build && cd build
cmake .. && make -j$(nproc)
```

## Support

For issues, questions, or contributions, please open an issue on GitHub.

---

**Note**: This is test software for educational purposes. Do not use with real funds or production API keys.
# TradingPlatform

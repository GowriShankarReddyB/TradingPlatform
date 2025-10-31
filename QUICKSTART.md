# PulseExec Quick Start Guide

## Setup (5 minutes)

### 1. Build the Project
```bash
cd /home/gowrishankar/cppPractice/tradingProject
./build.sh
```

### 2. Configure Credentials

The project includes example credentials in `.env.example`. For quick testing, you can use them directly:

```bash
# Copy the example environment file
cp .env.example .env

# The default .env.example already contains test credentials:
# DERIBIT_KEY=yI5PD35K
# DERIBIT_SECRET=EvHuYh9nUqK9VG10-gHF9MWmUS6tZgbPQnHjKeyUYLc
```

**Note**: For production trading or personal testing, get your own credentials:
1. Visit https://test.deribit.com/
2. Create an account
3. Navigate to Account → API
4. Create a new API key with trading permissions
5. Update `.env` with your credentials

### 3. Verify Setup
```bash
# Test the CLI
./run.sh help

# You should see the command reference
```

## CLI Commands

### Interactive Mode (Recommended for Beginners)
```bash
./run.sh interactive
```

This opens a menu-driven interface:
```
╔══════════════════════════════════════════════════════════════╗
║         PulseExec - Interactive Trading Mode                ║
╚══════════════════════════════════════════════════════════════╝

Select an option:
  1. Place Order
  2. Cancel Order
  3. List Orders
  4. Get Order Details
  5. View Orderbook
  6. Exit

Enter choice (1-6):
```

### Command-Line Mode

#### 1. View Market Data
```bash
# Get orderbook for BTC perpetual contract
./run.sh get-orderbook --symbol BTC-PERPETUAL
```

Output example:
```
╔══════════════════════════════════════════════════════════════╗
║  Orderbook: BTC-PERPETUAL                                   ║
╚══════════════════════════════════════════════════════════════╝

  BIDS (Buy Orders)              │  ASKS (Sell Orders)
  Price       │  Amount          │  Price       │  Amount
──────────────┼──────────────────┼──────────────┼──────────────
  45123.50    │  1.234           │  45125.00    │  0.567
  45120.00    │  2.100           │  45127.50    │  1.234

  Best Bid: 45123.50  │  Best Ask: 45125.00
  Spread: 1.50        │  Mid Price: 45124.25
```

#### 2. Place an Order
```bash
# Place a BUY limit order for 10 USD worth of BTC at $50,000
# Note: For BTC-PERPETUAL, amount is in USD (minimum 10)
./run.sh place-order \
  --symbol BTC-PERPETUAL \
  --side BUY \
  --price 50000 \
  --amount 10 \
  --type LIMIT
```

Output:
```
╔══════════════════════════════════════════════════════════════╗
║  Order Placed Successfully                                   ║
╚══════════════════════════════════════════════════════════════╝

  Order ID      : ORDER_1234567890
  Client ID     : <none>
  Symbol        : BTC-PERPETUAL
  Side          : BUY
  Type          : LIMIT
  Price         : 50000.00
  Amount        : 10 (USD)
  Filled        : 0
  Status        : open
  Exchange ID   : 123456
  Created       : 2024-01-15 14:30:45
```

**Important**: For perpetual contracts on Deribit:
- **BTC-PERPETUAL**: Amount is in **USD** (minimum: 10)
- **ETH-PERPETUAL**: Amount is in **USD** (minimum: 10)  
- For spot trading (e.g., BTC-USD), amount would be in BTC

#### 3. List All Orders
```bash
# List all orders
./run.sh list-orders

# List only active orders
./run.sh list-orders --active

# Filter by symbol
./run.sh list-orders --active --symbol BTC-PERPETUAL
```

#### 4. Check Order Status
```bash
# Get details of a specific order
./run.sh get-order --order-id ORDER_1234567890
```

#### 5. Cancel an Order
```bash
./run.sh cancel-order --order-id ORDER_1234567890
```

Output:
```
✓ Order ORDER_1234567890 marked for cancellation
  Status will update when exchange confirms
```

#### 6. Modify an Order
```bash
# Change price and amount
./run.sh modify-order \
  --order-id ORDER_1234567890 \
  --price 51000 \
  --amount 0.002
```

## Common Workflows

### Workflow 1: Simple Buy Order
```bash
# 1. Check current market price
./run.sh get-orderbook --symbol BTC-PERPETUAL

# 2. Place order at specific price (amount in USD for perpetuals)
./run.sh place-order --symbol BTC-PERPETUAL --side BUY --price 45000 --amount 10

# 3. Check if order filled
./run.sh list-orders --active
```

### Workflow 2: Trading Session
```bash
# Start interactive mode for continuous trading
./run.sh interactive

# In the menu:
# - Option 5: View current orderbook
# - Option 1: Place orders
# - Option 3: Monitor active orders
# - Option 4: Check order details
# - Option 2: Cancel orders
```

### Workflow 3: Market Analysis
```bash
# Monitor orderbook for BTC
while true; do
  clear
  ./run.sh get-orderbook --symbol BTC-PERPETUAL
  sleep 5
done
```

## Troubleshooting

### Error: "DERIBIT_KEY and DERIBIT_SECRET must be set"
```bash
# Make sure .env file exists
ls -la .env

# Verify it has credentials
cat .env | grep DERIBIT_KEY

# If missing, copy from example
cp .env.example .env
```

### Error: "pulseexec binary not found"
```bash
# Rebuild the project
./build.sh

# Verify binary exists
ls -lh ./build/src/pulseexec
```

### Error: "Failed to connect to Deribit"
```bash
# Check internet connection
ping test.deribit.com

# Verify API credentials are valid
# Login to https://test.deribit.com/ and check API key status
```

### Order Rejected by Exchange
Common reasons:
- **Insufficient funds**: Deposit test funds at https://test.deribit.com/
- **Price too far from market**: Check current prices with `get-orderbook`
- **Invalid symbol**: Use exact symbol names (e.g., BTC-PERPETUAL, not BTC)
- **Amount too small**: Minimum is **10 USD** for BTC/ETH perpetual contracts
- **Invalid amount**: Must be multiple of contract size (use 10, 20, 30, etc.)

**Quick Fix for "must be a multiple of contract size":**
```bash
# ❌ Wrong: Too small or fractional amount
./run.sh place-order --symbol BTC-PERPETUAL --side BUY --price 50000 --amount 0.001

# ✅ Correct: Use USD amount (minimum 10)
./run.sh place-order --symbol BTC-PERPETUAL --side BUY --price 50000 --amount 10
```

## Testing Without Real Trading

### 1. Run Unit Tests
```bash
cd build/tests
./test_runner
```

### 2. Dry Run Orders (View Only)
```bash
# Just view orderbook without placing orders
./run.sh get-orderbook --symbol BTC-PERPETUAL
./run.sh get-orderbook --symbol ETH-PERPETUAL
./run.sh get-orderbook --symbol SOL-PERPETUAL
```

## Logs and Database

### View Logs
```bash
# Real-time log monitoring
tail -f logs/pulseexec.log

# View recent errors
grep "ERROR" logs/pulseexec.log | tail -20

# View order events
grep "ORDER" logs/pulseexec.log | tail -20
```

### Inspect Database
```bash
# Open SQLite database
sqlite3 pulseexec.db

# View all orders
sqlite> SELECT * FROM orders;

# View recent orders
sqlite> SELECT order_id, symbol, side, price, amount, status 
        FROM orders 
        ORDER BY created_at DESC 
        LIMIT 10;

# Exit
sqlite> .quit
```

## Next Steps

1. **Explore All Symbols**: Try different instruments
   ```bash
   ./run.sh get-orderbook --symbol ETH-PERPETUAL
   ./run.sh get-orderbook --symbol SOL-PERPETUAL
   ```

2. **Test Order Types**: Try MARKET orders
   ```bash
   ./run.sh place-order --symbol BTC-PERPETUAL --side BUY --type MARKET --amount 0.001
   ```

3. **Monitor Performance**: Check logs for latency metrics
   ```bash
   grep "latency_ms" logs/pulseexec.log
   ```

4. **Advanced**: Review Phase 1 implementation details
   ```bash
   cat docs/PHASE1_SUMMARY.md
   ```

## Support

- **Documentation**: See `docs/` folder
- **Specification**: Read `SPEC.md` for technical details
- **API Reference**: Visit https://docs.deribit.com/
- **Issues**: Check build logs and test output

## Safety Reminders

⚠️ **This is TEST environment with TEST funds**
- Real trading uses different API endpoints
- Test funds have no real value
- Test environment may have limitations
- Always test thoroughly before considering production use

---

**Happy Trading! 🚀**

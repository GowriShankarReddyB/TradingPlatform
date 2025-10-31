# PulseExec — High-Performance Order Execution & Management System

**License:** MIT  
**Language:** C++17 / C++20  
**Build:** CMake  
**Author:** B Gowri Shankar Reddy

---

## 1. Project summary (MVP)

PulseExec is a low-latency Order Execution & Management System (OEMS) implemented in modern C++ that integrates with **Deribit Test**.  
**MVP goal:** Produce a correct, concurrent, and maintainable C++ codebase with unit tests, README, and build scripts. Phase 2 will add benchmarks and socket-level tuning.

**MVP deliverables**
- Source code (clean, modular)
- README: build/run/tests instructions + how to configure Deribit Test keys
- Unit tests covering core logic
- Minimal integration flow tested against Deribit Test
- `.clang-format` and basic CI (format + unit tests)

---

## 2. High-level requirements

**Core features (MVP)**  
- Place / Cancel / Modify orders (Spot & Perp initially) via Deribit Test REST API.  
- Query orderbook and positions.  
- MarketDataFeed: WebSocket client to Deribit Test for real-time orderbook updates (top-N, default N=10).  
- WebSocket server: publish updates to subscribed clients (basic subscribe/unsubscribe).  
- OrderManager: manage order lifecycle, idempotency, and persistence.  
- Persistence: SQLite (WAL) for orders, positions, and basic metrics.  
- Basic client-side handling for HTTP 429/5xx: exponential backoff with jitter (no heavy client-side throttling).  
- Async, low-overhead logging (JSON for events/errors; minimal per-tick logging).

**Constraints & scope**
- Use live **Deribit Test** endpoints for integration tests (no local API mocks).
- No production-grade kernel bypass / DPDK in Phase 1.
- Performance benchmarking and heavy socket tuning are Phase 2.

---

## 3. Tech stack (final)

- C++ standard: **C++17**, selectively use **C++20** features if justified.  
- Build: **CMake**.  
- WebSocket (client/server): **Boost.Beast** (Asio).  
- HTTP REST client: **libcurl**.  
- JSON: **nlohmann::json**.  
- Persistence: **SQLite** (WAL mode).  
- Tests: **Catch2** (recommended — single-header, light). If you prefer zero dependency initially, simple assert-based tests are acceptable.  
- Formatting: **clang-format** (LLVM style).  
- CI: **GitHub Actions** — run build, unit tests, and clang-format check.

---

## 4. Architecture (component summary)

```
[ CLI / Test Client ] <--> [ WebSocketServer ] <--> [ OrderManager (OMS) ] <--> [ExecutionGateway (libcurl)]
                                 ^                                |
                                 |                                v
                      [ MarketDataFeed (Deribit WS) ]      [ SQLite (DB Writer) ]
                                 |
                            Broadcasts
                                 |
                          [ Async Logger ]
```

**Primary components**
- `MarketDataFeed` — connects to Deribit WS, parses messages, publishes deltas.
- `OrderManager` — order lifecycle, per-order concurrency, idempotency handling.
- `ExecutionGateway` — synchronous REST calls with backoff and jitter; fresh libcurl handle per call.
- `WebSocketServer` — accepts client connections, manages subscriptions, broadcasts updates.
- `Persistence/DBWriter` — single writer thread that serializes writes to SQLite.
- `Logger` — async logging with bounded queue and background writer.

---

## 5. Concurrency model (MVP)

**Core principles**
- Separate responsibilities across threads/contexts so blocking in one area does not stall others.  
- Fine-grained locking: per-order mutex; per-orderbook mutex or atomic snapshot swap.  
- Bounded queues between stages for backpressure.  
- Single writer thread for SQLite to avoid `SQLITE_BUSY` contention.

**Thread layout (suggested MVP)**
- `io_context_net` (1–2 threads): Boost.Asio for WS client & server I/O.
- `dispatcher_thread` (1): consumes broadcast queue and enqueues messages to sessions.
- `worker_pool` (N threads, default 4): performs ExecutionGateway calls and heavier OrderManager tasks.
- `db_writer_thread` (1): serializes DB writes.
- `logger_thread` (1): drains log queue and writes logs.

---

## 6. Data models (examples)

**OrderRequest**
```cpp
struct OrderRequest {
  std::string symbol;
  enum Side { BUY, SELL } side;
  double price;
  double amount;
  enum Type { LIMIT, MARKET } type;
  std::string client_order_id; // optional
};
```

**Order**

```cpp
struct Order {
  std::string client_order_id;
  std::string exchange_order_id; // from Deribit
  OrderRequest request;
  enum State { PENDING, OPEN, PARTIAL, FILLED, CANCELED, REJECTED } state;
  int64_t created_ts_us;
  int64_t last_update_ts_us;
};
```

**OrderBook**: symbol + top-N bids/asks + timestamp (snapshot or delta model).

---

## 7. Persistence (SQLite)

**Strategy**

* Use SQLite with WAL (`PRAGMA journal_mode=WAL`).
* Single `db_writer_thread` consumes `db_write_queue` entries and performs transactions.
* Tables: `orders`, `positions`, `latency_metrics` (schema described in repo).
* Read queries can be done directly (reads are concurrent-safe); writes happen through `db_writer_thread`.

---

## 8. ExecutionGateway: retries and backoff

* Use libcurl synchronous calls from worker threads (fresh `CURL*` per call).
* Implement exponential backoff + jitter for responses indicating transient errors (HTTP 429 and 5xx).
* Configurable retries (default 3 attempts).
* Do not rely on server limits — handle transient responses politely.

---

## 9. Logging

* Asynchronous logger: producers push `LogMessage` into bounded queue; background thread writes messages.
* Use compact **JSON** for errors and important events. Avoid logging every market tick.
* Default policy: non-blocking enqueue, drop when full (configurable), with a fallback to sample or metric to record drops.

---

## 10. Testing & CI

**Unit tests**

* Use Catch2 for unit tests (recommended). Cover the Order model, OrderManager logic, MarketData parser, and DB writer concurrency behavior.

**Integration tests**

* Run live against **Deribit Test** using environment-provided keys. Integration tests should be optional in CI unless keys/config are present.

**CI**

* GitHub Actions: `build` + `unit tests` + `clang-format` check. Exclude integration and benchmark jobs.

**No local mock servers** in MVP (decided). Unit tests isolate logic; integration tests use Deribit Test.

---

## 11. Formatting & tooling

* Add `.clang-format` (LLVM style) in repo root.
* Developers should run `clang-format -i` before commit. CI will validate formatting on PRs.
* Best practices:

  * Agree on one style and apply across repo.
  * Run automatic formatting in pre-commit or CI to avoid style discussions.

---

## 12. Phase plan

**Phase 1 — MVP (weekend / short sprint)**

* Implement: `Order`, `OrderManager`, `ExecutionGateway`, `MarketDataFeed` parser, `WebSocketServer` minimal, `SQLite db_writer`, `Async Logger`.
* Unit tests (Catch2) for core logic.
* README + build scripts + `.clang-format`.
* Integration: sanity test against Deribit Test for place/place-cancel flows.

**Phase 2 — Performance**

* Add bench runner, p50/p90/p99 collection, socket tuning (`TCP_NODELAY`, `SO_REUSEPORT`), advanced thread-pool + lock-free queues, profiling.

---

## 13. Environment & setup notes

* Target platform: WSL Ubuntu (your dev machine).
* Compiler: g++ >= 10 or clang++ >= 13.
* Dev packages: `libboost-all-dev`, `libcurl4-openssl-dev`, `libsqlite3-dev`.
* Env vars: `DERIBIT_KEY`, `DERIBIT_SECRET`. Do not commit secrets. Provide `.env.example`.

---

## 14. Links & references

* Deribit API docs: [https://docs.deribit.com/](https://docs.deribit.com/)
* Boost.Beast docs: [https://www.boost.org/doc/libs/release/libs/beast/](https://www.boost.org/doc/libs/release/libs/beast/)
* SQLite WAL: [https://sqlite.org/wal.html](https://sqlite.org/wal.html)

---

## 15. Next immediate actions (start coding)

1. Create repo skeleton: `src/`, `include/`, `tests/`, `bench/`, `docs/`.
2. Add `CMakeLists.txt` and `.clang-format`.
3. Implement `Order` + `OrderManager` headers and unit tests (TDD-first).
4. Implement `ExecutionGateway` (libcurl) + basic worker pool.
5. Implement `MarketDataFeed` parser and a minimal `WebSocketServer`.
6. Wire SQLite db writer and async logger.
7. Add README with build/test instructions and Deribit Test setup.

---

### Notes / assumptions (explicit)

* Integration tests will hit Deribit Test endpoints — no local mock servers.
* One Deribit Test account is sufficient for MVP.
* Benchmarks are Phase 2.
* Use compact JSON for important logs; avoid verbose per-tick logging.

---

**End of SPEC**

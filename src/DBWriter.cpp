#include "pulseexec/DBWriter.hpp"
#include "pulseexec/Logger.hpp"
#include <sqlite3.h>
#include <sstream>

namespace pulseexec {

DBWriter::DBWriter(const std::string& db_path, std::shared_ptr<Logger> logger,
                   size_t queue_capacity)
    : db_path_(db_path), db_(nullptr), logger_(logger), queue_capacity_(queue_capacity) {}

DBWriter::~DBWriter() {
  stop();
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void DBWriter::start() {
  if (running_.exchange(true)) {
    return; // Already running
  }

  if (!init_database()) {
    if (logger_) {
      logger_->log_error("DBWriter", "Failed to initialize database");
    }
    running_ = false;
    return;
  }

  worker_ = std::thread(&DBWriter::worker_thread, this);
}

void DBWriter::stop() {
  if (!running_.exchange(false)) {
    return; // Already stopped
  }

  queue_cv_.notify_one();

  if (worker_.joinable()) {
    worker_.join();
  }
}

bool DBWriter::write_order(const Order& order) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (write_queue_.size() >= queue_capacity_) {
      dropped_count_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    write_queue_.emplace(order);
  }

  queue_cv_.notify_one();
  return true;
}

void DBWriter::worker_thread() {
  while (running_.load(std::memory_order_relaxed)) {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    queue_cv_.wait(lock, [this] {
      return !write_queue_.empty() || !running_.load(std::memory_order_relaxed);
    });

    while (!write_queue_.empty()) {
      DBWriteRequest req = std::move(write_queue_.front());
      write_queue_.pop();

      lock.unlock();

      // Execute write
      if (req.type == DBWriteRequest::ORDER) {
        execute_order_write(req.order);
      }

      lock.lock();
    }
  }

  // Drain remaining writes
  std::lock_guard<std::mutex> lock(queue_mutex_);
  while (!write_queue_.empty()) {
    DBWriteRequest req = std::move(write_queue_.front());
    write_queue_.pop();

    if (req.type == DBWriteRequest::ORDER) {
      execute_order_write(req.order);
    }
  }
}

bool DBWriter::init_database() {
  int rc = sqlite3_open(db_path_.c_str(), &db_);
  if (rc != SQLITE_OK) {
    if (logger_) {
      logger_->log_error("DBWriter", "Failed to open database: " + std::string(sqlite3_errmsg(db_)));
    }
    return false;
  }

  // Enable WAL mode
  char* err_msg = nullptr;
  rc = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    if (logger_) {
      logger_->log_error("DBWriter", "Failed to enable WAL: " + std::string(err_msg));
    }
    sqlite3_free(err_msg);
    return false;
  }

  // Create tables
  return create_tables();
}

bool DBWriter::create_tables() {
  const char* orders_table_sql = R"(
    CREATE TABLE IF NOT EXISTS orders (
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
  )";

  const char* positions_table_sql = R"(
    CREATE TABLE IF NOT EXISTS positions (
      symbol TEXT PRIMARY KEY,
      amount REAL NOT NULL,
      avg_price REAL NOT NULL,
      unrealized_pnl REAL DEFAULT 0.0,
      last_update_ts_us INTEGER NOT NULL
    );
  )";

  const char* metrics_table_sql = R"(
    CREATE TABLE IF NOT EXISTS latency_metrics (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      operation TEXT NOT NULL,
      latency_us INTEGER NOT NULL,
      timestamp_us INTEGER NOT NULL
    );
  )";

  char* err_msg = nullptr;

  // Create orders table
  int rc = sqlite3_exec(db_, orders_table_sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    if (logger_) {
      logger_->log_error("DBWriter", "Failed to create orders table: " + std::string(err_msg));
    }
    sqlite3_free(err_msg);
    return false;
  }

  // Create positions table
  rc = sqlite3_exec(db_, positions_table_sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    if (logger_) {
      logger_->log_error("DBWriter", "Failed to create positions table: " + std::string(err_msg));
    }
    sqlite3_free(err_msg);
    return false;
  }

  // Create metrics table
  rc = sqlite3_exec(db_, metrics_table_sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    if (logger_) {
      logger_->log_error("DBWriter", "Failed to create metrics table: " + std::string(err_msg));
    }
    sqlite3_free(err_msg);
    return false;
  }

  return true;
}

bool DBWriter::execute_order_write(const Order& order) {
  const char* sql = R"(
    INSERT OR REPLACE INTO orders 
    (client_order_id, exchange_order_id, symbol, side, price, amount, order_type, 
     state, filled_amount, created_ts_us, last_update_ts_us, error_message)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
  )";

  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    if (logger_) {
      logger_->log_error("DBWriter", "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    return false;
  }

  // Bind parameters
  sqlite3_bind_text(stmt, 1, order.client_order_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, order.exchange_order_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, order.request.symbol.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, to_string(order.request.side).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_double(stmt, 5, order.request.price);
  sqlite3_bind_double(stmt, 6, order.request.amount);
  sqlite3_bind_text(stmt, 7, to_string(order.request.type).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, to_string(order.state).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_double(stmt, 9, order.filled_amount);
  sqlite3_bind_int64(stmt, 10, order.created_ts_us);
  sqlite3_bind_int64(stmt, 11, order.last_update_ts_us);
  sqlite3_bind_text(stmt, 12, order.error_message.c_str(), -1, SQLITE_TRANSIENT);

  // Execute
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    if (logger_) {
      logger_->log_error("DBWriter", "Failed to execute order write: " + std::string(sqlite3_errmsg(db_)));
    }
    return false;
  }

  return true;
}

} // namespace pulseexec

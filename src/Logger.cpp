#include "pulseexec/Logger.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace pulseexec {

Logger::Logger(const std::string& log_file, size_t queue_capacity)
    : log_file_(log_file), queue_capacity_(queue_capacity), min_level_(LogLevel::INFO) {
  if (!log_file_.empty()) {
    log_stream_.open(log_file_, std::ios::app);
    if (!log_stream_.is_open()) {
      std::cerr << "Failed to open log file: " << log_file_ << std::endl;
    }
  }
}

Logger::~Logger() {
  stop();
  if (log_stream_.is_open()) {
    log_stream_.close();
  }
}

void Logger::start() {
  if (running_.exchange(true)) {
    return; // Already running
  }
  worker_ = std::thread(&Logger::worker_thread, this);
}

void Logger::stop() {
  if (!running_.exchange(false)) {
    return; // Already stopped
  }

  queue_cv_.notify_one();

  if (worker_.joinable()) {
    worker_.join();
  }
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
  if (level < min_level_) {
    return; // Below minimum level
  }

  auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();

  LogMessage msg(level, component, message, now_us);

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (message_queue_.size() >= queue_capacity_) {
      // Queue full - drop message
      dropped_count_.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    message_queue_.push(std::move(msg));
  }

  queue_cv_.notify_one();
}

void Logger::log_debug(const std::string& component, const std::string& message) {
  log(LogLevel::DEBUG, component, message);
}

void Logger::log_info(const std::string& component, const std::string& message) {
  log(LogLevel::INFO, component, message);
}

void Logger::log_warning(const std::string& component, const std::string& message) {
  log(LogLevel::WARNING, component, message);
}

void Logger::log_error(const std::string& component, const std::string& message) {
  log(LogLevel::ERROR, component, message);
}

void Logger::set_min_level(LogLevel level) { min_level_ = level; }

void Logger::worker_thread() {
  while (running_.load(std::memory_order_relaxed)) {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    queue_cv_.wait(lock, [this] {
      return !message_queue_.empty() || !running_.load(std::memory_order_relaxed);
    });

    while (!message_queue_.empty()) {
      LogMessage msg = std::move(message_queue_.front());
      message_queue_.pop();

      lock.unlock();

      // Format and write message
      std::string formatted = format_message(msg);

      if (log_stream_.is_open()) {
        log_stream_ << formatted << std::endl;
        log_stream_.flush();
      } else {
        std::cout << formatted << std::endl;
      }

      lock.lock();
    }
  }

  // Drain remaining messages
  std::lock_guard<std::mutex> lock(queue_mutex_);
  while (!message_queue_.empty()) {
    LogMessage msg = std::move(message_queue_.front());
    message_queue_.pop();

    std::string formatted = format_message(msg);
    if (log_stream_.is_open()) {
      log_stream_ << formatted << std::endl;
    } else {
      std::cout << formatted << std::endl;
    }
  }
}

std::string Logger::format_message(const LogMessage& msg) const {
  // Use compact JSON format
  json j;
  j["timestamp"] = msg.timestamp_us;
  j["level"] = level_to_string(msg.level);
  j["component"] = msg.component;
  j["message"] = msg.message;

  return j.dump();
}

std::string Logger::level_to_string(LogLevel level) const {
  switch (level) {
  case LogLevel::DEBUG:
    return "DEBUG";
  case LogLevel::INFO:
    return "INFO";
  case LogLevel::WARNING:
    return "WARNING";
  case LogLevel::ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

} // namespace pulseexec

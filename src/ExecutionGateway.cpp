#include "pulseexec/ExecutionGateway.hpp"
#include "pulseexec/Logger.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <thread>

using json = nlohmann::json;

namespace pulseexec {

// CURL write callback
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  ((std::string*)userp)->append((char*)contents, size * nmemb);
  return size * nmemb;
}

ExecutionGateway::ExecutionGateway(const std::string& api_key, const std::string& api_secret,
                                   const std::string& base_url, std::shared_ptr<Logger> logger)
    : api_key_(api_key), api_secret_(api_secret), base_url_(base_url), logger_(logger),
      max_retries_(3), base_backoff_ms_(100) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

ExecutionGateway::~ExecutionGateway() { curl_global_cleanup(); }

ExecutionResult ExecutionGateway::place_order(const OrderRequest& request) {
  ExecutionResult result;

  // Build JSON-RPC 2.0 request body for Deribit
  json params;
  params["instrument_name"] = request.symbol;
  params["amount"] = request.amount;
  params["type"] = to_string(request.type);

  if (request.type == OrderType::LIMIT) {
    params["price"] = request.price;
  }

  if (!request.client_order_id.empty()) {
    params["label"] = request.client_order_id;
  }

  // Deribit uses JSON-RPC 2.0 format
  std::string method = request.side == Side::BUY ? "private/buy" : "private/sell";
  std::string body = build_jsonrpc_request(method, params);

  std::string endpoint = std::string("/api/v2/private/") + (request.side == Side::BUY ? "buy" : "sell");
  Response resp = execute_with_retry(endpoint, "POST", body);

  result.http_status = resp.http_status;
  result.success = resp.success;

  if (resp.success) {
    try {
      json response = json::parse(resp.body);
      if (response.contains("result") && response["result"].contains("order")) {
        json order = response["result"]["order"];
        result.exchange_order_id = order.value("order_id", "");
        result.success = true;
      } else {
        result.success = false;
        result.error_message = "Invalid response format";
      }
    } catch (const std::exception& e) {
      result.success = false;
      result.error_message = std::string("JSON parse error: ") + e.what();
    }
  } else {
    result.error_message = resp.body;
  }

  return result;
}

ExecutionResult ExecutionGateway::cancel_order(const std::string& exchange_order_id) {
  ExecutionResult result;

  json j;
  j["order_id"] = exchange_order_id;

  std::string endpoint = "/api/v2/private/cancel";
  std::string body = j.dump();

  Response resp = execute_with_retry(endpoint, "POST", body);

  result.http_status = resp.http_status;
  result.success = resp.success;

  if (!resp.success) {
    result.error_message = resp.body;
  }

  return result;
}

ExecutionResult ExecutionGateway::modify_order(const std::string& exchange_order_id,
                                                double new_price, double new_amount) {
  ExecutionResult result;

  json j;
  j["order_id"] = exchange_order_id;
  j["amount"] = new_amount;
  j["price"] = new_price;

  std::string endpoint = "/api/v2/private/edit";
  std::string body = j.dump();

  Response resp = execute_with_retry(endpoint, "POST", body);

  result.http_status = resp.http_status;
  result.success = resp.success;

  if (!resp.success) {
    result.error_message = resp.body;
  }

  return result;
}

ExecutionResult ExecutionGateway::get_order_status(const std::string& exchange_order_id,
                                                    Order& out_order) {
  ExecutionResult result;

  std::string endpoint = "/api/v2/private/get_order_state?order_id=" + exchange_order_id;

  Response resp = execute_with_retry(endpoint, "GET");

  result.http_status = resp.http_status;
  result.success = resp.success;

  if (resp.success) {
    try {
      json response = json::parse(resp.body);
      if (response.contains("result")) {
        json order_data = response["result"];
        // Parse order data and populate out_order
        // This is simplified - add full parsing logic
        result.success = true;
      }
    } catch (const std::exception& e) {
      result.success = false;
      result.error_message = std::string("JSON parse error: ") + e.what();
    }
  } else {
    result.error_message = resp.body;
  }

  return result;
}

ExecutionResult ExecutionGateway::get_orderbook(const std::string& symbol,
                                                 OrderBook& out_orderbook) {
  ExecutionResult result;

  std::string endpoint = "/api/v2/public/get_order_book?instrument_name=" + symbol + "&depth=10";

  Response resp = execute_with_retry(endpoint, "GET");

  result.http_status = resp.http_status;
  result.success = resp.success;

  if (resp.success) {
    try {
      json response = json::parse(resp.body);
      if (response.contains("result")) {
        json book = response["result"];
        
        out_orderbook.symbol = symbol;
        out_orderbook.bids.clear();
        out_orderbook.asks.clear();

        if (book.contains("bids")) {
          for (const auto& bid : book["bids"]) {
            double price = bid[0].get<double>();
            double amount = bid[1].get<double>();
            out_orderbook.bids.emplace_back(price, amount);
          }
        }

        if (book.contains("asks")) {
          for (const auto& ask : book["asks"]) {
            double price = ask[0].get<double>();
            double amount = ask[1].get<double>();
            out_orderbook.asks.emplace_back(price, amount);
          }
        }

        out_orderbook.timestamp_us = book.value("timestamp", 0L);
        result.success = true;
      }
    } catch (const std::exception& e) {
      result.success = false;
      result.error_message = std::string("JSON parse error: ") + e.what();
    }
  } else {
    result.error_message = resp.body;
  }

  return result;
}

ExecutionGateway::Response ExecutionGateway::http_post(const std::string& endpoint,
                                                        const std::string& json_body) {
  Response response;
  CURL* curl = curl_easy_init();

  if (!curl) {
    response.success = false;
    response.http_status = 0;
    response.body = "Failed to initialize CURL";
    return response;
  }

  std::string url = base_url_ + endpoint;
  std::string response_body;

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  
  // For private endpoints, add access token
  if (endpoint.find("/private/") != std::string::npos) {
    std::string token = get_access_token();
    if (!token.empty()) {
      std::string auth_header = "Authorization: Bearer " + token;
      headers = curl_slist_append(headers, auth_header.c_str());
    }
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

  CURLcode res = curl_easy_perform(curl);

  if (res == CURLE_OK) {
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    response.http_status = static_cast<int>(http_code);
    response.body = response_body;
    response.success = (http_code >= 200 && http_code < 300);
  } else {
    response.success = false;
    response.http_status = 0;
    response.body = curl_easy_strerror(res);
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return response;
}

ExecutionGateway::Response ExecutionGateway::http_get(const std::string& endpoint) {
  Response response;
  CURL* curl = curl_easy_init();

  if (!curl) {
    response.success = false;
    response.http_status = 0;
    response.body = "Failed to initialize CURL";
    return response;
  }

  std::string url = base_url_ + endpoint;
  std::string response_body;

  struct curl_slist* headers = nullptr;
  std::string auth_header = get_auth_header();
  if (!auth_header.empty()) {
    headers = curl_slist_append(headers, auth_header.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

  CURLcode res = curl_easy_perform(curl);

  if (res == CURLE_OK) {
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    response.http_status = static_cast<int>(http_code);
    response.body = response_body;
    response.success = (http_code >= 200 && http_code < 300);
  } else {
    response.success = false;
    response.http_status = 0;
    response.body = curl_easy_strerror(res);
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return response;
}

ExecutionGateway::Response ExecutionGateway::execute_with_retry(const std::string& endpoint,
                                                                  const std::string& method,
                                                                  const std::string& json_body) {
  Response response;

  for (int attempt = 0; attempt <= max_retries_; ++attempt) {
    if (method == "POST") {
      response = http_post(endpoint, json_body);
    } else if (method == "GET") {
      response = http_get(endpoint);
    }

    // Success
    if (response.success) {
      return response;
    }

    // Check if we should retry (429 or 5xx)
    bool should_retry =
        (response.http_status == 429 || response.http_status >= 500) && attempt < max_retries_;

    if (!should_retry) {
      break;
    }

    // Calculate backoff and sleep
    int backoff_ms = calculate_backoff_ms(attempt);
    if (logger_) {
      logger_->log_warning("ExecutionGateway",
                           "Retrying after " + std::to_string(backoff_ms) + "ms (attempt " +
                               std::to_string(attempt + 1) + "/" + std::to_string(max_retries_) +
                               ")");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
  }

  return response;
}

int ExecutionGateway::calculate_backoff_ms(int attempt) const {
  // Exponential backoff with jitter
  int base = base_backoff_ms_ * (1 << attempt); // 2^attempt
  
  // Add jitter (±25%)
  static thread_local std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(-base / 4, base / 4);
  
  return base + dist(rng);
}

std::string ExecutionGateway::get_auth_header() const {
  // Deribit uses client credentials, not headers for auth
  // Authentication is done via /public/auth endpoint
  return "";
}

std::string ExecutionGateway::build_jsonrpc_request(const std::string& method, const json& params) const {
  json request;
  request["jsonrpc"] = "2.0";
  request["id"] = 1;
  request["method"] = method;
  request["params"] = params;
  return request.dump();
}

std::string ExecutionGateway::get_access_token() {
  // Check if we have a valid cached token
  auto now = std::chrono::steady_clock::now();
  if (!access_token_.empty() && now < token_expiry_) {
    return access_token_;
  }

  // Authenticate and get new access token
  json params;
  params["grant_type"] = "client_credentials";
  params["client_id"] = api_key_;
  params["client_secret"] = api_secret_;

  std::string body = build_jsonrpc_request("public/auth", params);
  
  Response resp = http_post("/api/v2/public/auth", body);
  
  if (resp.success) {
    try {
      json response = json::parse(resp.body);
      if (response.contains("result")) {
        access_token_ = response["result"]["access_token"].get<std::string>();
        int expires_in = response["result"]["expires_in"].get<int>();
        
        // Set expiry to 90% of actual expiry for safety margin
        token_expiry_ = now + std::chrono::seconds(expires_in * 9 / 10);
        
        if (logger_) {
          logger_->log_info("ExecutionGateway", "Successfully authenticated with Deribit");
        }
        return access_token_;
      }
    } catch (const std::exception& e) {
      if (logger_) {
        logger_->log_error("ExecutionGateway", 
                          std::string("Auth failed: ") + e.what());
      }
    }
  }
  
  return "";
}

} // namespace pulseexec

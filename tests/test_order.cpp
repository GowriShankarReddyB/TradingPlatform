#include <catch2/catch_test_macros.hpp>
#include "pulseexec/Order.hpp"
#include "pulseexec/OrderRequest.hpp"

using namespace pulseexec;

TEST_CASE("OrderRequest construction", "[order][request]") {
  SECTION("Default construction") {
    OrderRequest req;
    REQUIRE(req.symbol.empty());
    REQUIRE(req.side == Side::BUY);
    REQUIRE(req.price == 0.0);
    REQUIRE(req.amount == 0.0);
    REQUIRE(req.type == OrderType::LIMIT);
  }

  SECTION("Parameterized construction") {
    OrderRequest req("BTC-PERPETUAL", Side::SELL, 50000.0, 1.5, OrderType::LIMIT, "test_123");
    REQUIRE(req.symbol == "BTC-PERPETUAL");
    REQUIRE(req.side == Side::SELL);
    REQUIRE(req.price == 50000.0);
    REQUIRE(req.amount == 1.5);
    REQUIRE(req.type == OrderType::LIMIT);
    REQUIRE(req.client_order_id == "test_123");
  }
}

TEST_CASE("Side enum conversions", "[order][enums]") {
  SECTION("to_string") {
    REQUIRE(to_string(Side::BUY) == "buy");
    REQUIRE(to_string(Side::SELL) == "sell");
  }

  SECTION("parse_side") {
    REQUIRE(parse_side("buy") == Side::BUY);
    REQUIRE(parse_side("BUY") == Side::BUY);
    REQUIRE(parse_side("sell") == Side::SELL);
    REQUIRE(parse_side("SELL") == Side::SELL);
  }
}

TEST_CASE("OrderType enum conversions", "[order][enums]") {
  SECTION("to_string") {
    REQUIRE(to_string(OrderType::LIMIT) == "limit");
    REQUIRE(to_string(OrderType::MARKET) == "market");
  }

  SECTION("parse_order_type") {
    REQUIRE(parse_order_type("limit") == OrderType::LIMIT);
    REQUIRE(parse_order_type("LIMIT") == OrderType::LIMIT);
    REQUIRE(parse_order_type("market") == OrderType::MARKET);
    REQUIRE(parse_order_type("MARKET") == OrderType::MARKET);
  }
}

TEST_CASE("Order construction and state", "[order]") {
  SECTION("Default construction") {
    Order order;
    REQUIRE(order.client_order_id.empty());
    REQUIRE(order.exchange_order_id.empty());
    REQUIRE(order.state == OrderState::PENDING);
    REQUIRE(order.filled_amount == 0.0);
    REQUIRE(order.created_ts_us == 0);
    REQUIRE(order.last_update_ts_us == 0);
  }

  SECTION("Parameterized construction") {
    OrderRequest req("BTC-PERPETUAL", Side::BUY, 50000.0, 1.0, OrderType::LIMIT);
    Order order("client_123", req, 1000000);

    REQUIRE(order.client_order_id == "client_123");
    REQUIRE(order.state == OrderState::PENDING);
    REQUIRE(order.filled_amount == 0.0);
    REQUIRE(order.created_ts_us == 1000000);
    REQUIRE(order.last_update_ts_us == 1000000);
  }

  SECTION("Terminal state checks") {
    Order order;
    
    order.state = OrderState::PENDING;
    REQUIRE_FALSE(order.is_terminal());
    
    order.state = OrderState::OPEN;
    REQUIRE_FALSE(order.is_terminal());
    
    order.state = OrderState::PARTIAL;
    REQUIRE_FALSE(order.is_terminal());
    
    order.state = OrderState::FILLED;
    REQUIRE(order.is_terminal());
    
    order.state = OrderState::CANCELED;
    REQUIRE(order.is_terminal());
    
    order.state = OrderState::REJECTED;
    REQUIRE(order.is_terminal());
  }

  SECTION("Active state checks") {
    Order order;
    
    order.state = OrderState::PENDING;
    REQUIRE_FALSE(order.is_active());
    
    order.state = OrderState::OPEN;
    REQUIRE(order.is_active());
    
    order.state = OrderState::PARTIAL;
    REQUIRE(order.is_active());
    
    order.state = OrderState::FILLED;
    REQUIRE_FALSE(order.is_active());
    
    order.state = OrderState::CANCELED;
    REQUIRE_FALSE(order.is_active());
  }
}

TEST_CASE("OrderState enum conversions", "[order][enums]") {
  SECTION("to_string") {
    REQUIRE(to_string(OrderState::PENDING) == "pending");
    REQUIRE(to_string(OrderState::OPEN) == "open");
    REQUIRE(to_string(OrderState::PARTIAL) == "partial");
    REQUIRE(to_string(OrderState::FILLED) == "filled");
    REQUIRE(to_string(OrderState::CANCELED) == "canceled");
    REQUIRE(to_string(OrderState::REJECTED) == "rejected");
  }

  SECTION("parse_order_state") {
    REQUIRE(parse_order_state("pending") == OrderState::PENDING);
    REQUIRE(parse_order_state("open") == OrderState::OPEN);
    REQUIRE(parse_order_state("partial") == OrderState::PARTIAL);
    REQUIRE(parse_order_state("filled") == OrderState::FILLED);
    REQUIRE(parse_order_state("canceled") == OrderState::CANCELED);
    REQUIRE(parse_order_state("cancelled") == OrderState::CANCELED);
    REQUIRE(parse_order_state("rejected") == OrderState::REJECTED);
  }
}

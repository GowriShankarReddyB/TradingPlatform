# Source files for PulseExec library
set(PULSEEXEC_SOURCES
    OrderManager.cpp
    ExecutionGateway.cpp
    MarketDataFeed.cpp
    WebSocketServer.cpp
    DBWriter.cpp
    Logger.cpp
)

# Create library
add_library(pulseexec_lib STATIC ${PULSEEXEC_SOURCES})

target_link_libraries(pulseexec_lib
    PUBLIC
    Threads::Threads
    Boost::system
    ${CURL_LIBRARIES}
    SQLite::SQLite3
    nlohmann_json::nlohmann_json
)

# Main executable
add_executable(pulseexec main.cpp)
target_link_libraries(pulseexec pulseexec_lib)

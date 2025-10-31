# Test executable
add_executable(test_runner
    test_main.cpp
    test_order.cpp
    test_order_manager.cpp
)

target_link_libraries(test_runner
    PRIVATE
    pulseexec_lib
    Catch2::Catch2WithMain
)

# Register tests with CTest
include(CTest)
include(Catch)
catch_discover_tests(test_runner)

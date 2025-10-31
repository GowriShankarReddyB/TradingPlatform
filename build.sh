#!/bin/bash
# Quick build and test script for PulseExec

set -e  # Exit on error

echo "======================================"
echo "PulseExec - Build & Test Script"
echo "======================================"
echo ""

# Check dependencies
echo "Checking dependencies..."
command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake not found. Install with: sudo apt-get install cmake"; exit 1; }
command -v g++ >/dev/null 2>&1 || { echo "ERROR: g++ not found. Install with: sudo apt-get install build-essential"; exit 1; }

# Check for required libraries
echo "Checking required libraries..."
ldconfig -p | grep -q libcurl || { echo "WARNING: libcurl not found. Install with: sudo apt-get install libcurl4-openssl-dev"; }
ldconfig -p | grep -q libsqlite3 || { echo "WARNING: libsqlite3 not found. Install with: sudo apt-get install libsqlite3-dev"; }

# Create logs directory
mkdir -p logs

# Clean previous build
if [ -d "build" ]; then
    echo "Cleaning previous build..."
    rm -rf build
fi

# Create build directory
echo "Creating build directory..."
mkdir build
cd build

# Configure
echo ""
echo "Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
echo ""
echo "Building..."
make -j$(nproc)

# Run tests
echo ""
echo "Running tests..."
if [ -f "./test_runner" ]; then
    ./test_runner --success
    echo ""
    echo "✓ All tests passed!"
else
    echo "WARNING: Test runner not found"
fi

# Summary
echo ""
echo "======================================"
echo "Build complete!"
echo "======================================"
echo ""
echo "To run PulseExec:"
echo "  1. Configure .env file with your Deribit Test credentials"
echo "  2. source ../.env"
echo "  3. ./pulseexec"
echo ""
echo "To run tests:"
echo "  ./test_runner"
echo ""

#!/bin/bash
# Load .env and run PulseExec CLI

# Check if .env exists
if [ ! -f .env ]; then
    echo "❌ Error: .env file not found!"
    echo "   Create .env from .env.example:"
    echo "   cp .env.example .env"
    echo "   nano .env  # Add your Deribit Test credentials"
    exit 1
fi

# Load environment variables
export $(cat .env | grep -v '^#' | grep -v '^$' | xargs)

# Verify critical variables
if [ -z "$DERIBIT_KEY" ] || [ -z "$DERIBIT_SECRET" ]; then
    echo "❌ Error: DERIBIT_KEY and DERIBIT_SECRET must be set in .env"
    exit 1
fi

# Check if pulseexec binary exists
if [ ! -f ./build/src/pulseexec ]; then
    echo "❌ Error: pulseexec binary not found"
    echo "   Build the project first:"
    echo "   ./build.sh"
    exit 1
fi

# Run pulseexec with all arguments passed through
./build/src/pulseexec "$@"

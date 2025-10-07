#!/bin/bash

# Download and extract TWS API for IBKR Hotkey Trader
# This script downloads the official TWS API from Interactive Brokers

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXTERNAL_DIR="$SCRIPT_DIR/external"
TWS_API_DIR="$EXTERNAL_DIR/twsapi"

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo " IBKR Hotkey Trader - TWS API Setup"
echo "═══════════════════════════════════════════════════════════════"
echo ""

# Check if already installed
if [ -d "$TWS_API_DIR/source/cppclient/client" ]; then
    echo "✓ TWS API already installed at: $TWS_API_DIR"
    echo ""
    read -p "Reinstall? (y/N) " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Setup cancelled."
        exit 0
    fi
    rm -rf "$TWS_API_DIR"
fi

# Create external directory
mkdir -p "$EXTERNAL_DIR"
cd "$EXTERNAL_DIR"

# Download TWS API
echo "Downloading TWS API..."
echo ""

# TWS API download URL
# latest Mac/Unix version
# TWS_API_URL="https://interactivebrokers.github.io/downloads/twsapi_macunix.1040.01.zip"
# stable version
TWS_API_URL="https://interactivebrokers.github.io/downloads/twsapi_macunix.1037.02.zip"

if command -v curl &> /dev/null; then
    curl -L -o twsapi.zip "$TWS_API_URL"
elif command -v wget &> /dev/null; then
    wget -O twsapi.zip "$TWS_API_URL"
else
    echo "Error: Neither curl nor wget found. Please install one of them."
    echo ""
    echo "Or download manually from: https://interactivebrokers.github.io/"
    echo "Extract to: $TWS_API_DIR"
    exit 1
fi

# Extract
echo ""
echo "Extracting TWS API..."
unzip -q twsapi.zip

# The zip extracts to IBJts/source/cppclient/client
# Move IBJts to twsapi for consistency
if [ -d "IBJts" ]; then
    mv IBJts twsapi
fi

# Cleanup
rm twsapi.zip
rm -rf META-INF

# Verify installation
if [ -d "$TWS_API_DIR/source/cppclient/client" ]; then
    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo " ✓ TWS API successfully installed!"
    echo "═══════════════════════════════════════════════════════════════"
    echo ""
    echo "You can now build the project:"
    echo "  mkdir build"
    echo "  cd build"
    echo "  cmake .."
    echo "  make"
    echo ""
else
    echo ""
    echo "✗ Installation failed. Please check the directory structure."
    echo ""
    echo "Please download manually from: https://interactivebrokers.github.io/"
    echo "Extract to: $TWS_API_DIR"
    exit 1
fi

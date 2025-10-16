#!/bin/bash

# Download and extract QCustomPlot for IBKR Hotkey Trader
# This script downloads the official QCustomPlot library

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXTERNAL_DIR="$SCRIPT_DIR/external"
QCUSTOMPLOT_DIR="$EXTERNAL_DIR/qcustomplot"

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo " IBKR Hotkey Trader - QCustomPlot Setup"
echo "═══════════════════════════════════════════════════════════════"
echo ""

# Check if already installed
if [ -f "$QCUSTOMPLOT_DIR/qcustomplot.h" ] && [ -f "$QCUSTOMPLOT_DIR/qcustomplot.cpp" ]; then
    echo "✓ QCustomPlot already installed at: $QCUSTOMPLOT_DIR"
    echo ""
    read -p "Reinstall? (y/N) " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Setup cancelled."
        exit 0
    fi
    rm -rf "$QCUSTOMPLOT_DIR"
fi

# Create external directory
mkdir -p "$QCUSTOMPLOT_DIR"
cd "$EXTERNAL_DIR"

# Download QCustomPlot
echo "Downloading QCustomPlot..."
echo ""

# QCustomPlot download URL (version 2.1.1)
QCUSTOMPLOT_URL="https://www.qcustomplot.com/release/2.1.1/QCustomPlot.tar.gz"

if command -v curl &> /dev/null; then
    curl -L -o qcustomplot.tar.gz "$QCUSTOMPLOT_URL"
elif command -v wget &> /dev/null; then
    wget -O qcustomplot.tar.gz "$QCUSTOMPLOT_URL"
else
    echo "Error: Neither curl nor wget found. Please install one of them."
    echo ""
    echo "Or download manually from: https://www.qcustomplot.com/"
    echo "Extract qcustomplot.h and qcustomplot.cpp to: $QCUSTOMPLOT_DIR"
    exit 1
fi

# Extract
echo ""
echo "Extracting QCustomPlot..."
tar -xzf qcustomplot.tar.gz

# The tar extracts to qcustomplot/ directory
# Move the source files to our target directory
if [ -d "qcustomplot" ]; then
    mv qcustomplot/qcustomplot.h "$QCUSTOMPLOT_DIR/"
    mv qcustomplot/qcustomplot.cpp "$QCUSTOMPLOT_DIR/"
    rm -rf qcustomplot
elif [ -d "qcustomplot-source" ]; then
    # Alternative directory structure
    mv qcustomplot-source/qcustomplot.h "$QCUSTOMPLOT_DIR/"
    mv qcustomplot-source/qcustomplot.cpp "$QCUSTOMPLOT_DIR/"
    rm -rf qcustomplot-source
fi

# Cleanup
rm qcustomplot.tar.gz

# Verify installation
if [ -f "$QCUSTOMPLOT_DIR/qcustomplot.h" ] && [ -f "$QCUSTOMPLOT_DIR/qcustomplot.cpp" ]; then
    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo " ✓ QCustomPlot successfully installed!"
    echo "═══════════════════════════════════════════════════════════════"
    echo ""
    echo "Files installed:"
    echo "  - $QCUSTOMPLOT_DIR/qcustomplot.h"
    echo "  - $QCUSTOMPLOT_DIR/qcustomplot.cpp"
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
    echo "Please download manually from: https://www.qcustomplot.com/"
    echo "Extract qcustomplot.h and qcustomplot.cpp to: $QCUSTOMPLOT_DIR"
    exit 1
fi

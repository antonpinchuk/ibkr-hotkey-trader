# IBKR Hotkey Trader

A fast trading application for Interactive Brokers using keyboard hotkeys for rapid execution via the TWS API.

## Features

- **Rapid Trading**: Execute buy/sell orders instantly using keyboard shortcuts
- **Real-time Charts**: 10-second candlestick charts with live price updates
- **Position Management**: Track open positions and pending orders in real-time
- **Risk Controls**: Budget limits and automatic order management
- **Session Statistics**: Track daily P&L, win rate, and trade history

## Requirements

### System Requirements
- macOS 10.15+ / Windows 10+ / Linux
- Qt 6.2 or later
- CMake 3.16 or later
- Protocol Buffers (protobuf) compiler
- C++17 compatible compiler

**Install dependencies (macOS):**
```bash
brew install cmake qt@6 protobuf
```

**Install dependencies (Ubuntu/Debian):**
```bash
sudo apt install cmake qt6-base-dev protobuf-compiler libprotobuf-dev
```

**Install dependencies (Windows):**
- Install CMake from https://cmake.org/download/
- Install Qt6 from https://www.qt.io/download
- Install vcpkg and run: `vcpkg install protobuf`

### Interactive Brokers Setup
1. **TWS (Trader Workstation)** or **IB Gateway** installed and running
2. Enable API connections in TWS:
   - Go to **File → Global Configuration → API → Settings**
   - Check **Enable ActiveX and Socket Clients**
   - Check **Allow connections from localhost only** (recommended for security)
   - Note the **Socket port** (default: 7496 for TWS, 4001 for IB Gateway)
   - Check **Read-Only API** is **unchecked** to allow trading

## Installation

### 1. Clone the Repository
```bash
git clone https://github.com/kinect-pro/ibkr-hotkey-trader.git
cd ibkr-hotkey-trader
```

### 2. Download TWS API (One-Time Setup)

The app requires the Interactive Brokers TWS API C++ client library (not included in this repository).

**Automatic Setup (Recommended):**
```bash
./download_tws_api.sh
```

This script will:
- Download the latest TWS API (~10MB)
- Extract to `external/twsapi/`
- Verify installation

Takes ~1 minute depending on your connection.

**Manual Setup (if script doesn't work):**
1. Go to https://interactivebrokers.github.io/
2. Download the TWS API (any platform version works)
3. Extract the archive
4. Copy/move the extracted folder to `external/twsapi` in this project
5. Verify: `external/twsapi/source/cppclient/client` directory exists

### 3. Build the Application

#### macOS / Linux
```bash
mkdir build
cd build
cmake ..
make
```

#### Windows
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

### 4. Running the Application

**Important**: The app connects to TWS via localhost socket connection. TWS must be running separately.

1. **Start TWS or IB Gateway** and log in
2. **Ensure API connections are enabled** (see requirements above)
3. **Run the application**:
   - macOS: `./IBKRHotkeyTrader.app/Contents/MacOS/IBKRHotkeyTrader` or `open IBKRHotkeyTrader.app`
   - Linux: `./IBKRHotkeyTrader`
   - Windows: `IBKRHotkeyTrader.exe`

The application is completely autonomous and only communicates with TWS through the socket API (default: localhost:7496).

## Configuration

### First Launch
1. Go to **Settings** (⌘+, or File → Settings)
2. **Connection Tab**: Verify connection settings match your TWS/Gateway
   - Host: `127.0.0.1` (default)
   - Port: `7496` (TWS) or `4001` (IB Gateway)
   - Client ID: `1` (must be unique if multiple apps connect)
3. **Trading Tab**:
   - Select your account from the dropdown
   - Set your daily budget (recommended: start with small amount for testing)
4. Click **OK** to save settings

The application will attempt to connect to TWS automatically.

## Usage

### Keyboard Shortcuts

#### Opening Positions (Buy)
- `Cmd+O` (⌘O): Buy 100% of budget
- `Cmd+P` (⌘P): Buy 50% of budget
- `Cmd+1-9`: Add 5%-45% to position (increments of 5%)
- `Cmd+0`: Add 50% to position

#### Closing Positions (Sell)
- `Cmd+Z`: Sell 100% of position
- `Cmd+X`: Sell 75% of position
- `Cmd+C`: Sell 50% of position
- `Cmd+V`: Sell 25% of position

#### Other Controls
- `Cmd+K`: Open symbol search
- `Esc`: Cancel all pending orders for current ticker
- `Cmd+Q`: Quit application

### Trading Workflow
1. Press `Cmd+K` to search for a symbol (e.g., "AAPL", "TSLA")
2. Use arrow keys to select, press Enter
3. The chart will load with historical and real-time data
4. Press `Cmd+O` or `Cmd+P` to enter a position
5. Monitor the chart and order panel (right side)
6. Use `Cmd+Z/X/C/V` to close position (partial or full)
7. All trades for the day are tracked in the history panel

### Safety Features
- Cannot exceed 100% of budget (warning toast)
- Budget validation against account balance
- All positions must be closed before switching symbols
- Automatic reconnection if TWS connection drops
- Visual warnings for account balance issues

## User Interface

### Main Window Layout
- **Top Toolbar**: Current ticker, quick action buttons, settings
- **Left Panel**: Today's ticker list with live prices and % change
- **Center**: Real-time 10-second candlestick chart
- **Right Panel**: Order history and session statistics

### Statistics Panel
Shows real-time daily performance:
- Total Account Balance
- Total P&L ($ and %)
- Win Rate
- Number of Trades
- Largest Win/Loss

## Troubleshooting

### Cannot Connect to TWS
- Verify TWS/Gateway is running and you're logged in
- Check API settings are enabled (see Requirements)
- Verify port number matches in Settings
- Check firewall isn't blocking localhost connections

### Orders Not Executing
- Ensure "Read-Only API" is disabled in TWS settings
- Check account has sufficient buying power
- Verify market is open (or enable outside RTH in TWS)
- Check error messages in application toast notifications

### Real-time Data Not Updating
- Ensure you have market data subscriptions for the symbols
- Check TWS shows live data for the symbol
- Try restarting the application and TWS

## Development

### Project Structure
```
ibkr-hotkey-trader/
├── src/
│   ├── main.cpp              # Application entry
│   ├── mainwindow.h/cpp      # Main window UI
│   ├── ibkrclient.h/cpp      # TWS API client
│   ├── ibkrwrapper.h/cpp     # TWS API callbacks
│   ├── tradingmanager.h/cpp  # Trading logic
│   ├── chartwidget.h/cpp     # Chart component
│   ├── settingsdialog.h/cpp  # Settings dialog
│   └── ...                   # Other components
├── external/
│   └── tws-api/              # TWS API (download separately)
├── CMakeLists.txt
└── README.md
```

### Building for Development
```bash
# After downloading TWS API (see installation steps above)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

### Project Dependencies
- **TWS API**: Downloaded once to `external/twsapi/` (excluded from git via .gitignore)
  - Use `./download_tws_api.sh` for automatic setup
  - Or download manually from https://interactivebrokers.github.io/
- **Application code**: Self-contained in `src/`
- **User data**: Settings stored in local SQLite database (OS-specific app data folder)

**Note**: The TWS API is not distributed with this repository per Interactive Brokers' distribution policy. It's a one-time ~10MB download.

## License

MIT License - see [LICENSE](LICENSE) file for details

## Links

- Website: https://kinect-pro.com/solutions/ibkr-hotkey-trader/
- GitHub: https://github.com/kinect-pro/ibkr-hotkey-trader
- Support: https://kinect-pro.com/contact/

## Disclaimer

This software is for educational and personal use only. Trading involves substantial risk of loss. The authors and Kinect.PRO are not responsible for any financial losses incurred through the use of this software. Always test with paper trading accounts before using real money.

**Use at your own risk.**

## Author

Developed by **Kinect.PRO**
Website: https://kinect-pro.com

---

**Version**: 0.1
**Last Updated**: 2025

# TradingView IBKR Integration

Chrome Extension for automatic ticker synchronization from TradingView to IBKR Hotkey Trader app.

## Features

- **Auto-sync active ticker**: Symbol switches in TradingView always automatically select in IBKR Hotkey app (append if not yet in app)
- **Automatic cleanup**: Removes inactive tickers from Hotkey app when they no longer match wishlist criteria
  - **Wishlist criteria**: Match tickers from custom and colored (flagged) wishlists
  - **Exchange criteria**: Match tickers by exchange (NASDAQ, NYSE, etc.)

## Settings

Configure via extension popup:

**Base URL**: `http://127.0.0.1:8496` (IBKR Hotkey Trader webhook REST API endpoint)

**Stock Exchanges**: Comma-separated list or empty for all exchanges
Example: `NASDAQ, NYSE`

**Custom Wishlist Names**: Comma-separated wishlist names
Example: `My Stocks, Day Trading`

**Wishlist Colors**: Select flag colors to track (red, blue, green, orange, purple)

### Filtering Logic

Keep ticker is Hotkey app if:
```
(in_custom_wishlist OR has_selected_flag_color) AND (exchanges_empty OR exchange_allowed)
```

## Algorithm

1. **Active ticker**: When you switch symbols in TradingView, the extension always adds it to IBKR app
2. **Wishlist sync**: On wishlist operations (load all, bulk remove, etc.), extension fetches all wishlists from TradingView API
3. **Cleanup**: Compares wishlists with IBKR app tickers and removes those that don't match criteria

## Installation & Run

1. **Enable IBKR Remote Control Server**: Settings → Connection → Enable Remote Control Server (port 8496)
2. **Install extension**:
   - `chrome://extensions/` → Enable Developer mode → Load unpacked → Select `chrome-extension` folder
3. **Configure**: Click extension icon → Set Base URL and filters → Save Settings → Test Connection
4. **Use**: Open TradingView, switch symbols, trade via IBKR global hotkeys

## Debug & Logs

**Background worker**: `chrome://extensions/` → service worker (blue link) → Console
Shows: settings loaded, ticker filtering logic, API calls

**Content script**: TradingView page → F12 → Console → Filter `[IBKR Extension]`
Shows: DOM extraction, fetch interception, ticker data

## Troubleshooting

**Tickers not added**:
- Check IBKR Remote Control Server is enabled
- Verify ticker matches filter criteria (wishlist + exchange)
- Check logs: `[IBKR Extension] Final match result: false`

**Tickers not removed**:
- Removal happens on wishlist API calls (load all, bulk remove)
- Trigger by switching colored wishlists or refreshing TradingView page

**CORS errors**:
- Verify `http://127.0.0.1:8496` (not https, not localhost)
- Check IBKR app is running

**DOM extraction fails**:
- TradingView changed DOM structure
- Update selectors in `content.js` → `extractTickerInfo()`

## Architecture

```
chrome-extension/
├── manifest.json          # Extension manifest (v3)
├── background.js          # Service worker: API interception, filtering logic
├── content.js             # Content script: DOM extraction, fetch interception
├── injected.js            # Injected script: fetch interceptor
├── popup.html/js/css      # Settings UI
├── debug-popup.html       # Debug settings viewer
├── debug-settings.js      # Debug utilities
└── icons/                 # Extension icons
```

**Request interception**: Content script intercepts TradingView API calls (`symbols_list/all`, `symbols_list/colored/*`, `chart-token`) and extracts wishlist data
- GET https://www.tradingview.com/chart-token/ (event to switch symbol)
- GET https://www.tradingview.com/api/v1/symbols_list/all/
- POST https://www.tradingview.com/api/v1/symbols_list/active/[red|blue|green|orange|purple]/
- POST https://www.tradingview.com/api/v1/symbols_list/colored/[red|blue|green|orange|purple]/append/
- POST https://www.tradingview.com/api/v1/symbols_list/colored/bulk_remove/

**DOM extraction**: Reads symbol, exchange, and flag color from TradingView DOM

**API communication**: Background worker calls IBKR REST API:
- `GET /ticker` - list tickers
- `POST /ticker` - add ticker
- `PUT /ticker` - select ticker
- `DELETE /ticker` - remove ticker

## License

MIT License (same as IBKR Hotkey Trader)

## Support

GitHub Issues: https://github.com/kinect-pro/ibkr-hotkey-trader/issues
Website: https://kinect-pro.com/contact/

## Disclaimer

For personal use only. Use at your own risk. Authors not responsible for financial losses.

---

**Version**: 1.0.0

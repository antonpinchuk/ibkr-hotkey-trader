// Background service worker for TradingView IBKR Integration

let settings = {
  webhookUrl: 'http://127.0.0.1:8496',
  exchanges: '',
  customWishlists: '',
  coloredWishlists: [],
  exchangeMapping: [
    { from: 'NYSE Arca', to: 'AMEX' }
  ]
};

// Connection state tracking
let isConnected = true;
let lastConnectionError = null;

// Ticker tracking structures
// tickersMatched: Map<"TW_EXCH:SYMBOL", { symbol, exchange, customWishlists: Set<wishlistName>, coloredWishlist: color|null }>
const tickersMatched = new Map();

// Wishlist indices for fast lookup
const wishlistCustom = new Map(); // Map<wishlistName, { id: number, tickers: Set<"TW_EXCH:SYMBOL"> }>
const wishlistCustomById = new Map(); // Map<wishlistId, wishlistName> - for reverse lookup
const wishlistColored = new Map(); // Map<color, Set<"TW_EXCH:SYMBOL">>

// Exchange conversion functions
function convertExchangeTWtoIBKR(exchange) {
  if (!exchange) return exchange;
  const mapping = settings.exchangeMapping.find(m => m.from === exchange);
  return mapping ? mapping.to : exchange;
}

function convertExchangeIBKRtoTW(exchange) {
  if (!exchange) return exchange;
  const mapping = settings.exchangeMapping.find(m => m.to === exchange);
  return mapping ? mapping.from : exchange;
}

// Delete ticker from app via API
async function deleteTickerFromApp(tickerKey, ticker) {
  const ibkrExchange = convertExchangeTWtoIBKR(ticker.exchange);

  try {
    const deleteResponse = await fetchWithConnectionTracking(`${settings.webhookUrl}/ticker`, {
      method: 'DELETE',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ symbol: ticker.symbol, exchange: ibkrExchange })
    });

    if (deleteResponse.status === 204) {
      return true;
    } else {
      console.log(`[IBKR Extension] Failed to remove ${tickerKey}:`, deleteResponse.status);
      return false;
    }
  } catch (error) {
    // Connection error already logged by fetchWithConnectionTracking
    return false;
  }
}

// Cleanup tickers with zero references in wishlists
async function cleanupTickers() {
  const storage = await chrome.storage.local.get(['activeTicker']);
  const activeTicker = storage.activeTicker;
  const removed = [];

  // Get all tickers from app
  let appTickers = [];
  try {
    const response = await fetchWithConnectionTracking(`${settings.webhookUrl}/ticker`);
    if (response.ok) {
      appTickers = await response.json();
    }
  } catch (error) {
    // Connection error already logged by fetchWithConnectionTracking
    return;
  }

  // Build set of app ticker keys (in TW format for comparison)
  const appTickerKeys = new Set();
  for (const appTicker of appTickers) {
    const twExchange = convertExchangeIBKRtoTW(appTicker.exchange);
    const tickerKey = `${twExchange}:${appTicker.symbol}`;
    appTickerKeys.add(tickerKey);
  }

  // Pass 1: Remove matched tickers that have no wishlist references
  for (const [tickerKey, ticker] of tickersMatched.entries()) {
    // Skip if this is the active ticker
    if (tickerKey === activeTicker) {
      appTickerKeys.delete(tickerKey); // Mark as "should keep"
      continue;
    }

    // Check if ticker has any references in wishlists
    if (ticker.customWishlists.size === 0 && ticker.coloredWishlist === null) {
      // No references - remove from app and from tickersMatched
      const success = await deleteTickerFromApp(tickerKey, ticker);

      if (success) {
        tickersMatched.delete(tickerKey);
        removed.push(tickerKey);
      }
    }

    // Remove from app list (we've processed it)
    appTickerKeys.delete(tickerKey);
  }

  // Pass 2: Remove unmatched tickers (those that are in app but not in tickersMatched)
  // These are tickers that were added via "Select ticker" but not from wishlists
  for (const tickerKey of appTickerKeys) {
    // Skip if this is the active ticker
    if (tickerKey === activeTicker) continue;

    // Find original ticker in appTickers to get IBKR format
    const [twExchange, symbol] = tickerKey.split(':');
    const ibkrExchange = convertExchangeTWtoIBKR(twExchange);

    const success = await deleteTickerFromApp(tickerKey, { symbol, exchange: twExchange });

    if (success) {
      removed.push(tickerKey);
    }
  }

  if (removed.length > 0) {
    console.log('[IBKR Extension] Cleanup removed:', removed.join(', '));
  }
}

// Load settings on startup
chrome.runtime.onStartup.addListener(loadSettings);
chrome.runtime.onInstalled.addListener(loadSettings);

async function loadSettings() {
  try {
    const stored = await chrome.storage.sync.get({
      webhookUrl: 'http://127.0.0.1:8496',
      exchanges: '',
      customWishlists: '',
      coloredWishlists: [],
      exchangeMapping: [
        { from: 'NYSE Arca', to: 'AMEX' }
      ]
    });
    settings = stored;
    console.log('[IBKR Extension] Settings loaded:', settings);
  } catch (error) {
    console.error('[IBKR Extension] Failed to load settings:', error);
  }
}

// Initialize settings immediately on worker start
loadSettings();

// Listen for settings updates
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message.type === 'SETTINGS_UPDATED') {
    loadSettings().then(() => {
      sendResponse({ success: true });
    }).catch(error => {
      console.error('[IBKR Extension] Settings update error:', error);
      sendResponse({ success: false, error: error.message });
    });
    return true; // Keep channel open for async response
  } else if (message.type === 'TICKER_CHANGED') {
    handleTickerChange(message.data).then(() => {
      sendResponse({ success: true });
    }).catch(error => {
      console.log('[IBKR Extension] Ticker change error:', error);
      sendResponse({ success: false, error: error.message });
    });
    return true; // Keep channel open for async response
  } else if (message.type === 'CUSTOM_WISHLISTS') {
    // 2.1: GET /api/v1/symbols_list/custom/
    handleCustomWishlists(message.data).then(() => {
      sendResponse({ success: true });
    }).catch(error => {
      console.log('[IBKR Extension] Custom wishlists error:', error);
      sendResponse({ success: false, error: error.message });
    });
    return true; // Keep channel open for async response
  } else if (message.type === 'CUSTOM_WISHLIST_UPDATE') {
    // 2.1.1, 2.1.2: POST /api/v1/symbols_list/custom/{id}/append/ or remove/
    handleCustomWishlistUpdate(message.wishlistId, message.symbols).then(() => {
      sendResponse({ success: true });
    }).catch(error => {
      console.log('[IBKR Extension] Custom wishlist update error:', error);
      sendResponse({ success: false, error: error.message });
    });
    return true; // Keep channel open for async response
  } else if (message.type === 'COLORED_WISHLIST') {
    // 2.2, 2.3: GET/POST colored wishlists
    handleColoredWishlist(message.color, message.symbols).then(() => {
      sendResponse({ success: true });
    }).catch(error => {
      console.log('[IBKR Extension] Colored wishlist error:', error);
      sendResponse({ success: false, error: error.message });
    });
    return true; // Keep channel open for async response
  } else if (message.type === 'ALL_WISHLISTS') {
    // 2.4: GET /api/v1/symbols_list/all/
    handleAllWishlists(message.data).then(() => {
      sendResponse({ success: true });
    }).catch(error => {
      console.log('[IBKR Extension] All wishlists error:', error);
      sendResponse({ success: false, error: error.message });
    });
    return true; // Keep channel open for async response
  } else if (message.type === 'BULK_REMOVE') {
    // 2.5: POST bulk_remove
    handleBulkRemove(message.symbols).then(() => {
      sendResponse({ success: true });
    }).catch(error => {
      console.log('[IBKR Extension] Bulk remove error:', error);
      sendResponse({ success: false, error: error.message });
    });
    return true; // Keep channel open for async response
  }
});

// Helper to handle fetch with connection state tracking
async function fetchWithConnectionTracking(url, options) {
  try {
    const response = await fetch(url, options);

    // Any response (2xx-5xx) means connection is OK
    if (!isConnected) {
      console.log('[IBKR Extension] Connection restored');
      isConnected = true;
      lastConnectionError = null;
    }

    return response;
  } catch (error) {
    // Connection error (app not running, network error, etc.)
    if (isConnected) {
      // First error - log it
      console.log('[IBKR Extension] Connection error:', error.message);
      isConnected = false;
      lastConnectionError = Date.now();
    }
    // Subsequent errors - suppress (don't log)
    throw error;
  }
}

// Handle ticker change from content script
async function handleTickerChange(tickerData) {
  const { symbol, exchange } = tickerData;

  // Read current active ticker (this is the previous one)
  const storage = await chrome.storage.local.get(['activeTicker']);
  const previousTicker = storage.activeTicker;

  // Convert exchange from TradingView to IBKR format
  const ibkrExchange = convertExchangeTWtoIBKR(exchange);

  // Always add/select ticker regardless of criteria
  // Criteria are only used for removal during wishlist sync
  try {
    const putResponse = await fetchWithConnectionTracking(`${settings.webhookUrl}/ticker`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ symbol, exchange: ibkrExchange })
    });

    if (putResponse.status === 404) {
      // Ticker not found, add it (POST)
      const postResponse = await fetchWithConnectionTracking(`${settings.webhookUrl}/ticker`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ symbol, exchange: ibkrExchange })
      });

      if (postResponse.ok) {
        console.log(`[IBKR Extension] Selected ${exchange}:${symbol} (IBKR: ${ibkrExchange}:${symbol})`);
        await chrome.storage.local.set({
          activeTicker: `${exchange}:${symbol}`,
          lastSync: Date.now()
        });
      } else {
        console.log(`[IBKR Extension] Failed to add ${exchange}:${symbol} (IBKR: ${ibkrExchange}:${symbol}):`, postResponse.status);
      }
    } else if (putResponse.ok) {
      console.log(`[IBKR Extension] Selected ${exchange}:${symbol} (IBKR: ${ibkrExchange}:${symbol})`);
      await chrome.storage.local.set({
        activeTicker: `${exchange}:${symbol}`,
        lastSync: Date.now()
      });
    } else {
      console.log(`[IBKR Extension] Failed to select ${exchange}:${symbol} (IBKR: ${ibkrExchange}:${symbol}):`, putResponse.status);
    }
  } catch (error) {
    // Connection error already logged by fetchWithConnectionTracking
  }

  // Check if previous ticker should be removed from app
  if (previousTicker && previousTicker !== `${exchange}:${symbol}`) {
    const ticker = tickersMatched.get(previousTicker);

    // If ticker not in any wishlists, remove it from app
    if (ticker && ticker.customWishlists.size === 0 && ticker.coloredWishlist === null) {
      const success = await deleteTickerFromApp(previousTicker, ticker);

      if (success) {
        tickersMatched.delete(previousTicker);
        console.log(`[IBKR Extension] Removed previous ticker ${previousTicker} (not in wishlists)`);
      }
    }
  }
}

// Handle custom wishlists update (2.1)
async function handleCustomWishlists(wishlists) {

  // Clear custom wishlist indices for wishlists in response
  for (const wishlist of wishlists) {
    if (matchesCustomWishlistName(wishlist.name)) {
      // Clear this wishlist's index
      const existing = wishlistCustom.get(wishlist.name);
      if (existing) {
        // Remove references from tickers
        for (const tickerKey of existing.tickers) {
          const ticker = tickersMatched.get(tickerKey);
          if (ticker) {
            ticker.customWishlists.delete(wishlist.name);
          }
        }
      }
      // Store id and create new ticker set
      wishlistCustom.set(wishlist.name, { id: wishlist.id, tickers: new Set() });
      wishlistCustomById.set(wishlist.id, wishlist.name);
    }
  }

  // Add tickers from response
  for (const wishlist of wishlists) {
    if (!matchesCustomWishlistName(wishlist.name)) continue;

    const wishlistData = wishlistCustom.get(wishlist.name);
    if (!wishlistData) continue;

    for (const symbolStr of wishlist.symbols) {
      const [exchange, symbol] = symbolStr.split(':');

      // Check exchange criteria
      if (!matchesExchangeCriteria(exchange)) continue;

      const tickerKey = `${exchange}:${symbol}`;

      // Add to tickersMatched if not exists
      if (!tickersMatched.has(tickerKey)) {
        tickersMatched.set(tickerKey, {
          symbol,
          exchange,
          customWishlists: new Set(),
          coloredWishlist: null
        });
      }

      const ticker = tickersMatched.get(tickerKey);
      ticker.customWishlists.add(wishlist.name);
      wishlistData.tickers.add(tickerKey);
    }
  }

  // Cleanup tickers with zero references
  await cleanupTickers();
}

// Handle custom wishlist append/remove (2.1.1, 2.1.2) - POST /custom/{id}/append/ or /custom/{id}/remove/
async function handleCustomWishlistUpdate(wishlistId, symbols) {
  // Find wishlist name by id
  const wishlistName = wishlistCustomById.get(wishlistId);
  if (!wishlistName) return;

  // Check if this wishlist matches criteria
  if (!matchesCustomWishlistName(wishlistName)) return;

  const wishlistData = wishlistCustom.get(wishlistName);
  if (!wishlistData) return;

  // Clear existing tickers for this wishlist
  for (const tickerKey of wishlistData.tickers) {
    const ticker = tickersMatched.get(tickerKey);
    if (ticker) {
      ticker.customWishlists.delete(wishlistName);
    }
  }
  wishlistData.tickers.clear();

  // Add all tickers from response (full list after append)
  for (const symbolStr of symbols) {
    const [exchange, symbol] = symbolStr.split(':');

    // Check exchange criteria
    if (!matchesExchangeCriteria(exchange)) continue;

    const tickerKey = `${exchange}:${symbol}`;

    // Add to tickersMatched if not exists
    if (!tickersMatched.has(tickerKey)) {
      tickersMatched.set(tickerKey, {
        symbol,
        exchange,
        customWishlists: new Set(),
        coloredWishlist: null
      });
    }

    const ticker = tickersMatched.get(tickerKey);
    ticker.customWishlists.add(wishlistName);
    wishlistData.tickers.add(tickerKey);
  }

  // Cleanup tickers with zero references
  await cleanupTickers();
}

// Handle colored wishlist update (2.2, 2.3)
async function handleColoredWishlist(color, symbols) {
  // Skip if color not in criteria
  if (!matchesColoredWishlist(color)) return;

  // Clear colored wishlist index
  const existingSet = wishlistColored.get(color);
  if (existingSet) {
    // Remove references from tickers
    for (const tickerKey of existingSet) {
      const ticker = tickersMatched.get(tickerKey);
      if (ticker && ticker.coloredWishlist === color) {
        ticker.coloredWishlist = null;
      }
    }
  }
  wishlistColored.set(color, new Set());

  const wishlistSet = wishlistColored.get(color);

  // Add tickers from response
  for (const symbolStr of symbols) {
    const [exchange, symbol] = symbolStr.split(':');

    // Check exchange criteria
    if (!matchesExchangeCriteria(exchange)) continue;

    const tickerKey = `${exchange}:${symbol}`;

    // Add to tickersMatched if not exists
    if (!tickersMatched.has(tickerKey)) {
      tickersMatched.set(tickerKey, {
        symbol,
        exchange,
        customWishlists: new Set(),
        coloredWishlist: null
      });
    }

    const ticker = tickersMatched.get(tickerKey);
    ticker.coloredWishlist = color;
    wishlistSet.add(tickerKey);
  }

  // Cleanup tickers with zero references
  await cleanupTickers();
}

// Handle all wishlists update (2.4) - rebuild from scratch
async function handleAllWishlists(wishlists) {

  // Build new structures
  const newTickersMatched = new Map();
  const newWishlistCustom = new Map();
  const newWishlistCustomById = new Map();
  const newWishlistColored = new Map();

  for (const wishlist of wishlists) {
    if (wishlist.type === 'custom') {
      if (!matchesCustomWishlistName(wishlist.name)) continue;

      const wishlistData = { id: wishlist.id, tickers: new Set() };
      newWishlistCustom.set(wishlist.name, wishlistData);
      newWishlistCustomById.set(wishlist.id, wishlist.name);

      for (const symbolStr of wishlist.symbols) {
        const [exchange, symbol] = symbolStr.split(':');
        if (!matchesExchangeCriteria(exchange)) continue;

        const tickerKey = `${exchange}:${symbol}`;

        if (!newTickersMatched.has(tickerKey)) {
          newTickersMatched.set(tickerKey, {
            symbol,
            exchange,
            customWishlists: new Set(),
            coloredWishlist: null
          });
        }

        const ticker = newTickersMatched.get(tickerKey);
        ticker.customWishlists.add(wishlist.name);
        wishlistData.tickers.add(tickerKey);
      }
    } else if (wishlist.type === 'colored') {
      if (!matchesColoredWishlist(wishlist.color)) continue;

      const wishlistSet = new Set();
      newWishlistColored.set(wishlist.color, wishlistSet);

      for (const symbolStr of wishlist.symbols) {
        const [exchange, symbol] = symbolStr.split(':');
        if (!matchesExchangeCriteria(exchange)) continue;

        const tickerKey = `${exchange}:${symbol}`;

        if (!newTickersMatched.has(tickerKey)) {
          newTickersMatched.set(tickerKey, {
            symbol,
            exchange,
            customWishlists: new Set(),
            coloredWishlist: null
          });
        }

        const ticker = newTickersMatched.get(tickerKey);
        ticker.coloredWishlist = wishlist.color;
        wishlistSet.add(tickerKey);
      }
    }
  }

  // Replace old structures with new ones
  tickersMatched.clear();
  wishlistCustom.clear();
  wishlistCustomById.clear();
  wishlistColored.clear();

  for (const [key, value] of newTickersMatched) {
    tickersMatched.set(key, value);
  }
  for (const [key, value] of newWishlistCustom) {
    wishlistCustom.set(key, value);
  }
  for (const [key, value] of newWishlistCustomById) {
    wishlistCustomById.set(key, value);
  }
  for (const [key, value] of newWishlistColored) {
    wishlistColored.set(key, value);
  }

  // Cleanup tickers with zero references
  await cleanupTickers();
}

// Handle bulk remove from colored wishlist (2.5)
// symbols = request body (symbols to remove), response is just {status: "ok"}
async function handleBulkRemove(symbols) {

  for (const symbolStr of symbols) {
    const [exchange, symbol] = symbolStr.split(':');
    const tickerKey = `${exchange}:${symbol}`;

    const ticker = tickersMatched.get(tickerKey);
    if (!ticker) continue;

    // Find which colored wishlist it's in
    if (ticker.coloredWishlist) {
      const color = ticker.coloredWishlist;
      const wishlistSet = wishlistColored.get(color);

      // Remove from index
      if (wishlistSet) {
        wishlistSet.delete(tickerKey);
      }

      // Remove reference from ticker
      ticker.coloredWishlist = null;
    }
  }

  // Cleanup tickers with zero references
  await cleanupTickers();
}

// Check custom wishlist name
function matchesCustomWishlistName(name) {
  if (!name) return false;
  if (!settings.customWishlists.trim()) return false;

  const allowedNames = settings.customWishlists.split(',').map(n => n.trim().toLowerCase());
  return allowedNames.includes(name.toLowerCase());
}

// Check colored wishlist
function matchesColoredWishlist(color) {
  if (!color) return false;
  return settings.coloredWishlists.includes(color);
}

// Check exchange criteria (uses contains instead of equals)
function matchesExchangeCriteria(exchange) {
  if (!settings.exchanges.trim()) return true; // Empty = all exchanges

  const allowedExchanges = settings.exchanges.split(',').map(e => e.trim().toUpperCase());
  const exchangeUpper = exchange.toUpperCase();

  // Check if any allowed exchange is contained in the ticker's exchange
  // Example: "NYSE Arca" contains "NYSE", so it matches
  return allowedExchanges.some(allowed => exchangeUpper.includes(allowed));
}

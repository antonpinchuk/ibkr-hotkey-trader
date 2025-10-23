// Background service worker for TradingView IBKR Integration

let settings = {
  webhookUrl: 'http://127.0.0.1:8496',
  exchanges: '',
  customWishlists: '',
  coloredWishlists: []
};

// Connection state tracking
let isConnected = true;
let lastConnectionError = null;

// Load settings on startup
chrome.runtime.onStartup.addListener(loadSettings);
chrome.runtime.onInstalled.addListener(loadSettings);

async function loadSettings() {
  try {
    const stored = await chrome.storage.sync.get({
      webhookUrl: 'http://127.0.0.1:8496',
      exchanges: '',
      customWishlists: '',
      coloredWishlists: []
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
  } else if (message.type === 'SYNC_WISHLISTS') {
    handleWishlistSync(message.data).then(() => {
      sendResponse({ success: true });
    }).catch(error => {
      console.log('[IBKR Extension] Wishlist sync error:', error);
      sendResponse({ success: false, error: error.message });
    });
    return true; // Keep channel open for async response
  } else if (message.type === 'BULK_REMOVE') {
    handleBulkRemove(message.data).then(() => {
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

  // Always add/select ticker regardless of criteria
  // Criteria are only used for removal during wishlist sync
  try {
    const putResponse = await fetchWithConnectionTracking(`${settings.webhookUrl}/ticker`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ symbol, exchange })
    });

    if (putResponse.status === 404) {
      // Ticker not found, add it (POST)
      const postResponse = await fetchWithConnectionTracking(`${settings.webhookUrl}/ticker`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ symbol, exchange })
      });

      if (postResponse.ok) {
        console.log(`[IBKR Extension] Selected ${exchange}:${symbol}`);
        await chrome.storage.local.set({
          activeTicker: `${exchange}:${symbol}`,
          lastSync: Date.now()
        });
      } else {
        console.log(`[IBKR Extension] Failed to add ${exchange}:${symbol}:`, postResponse.status);
      }
    } else if (putResponse.ok) {
      console.log(`[IBKR Extension] Selected ${exchange}:${symbol}`);
      await chrome.storage.local.set({
        activeTicker: `${exchange}:${symbol}`,
        lastSync: Date.now()
      });
    } else {
      console.log(`[IBKR Extension] Failed to select ${exchange}:${symbol}:`, putResponse.status);
    }
  } catch (error) {
    // Connection error already logged by fetchWithConnectionTracking
  }
}

// Handle wishlist synchronization
async function handleWishlistSync(data) {
  const { wishlists, type, color } = data;

  let filteredTickers = [];

  if (type === 'all') {
    // Filter wishlists by criteria
    const matchingWishlists = wishlists.filter(wl => matchesWishlistCriteria(wl));

    // Build hashmap of symbols
    const symbolMap = new Map();
    matchingWishlists.forEach(wl => {
      wl.symbols.forEach(symbolStr => {
        const [exchange, symbol] = symbolStr.split(':');
        symbolMap.set(symbolStr, { symbol, exchange });
      });
    });

    // Filter by exchange
    filteredTickers = Array.from(symbolMap.values()).filter(ticker =>
      matchesExchangeCriteria(ticker.exchange)
    );
  } else if (type === 'colored') {
    if (matchesWishlistCriteria({ type: 'colored', color })) {
      filteredTickers = wishlists.symbols.map(symbolStr => {
        const [exchange, symbol] = symbolStr.split(':');
        return { symbol, exchange };
      }).filter(ticker => matchesExchangeCriteria(ticker.exchange));
    }
  }

  // Get current active ticker from storage
  const storage = await chrome.storage.local.get(['activeTicker']);
  const activeTicker = storage.activeTicker;

  // Get current tickers from app
  try {
    const response = await fetchWithConnectionTracking(`${settings.webhookUrl}/ticker`);
    if (!response.ok) return;

    const appTickers = await response.json();
    const tvTickerSet = new Set(filteredTickers.map(t => `${t.exchange}:${t.symbol}`));
    const removed = [];

    for (const appTicker of appTickers) {
      const tickerKey = `${appTicker.exchange}:${appTicker.symbol}`;

      // Skip if this is the active ticker (never remove active ticker)
      if (tickerKey === activeTicker) continue;

      // Remove if not in filtered wishlists
      if (!tvTickerSet.has(tickerKey)) {
        try {
          const deleteResponse = await fetchWithConnectionTracking(`${settings.webhookUrl}/ticker`, {
            method: 'DELETE',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ symbol: appTicker.symbol, exchange: appTicker.exchange })
          });

          if (deleteResponse.status === 204) {
            removed.push(tickerKey);
          } else {
            console.log(`[IBKR Extension] Failed to remove ${tickerKey}:`, deleteResponse.status);
          }
        } catch (error) {
          // Connection error already logged by fetchWithConnectionTracking
        }
      }
    }

    if (removed.length > 0) {
      console.log('[IBKR Extension] Removed:', removed.join(', '));
    }
  } catch (error) {
    // Connection error already logged by fetchWithConnectionTracking
  }
}

// Handle bulk remove from colored wishlists
async function handleBulkRemove(data) {
  const { symbols } = data;

  // Get current active ticker from storage
  const storage = await chrome.storage.local.get(['activeTicker']);
  const activeTicker = storage.activeTicker;
  const removed = [];

  for (const symbolStr of symbols) {
    const [exchange, symbol] = symbolStr.split(':');
    const tickerKey = `${exchange}:${symbol}`;

    // Skip if this is the active ticker (never remove active ticker)
    if (tickerKey === activeTicker) continue;

    try {
      const deleteResponse = await fetchWithConnectionTracking(`${settings.webhookUrl}/ticker`, {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ symbol, exchange })
      });

      if (deleteResponse.status === 204) {
        removed.push(tickerKey);
      } else {
        console.log(`[IBKR Extension] Failed to remove ${tickerKey}:`, deleteResponse.status);
      }
    } catch (error) {
      // Connection error already logged by fetchWithConnectionTracking
    }
  }

  if (removed.length > 0) {
    console.log('[IBKR Extension] Removed:', removed.join(', '));
  }
}

// Check if wishlist matches criteria (for sync)
function matchesWishlistCriteria(wishlist) {
  if (wishlist.type === 'custom') {
    return matchesCustomWishlistName(wishlist.name);
  } else if (wishlist.type === 'colored') {
    return matchesColoredWishlist(wishlist.color);
  }
  return false;
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

// Check exchange criteria
function matchesExchangeCriteria(exchange) {
  if (!settings.exchanges.trim()) return true; // Empty = all exchanges

  const allowedExchanges = settings.exchanges.split(',').map(e => e.trim().toUpperCase());
  return allowedExchanges.includes(exchange.toUpperCase());
}

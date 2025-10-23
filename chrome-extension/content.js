// Content script for TradingView IBKR Integration
// Runs on https://www.tradingview.com/chart/*

console.log('[IBKR Extension] Content script loaded');

// Inject script into page context (not isolated world)
// This is necessary because TradingView code runs in page context
// and we need to intercept their fetch/XHR calls
function injectScript() {
  const script = document.createElement('script');
  script.src = chrome.runtime.getURL('injected.js');
  script.onload = function() { this.remove(); };
  (document.head || document.documentElement).appendChild(script);
}

// Inject immediately
injectScript();

// Helper: Handle sendMessage errors gracefully
function handleMessageError(errorContext) {
  return function(response) {
    if (chrome.runtime.lastError) {
      const error = chrome.runtime.lastError.message;
      // Only log unexpected errors
      if (!error.includes('message port closed') &&
          !error.includes('Receiving end does not exist') &&
          !error.includes('Extension context invalidated')) {
        console.warn(`[IBKR Extension] ${errorContext}:`, error);
      }
    }
  };
}

// Wait for page to be fully ready before processing ticker changes
let pageReady = false;
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => {
    setTimeout(() => {
      pageReady = true;
      console.log('[IBKR Extension] Page ready, ticker extraction enabled');
    }, 3000);
  });
} else {
  setTimeout(() => {
    pageReady = true;
    console.log('[IBKR Extension] Page ready, ticker extraction enabled');
  }, 3000);
}

// Listen for chart-token request (ticker changed)
let chartTokenDebounce = null;
window.addEventListener('IBKR_CHART_TOKEN', function() {
  // Skip if page not ready yet
  if (!pageReady) return;

  // Debounce: skip if less than 1 second since last request
  if (chartTokenDebounce) clearTimeout(chartTokenDebounce);

  chartTokenDebounce = setTimeout(() => {
    extractTickerInfo();
  }, 1000);
});

// Listen for wishlist data from injected script (page context)
window.addEventListener('IBKR_WISHLIST_DATA', function(event) {
  const { url, data } = event.detail;

  try {
    // Case 2.1: All wishlists
    if (url.includes('/api/v1/symbols_list/all/')) {
      chrome.runtime.sendMessage({
        type: 'SYNC_WISHLISTS',
        data: { wishlists: data, type: 'all' }
      }, handleMessageError('All wishlists sync'));
    }

    // Case 2.2.1: Colored wishlist active
    else if (url.match(/\/api\/v1\/symbols_list\/active\/(red|blue|green|orange|purple)\//)) {
      const color = url.match(/\/(red|blue|green|orange|purple)\//)[1];
      chrome.runtime.sendMessage({
        type: 'SYNC_WISHLISTS',
        data: { wishlists: data, type: 'colored', color }
      }, handleMessageError('Colored wishlist sync'));
    }

    // Case 2.2.2: Colored wishlist append
    else if (url.match(/\/api\/v1\/symbols_list\/colored\/(red|blue|green|orange|purple)\/append\//)) {
      const color = url.match(/\/(red|blue|green|orange|purple)\//)[1];
      chrome.runtime.sendMessage({
        type: 'SYNC_WISHLISTS',
        data: {
          wishlists: { symbols: data },
          type: 'colored',
          color
        }
      }, handleMessageError('Append sync'));
    }

    // Case 2.3: Bulk remove (data is request body - array of symbols)
    else if (url.includes('/api/v1/symbols_list/colored/bulk_remove/')) {
      if (Array.isArray(data) && data.length > 0) {
        chrome.runtime.sendMessage({
          type: 'BULK_REMOVE',
          data: { symbols: data }
        }, handleMessageError('Bulk remove'));
      }
    }
  } catch (error) {
    console.error('[IBKR Extension] Error processing wishlist:', error);
  }
});

// Extract ticker info from TradingView DOM
async function extractTickerInfo() {
  // Wait for DOM to be ready
  await waitForElement('[class*="button-"][class*="apply-common-tooltip"]', 5000);

  try {
    const tickerData = {
      symbol: null,
      exchange: null,
      inColoredWishlist: false,
      coloredWishlistColor: null,
      inCustomWishlist: false,
      customWishlistName: null
    };

    // Extract symbol (ticker name)
    const symbolButton = document.querySelector('[class*="uppercase-"][class*="button-"][class*="apply-common-tooltip"] div');
    if (symbolButton) {
      tickerData.symbol = symbolButton.textContent.trim();
    }

    // Extract exchange
    const exchangeDiv = document.querySelector('[class*="exchangeTitle-"] div');
    if (exchangeDiv) {
      tickerData.exchange = exchangeDiv.textContent.trim();
    }

    // Extract colored wishlist flag
    const flagButton = document.querySelector('[class*="flag-"][class*="button-"] > div > div > div');
    if (flagButton) {
      const flagClasses = flagButton.className;
      const colorMatch = flagClasses.match(/(red|blue|green|orange|purple)-/);
      if (colorMatch) {
        tickerData.inColoredWishlist = true;
        tickerData.coloredWishlistColor = colorMatch[1];
      }
    }

    // Extract custom wishlist info (if watchlist panel is visible)
    const customWishlist = extractCustomWishlistInfo(tickerData.symbol);
    if (customWishlist) {
      tickerData.inCustomWishlist = customWishlist.inWishlist;
      tickerData.customWishlistName = customWishlist.wishlistName;
    }

    // Send to background script
    if (tickerData.symbol && tickerData.exchange) {
      chrome.runtime.sendMessage({
        type: 'TICKER_CHANGED',
        data: tickerData
      }, handleMessageError('Ticker change'));
    }
  } catch (error) {
    console.error('[IBKR Extension] Error extracting ticker info:', error);
  }
}

// Extract custom wishlist info from watchlist panel
function extractCustomWishlistInfo(symbol) {
  try {
    // Find wishlist name
    const wishlistNameEl = document.querySelector('[class*="titleRow-"]');
    if (!wishlistNameEl) return null;

    const wishlistName = wishlistNameEl.textContent.trim();

    // Find watchlist items container
    const watchlistContainer = document.querySelector('[class*="widgetbar-widget-watchlist"] [class*="listContainer-"]');
    if (!watchlistContainer) return null;

    // Search for symbol in the list
    const items = watchlistContainer.querySelectorAll('[class*="row-"]');

    for (const item of items) {
      const symbolEl = item.querySelector('[class*="symbolCell-"] span');
      if (symbolEl && symbolEl.textContent.trim() === symbol) {
        // Check if item has "active" class
        const parentDiv = item.closest('[class*="row-"]');
        if (parentDiv && parentDiv.className.includes('active-')) {
          return {
            inWishlist: true,
            wishlistName: wishlistName
          };
        }
      }
    }

    return {
      inWishlist: false,
      wishlistName: null
    };
  } catch (error) {
    console.error('[IBKR Extension] Error extracting custom wishlist:', error);
    return null;
  }
}

// Helper: Wait for element to appear
function waitForElement(selector, timeout = 5000) {
  return new Promise((resolve, reject) => {
    const element = document.querySelector(selector);
    if (element) {
      resolve(element);
      return;
    }

    const observer = new MutationObserver((mutations, obs) => {
      const element = document.querySelector(selector);
      if (element) {
        obs.disconnect();
        resolve(element);
      }
    });

    observer.observe(document.body, {
      childList: true,
      subtree: true
    });

    setTimeout(() => {
      observer.disconnect();
      reject(new Error(`Element ${selector} not found after ${timeout}ms`));
    }, timeout);
  });
}

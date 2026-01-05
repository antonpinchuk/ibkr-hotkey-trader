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
    // 2.1.1, 2.1.2: Custom wishlist append/remove - POST /api/v1/symbols_list/custom/{id}/append|remove/
    const customUpdateMatch = url.match(/\/api\/v1\/symbols_list\/custom\/(\d+)\/(append|remove)\//);
    if (customUpdateMatch) {
      const wishlistId = parseInt(customUpdateMatch[1]);
      chrome.runtime.sendMessage({
        type: 'CUSTOM_WISHLIST_UPDATE',
        wishlistId: wishlistId,
        symbols: data
      }, handleMessageError('Custom wishlist update'));
    }

    // 2.1: Custom wishlists - GET /api/v1/symbols_list/custom/
    else if (url.includes('/api/v1/symbols_list/custom/')) {
      // Response can be array of wishlists or single wishlist object
      const wishlists = Array.isArray(data) ? data : [data];
      chrome.runtime.sendMessage({
        type: 'CUSTOM_WISHLISTS',
        data: wishlists
      }, handleMessageError('Custom wishlists'));
    }

    // 2.2: Colored wishlist GET - GET /api/v1/symbols_list/colored/[color]
    else if (url.match(/\/api\/v1\/symbols_list\/colored\/(red|blue|green|orange|purple)$/)) {
      const color = url.match(/\/(red|blue|green|orange|purple)$/)[1];
      chrome.runtime.sendMessage({
        type: 'COLORED_WISHLIST',
        color: color,
        symbols: data.symbols || []
      }, handleMessageError('Colored wishlist GET'));
    }

    // 2.2: Colored wishlist POST - POST /api/v1/symbols_list/active/[color]/
    else if (url.match(/\/api\/v1\/symbols_list\/active\/(red|blue|green|orange|purple)\//)) {
      const color = url.match(/\/(red|blue|green|orange|purple)\//)[1];
      chrome.runtime.sendMessage({
        type: 'COLORED_WISHLIST',
        color: color,
        symbols: data.symbols || []
      }, handleMessageError('Colored wishlist POST active'));
    }

    // 2.3: Colored wishlist append - POST /api/v1/symbols_list/colored/[color]/append/
    else if (url.match(/\/api\/v1\/symbols_list\/colored\/(red|blue|green|orange|purple)\/append\//)) {
      const color = url.match(/\/(red|blue|green|orange|purple)\//)[1];
      // Response is array of symbols in this wishlist
      chrome.runtime.sendMessage({
        type: 'COLORED_WISHLIST',
        color: color,
        symbols: data
      }, handleMessageError('Colored wishlist append'));
    }

    // 2.4: All wishlists - GET /api/v1/symbols_list/all/
    else if (url.includes('/api/v1/symbols_list/all/')) {
      // Response should be array of wishlists
      const wishlists = Array.isArray(data) ? data : [data];
      chrome.runtime.sendMessage({
        type: 'ALL_WISHLISTS',
        data: wishlists
      }, handleMessageError('All wishlists'));
    }

    // 2.5: Bulk remove - POST /api/v1/symbols_list/colored/bulk_remove/
    // data = request body (symbols to remove), response is just {status: "ok"}
    else if (url.includes('/api/v1/symbols_list/colored/bulk_remove/')) {
      if (Array.isArray(data) && data.length > 0) {
        chrome.runtime.sendMessage({
          type: 'BULK_REMOVE',
          symbols: data
        }, handleMessageError('Bulk remove'));
      }
    }
  } catch (error) {
    console.error('[IBKR Extension] Error processing wishlist:', error);
  }
});

// Extract ticker info from TradingView DOM
async function extractTickerInfo() {
  try {
    const tickerData = {
      symbol: null,
      exchange: null
    };

    // Wait for symbol to be ready
    await waitForElement('span[class*="symbolName-"]', 5000);
    const symbolSpan = document.querySelector('span[class*="symbolName-"]');
    if (symbolSpan) {
      tickerData.symbol = symbolSpan.textContent.trim();
    }

    // Wait for exchange to be ready (may update slower than symbol)
    await waitForElement('[class*="exchangeTitle-"]', 5000);
    let exchangeDiv = document.querySelector('[class*="exchangeTitle-"] div');
    if (exchangeDiv) {
      tickerData.exchange = exchangeDiv.textContent.trim();
    }

    // Send to background script
    if (tickerData.symbol && tickerData.exchange) {
      chrome.runtime.sendMessage({
        type: 'TICKER_CHANGED',
        data: tickerData
      }, handleMessageError('Ticker change'));
    } else {
      console.warn('[IBKR Extension] Failed to extract ticker data:', tickerData);
    }
  } catch (error) {
    console.error('[IBKR Extension] Error extracting ticker info:', error);
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

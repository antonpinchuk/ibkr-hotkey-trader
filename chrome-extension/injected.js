// Injected script - runs in page context (not isolated world)
// This allows us to intercept TradingView's actual fetch/XHR calls

(function() {
  'use strict';

  console.log('[IBKR Extension] Injected script ready');

  // ===================
  // 1. FETCH INTERCEPTOR
  // ===================
  const originalFetch = window.fetch;

  window.fetch = async function(...args) {
    const url = args[0];
    const options = args[1] || {};

    // Bulk remove - process REQUEST body before sending
    if (typeof url === 'string' && url.includes('/api/v1/symbols_list/colored/bulk_remove/')) {
      try {
        const requestBody = options.body ? JSON.parse(options.body) : null;
        if (requestBody) {
          window.dispatchEvent(new CustomEvent('IBKR_WISHLIST_DATA', {
            detail: { url: url, data: requestBody }
          }));
        }
      } catch (error) {
        console.warn('[IBKR Extension] Error processing bulk_remove:', error);
      }
    }

    const response = await originalFetch.apply(this, args);

    // Chart-token request = ticker changed
    if (typeof url === 'string' && url.includes('/chart-token/')) {
      window.dispatchEvent(new CustomEvent('IBKR_CHART_TOKEN'));
    }

    // Process response if it's a symbols_list request (but not bulk_remove - already processed)
    if (typeof url === 'string' && url.includes('/api/v1/symbols_list/') && !url.includes('/bulk_remove/')) {
      const clone = response.clone();

      try {
        const data = await clone.json();
        window.dispatchEvent(new CustomEvent('IBKR_WISHLIST_DATA', {
          detail: { url: url, data: data }
        }));
      } catch (error) {
        console.warn('[IBKR Extension] Error processing wishlist:', error);
      }
    }

    return response;
  };

  // ===================
  // 2. XHR INTERCEPTOR
  // ===================
  const originalXHROpen = XMLHttpRequest.prototype.open;
  const originalXHRSend = XMLHttpRequest.prototype.send;

  XMLHttpRequest.prototype.open = function(method, url, ...rest) {
    this._url = url;
    this._method = method;
    return originalXHROpen.call(this, method, url, ...rest);
  };

  XMLHttpRequest.prototype.send = function(...args) {
    const url = this._url;
    const requestBody = args[0];

    // Chart-token request = ticker changed
    if (typeof url === 'string' && url.includes('/chart-token/')) {
      this.addEventListener('load', function() {
        window.dispatchEvent(new CustomEvent('IBKR_CHART_TOKEN'));
      });
    }

    // Bulk remove - process REQUEST body before sending
    if (typeof url === 'string' && url.includes('/api/v1/symbols_list/colored/bulk_remove/')) {
      try {
        const data = requestBody ? JSON.parse(requestBody) : null;
        if (data) {
          window.dispatchEvent(new CustomEvent('IBKR_WISHLIST_DATA', {
            detail: { url: url, data: data }
          }));
        }
      } catch (error) {
        console.warn('[IBKR Extension] Error processing bulk_remove:', error);
      }
    }
    // Other wishlist sync requests - process RESPONSE
    else if (typeof url === 'string' && url.includes('/api/v1/symbols_list/')) {
      this.addEventListener('load', function() {
        try {
          const responseText = this.responseText;
          if (!responseText) return;

          const data = JSON.parse(responseText);
          window.dispatchEvent(new CustomEvent('IBKR_WISHLIST_DATA', {
            detail: { url: url, data: data }
          }));
        } catch (error) {
          console.warn('[IBKR Extension] Error processing wishlist:', error);
        }
      });
    }

    return originalXHRSend.apply(this, args);
  };

  // Signal that injection is complete
  window.dispatchEvent(new CustomEvent('IBKR_INJECTION_READY'));
})();

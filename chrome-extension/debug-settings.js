// Debug script to check stored settings
// Open chrome://extensions/ → TradingView IBKR Integration → service worker console
// Then paste this code to see current settings

chrome.storage.sync.get({
  webhookUrl: 'http://127.0.0.1:8496',
  exchanges: '',
  customWishlists: '',
  coloredWishlists: []
}, (result) => {
  console.log('=== STORED SETTINGS ===');
  console.log('webhookUrl:', result.webhookUrl);
  console.log('exchanges:', result.exchanges);
  console.log('customWishlists:', result.customWishlists);
  console.log('coloredWishlists:', result.coloredWishlists);
  console.log('coloredWishlists is array:', Array.isArray(result.coloredWishlists));
  console.log('coloredWishlists length:', result.coloredWishlists.length);
  console.log('coloredWishlists includes green:', result.coloredWishlists.includes('green'));
  console.log('coloredWishlists includes orange:', result.coloredWishlists.includes('orange'));
});

// Default settings
const DEFAULT_SETTINGS = {
  webhookUrl: 'http://127.0.0.1:8496',
  exchanges: '',
  customWishlists: '',
  coloredWishlists: [],
  exchangeMapping: [
    { from: 'NYSE Arca', to: 'AMEX' }
  ]
};

// Load settings on popup open
document.addEventListener('DOMContentLoaded', async () => {
  await loadSettings();
  await checkConnection();
  updateDebugInfo();
});

// Load settings from storage
async function loadSettings() {
  const settings = await chrome.storage.sync.get(DEFAULT_SETTINGS);

  document.getElementById('webhookUrl').value = settings.webhookUrl;
  document.getElementById('exchanges').value = settings.exchanges;
  document.getElementById('customWishlists').value = settings.customWishlists;

  // Load colored wishlist checkboxes
  const colors = ['red', 'blue', 'green', 'orange', 'purple'];
  colors.forEach(color => {
    const checkbox = document.getElementById(`color-${color}`);
    if (checkbox) {
      checkbox.checked = settings.coloredWishlists.includes(color);
    }
  });

  // Load exchange mapping
  renderExchangeMapping(settings.exchangeMapping || DEFAULT_SETTINGS.exchangeMapping);
}

// Render exchange mapping UI
function renderExchangeMapping(mappings) {
  const container = document.getElementById('exchangeMappingContainer');
  container.innerHTML = '';

  mappings.forEach((mapping, index) => {
    const row = document.createElement('div');
    row.className = 'mapping-row';
    row.innerHTML = `
      <input type="text" class="mapping-from" placeholder="TradingView Exchange" value="${mapping.from}" data-index="${index}">
      <span>→</span>
      <input type="text" class="mapping-to" placeholder="IBKR Exchange" value="${mapping.to}" data-index="${index}">
      <button class="btn-remove" data-index="${index}">×</button>
    `;
    container.appendChild(row);
  });

  // Add event listeners for remove buttons
  container.querySelectorAll('.btn-remove').forEach(btn => {
    btn.addEventListener('click', (e) => {
      const index = parseInt(e.target.dataset.index);
      removeMappingRow(index);
    });
  });
}

// Add new mapping row
document.getElementById('addMappingBtn').addEventListener('click', async () => {
  const settings = await chrome.storage.sync.get(DEFAULT_SETTINGS);
  const mappings = settings.exchangeMapping || [];
  mappings.push({ from: '', to: '' });
  renderExchangeMapping(mappings);
});

// Remove mapping row
async function removeMappingRow(index) {
  const settings = await chrome.storage.sync.get(DEFAULT_SETTINGS);
  const mappings = settings.exchangeMapping || [];
  mappings.splice(index, 1);
  renderExchangeMapping(mappings);
}

// Save settings
document.getElementById('saveBtn').addEventListener('click', async () => {
  const settings = {
    webhookUrl: document.getElementById('webhookUrl').value.trim() || DEFAULT_SETTINGS.webhookUrl,
    exchanges: document.getElementById('exchanges').value.trim(),
    customWishlists: document.getElementById('customWishlists').value.trim(),
    coloredWishlists: [],
    exchangeMapping: []
  };

  // Collect selected colors
  const colors = ['red', 'blue', 'green', 'orange', 'purple'];
  colors.forEach(color => {
    const checkbox = document.getElementById(`color-${color}`);
    if (checkbox && checkbox.checked) {
      settings.coloredWishlists.push(color);
    }
  });

  // Collect exchange mappings
  const mappingContainer = document.getElementById('exchangeMappingContainer');
  const mappingRows = mappingContainer.querySelectorAll('.mapping-row');
  mappingRows.forEach(row => {
    const from = row.querySelector('.mapping-from').value.trim();
    const to = row.querySelector('.mapping-to').value.trim();
    if (from && to) {
      settings.exchangeMapping.push({ from, to });
    }
  });

  // Ensure at least default mapping exists
  if (settings.exchangeMapping.length === 0) {
    settings.exchangeMapping = DEFAULT_SETTINGS.exchangeMapping;
  }

  await chrome.storage.sync.set(settings);

  showMessage('Settings saved successfully!', 'success');

  // Notify background script to reload settings
  chrome.runtime.sendMessage({ type: 'SETTINGS_UPDATED' }, (response) => {
    if (chrome.runtime.lastError) {
      console.error('[IBKR Extension] Error notifying background:', chrome.runtime.lastError.message);
    }
  });
});

// Test connection
document.getElementById('testBtn').addEventListener('click', async () => {
  const webhookUrl = document.getElementById('webhookUrl').value.trim() || DEFAULT_SETTINGS.webhookUrl;

  try {
    const response = await fetch(`${webhookUrl}/ticker`, {
      method: 'GET',
      headers: { 'Content-Type': 'application/json' }
    });

    if (response.ok) {
      const data = await response.json();
      showMessage(`Connection successful! Found ${data.length || 0} tickers.`, 'success');
      updateStatus(true);
    } else {
      showMessage(`Connection failed: ${response.status} ${response.statusText}`, 'error');
      updateStatus(false);
    }
  } catch (error) {
    showMessage(`Connection error: ${error.message}`, 'error');
    updateStatus(false);
  }
});

// Check connection status
async function checkConnection() {
  const settings = await chrome.storage.sync.get(DEFAULT_SETTINGS);

  try {
    const response = await fetch(`${settings.webhookUrl}/ticker`, {
      method: 'GET',
      headers: { 'Content-Type': 'application/json' }
    });

    updateStatus(response.ok);
  } catch (error) {
    updateStatus(false);
  }
}

// Update status indicator
function updateStatus(connected) {
  const indicator = document.getElementById('statusIndicator');
  const text = document.getElementById('statusText');

  if (connected) {
    indicator.classList.add('connected');
    text.textContent = 'Connected to IBKR';
  } else {
    indicator.classList.remove('connected');
    text.textContent = 'Not connected';
  }
}

// Show message
function showMessage(text, type) {
  const messageEl = document.getElementById('message');
  messageEl.textContent = text;
  messageEl.className = `message ${type}`;

  setTimeout(() => {
    messageEl.className = 'message';
  }, 5000);
}

// Update debug info
async function updateDebugInfo() {
  const data = await chrome.storage.local.get(['activeTicker', 'lastSync']);

  document.getElementById('activeTicker').textContent = data.activeTicker || '-';
  document.getElementById('lastSync').textContent = data.lastSync
    ? new Date(data.lastSync).toLocaleTimeString()
    : '-';

  // Refresh every 2 seconds
  setTimeout(updateDebugInfo, 2000);
}

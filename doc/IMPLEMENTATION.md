# Technical Implementation Details

## Order History 

### Tracking & Deduplication

**Проблема:** TWS API може надсилати кілька callbacks для одного ордера (`openOrder`, `completedOrder`, `orderStatus`), що призводить до дублікатів в UI.

**Рішення - Order Matching Logic:**

1. **Ордери з API (orderId > 0):**
    - Матчимо по `orderId`
    - При отриманні callback перевіряємо чи є ордер в пам'яті (`m_orders`)
    - Якщо є → **оновлюємо** існуючий ордер (статус, ціну, qty)
    - Якщо нема → додаємо новий
    - **Важливо:** Не додавати дублікат якщо статус змінився (PreSubmitted → Filled/Cancelled)

2. **Historical orders з TWS (orderId = 0):**
    - Неможливо матчити по ID (бо він 0)
    - Матчимо по **symbol + qty** АБО **symbol + price**

   **Можливі сценарії:**
    - Змінили ціну при тому ж qty і статусі PreSubmitted → матч по symbol+buysell+qty
    - Змінили qty при тій ж ціні і статусі PreSubmitted → матч по symbol+buysell+price
    - Ордер відмінили PreSubmitted → Cancelled → матч по symbol+buysell+qty або symbol+buysell+price
    - Ордер закрився PreSubmitted → Filled → матч по symbol+buysell+qty або symbol+buysell+price
      Обмеження: Не відловить одночасну зміну qty та ціни (рідкий кейс)

3. **Timestamp Logic:**
    - **Historical orders** (orderId=0, completed з TWS на старті): timestamp **порожній** (не можемо отримати час з API)
    - **Нові ордери** (створені через програму): timestamp = **NOW** при:
        - Order confirmed (PreSubmitted)
        - Order status updated (Filled/Cancelled)
        - Order price/qty changed
    - Сортування:
        - Filled orders: по `fillTime` (найточніший)
        - Pending/Cancelled orders: по `timestamp`
        - Historical orders (без timestamp): по `permId` (негативне значення, щоб були внизу списку)
        - **Примітка:** `permId` НЕ є глобально монотонно зростаючим - він може стрибати між різними символами

4. **Thread Safety:**
    - **EReader потік** (створений через `m_reader->start()`) читає з сокета і заповнює чергу повідомлень
    - **Main thread** (Qt timer 50ms) викликає `processMsgs()` для обробки черги
    - **Конфлікт mutex:** Обидва потоки можуть одночасно намагатися залочити `m_csMsgQueue` (внутрішній mutex TWS API)
    - **Рішення:** Обгортаємо `processMsgs()` в try-catch для `std::system_error` - при конфлікті просто пропускаємо цикл
    - **Важливо:** НЕ використовувати додатковий QMutex - це створює deadlock!


## Order Type and Price panel

Order Panel Widget - Created above the chart with:
- Order type dropdown (LMT/MKT) with persistence to SQLite
- Buy and Sell price fields that auto-update from market ticks
- Reset button to clear manual prices and re-enable auto-updates

✓ Auto-pricing Logic - Target prices in TradingManager:
- Buy price: Ask + offset (auto-updated from ticks)
- Sell price: Bid - offset (auto-updated from ticks)
- Manual prices override auto-updates (without adding offsets)

✓ Manual Price Override:
- Fields stop auto-updating when user focuses or edits them
- Manual prices are used WITHOUT offsets when placing orders
- Reset button properly clears both UI and TradingManager state

✓ Market Order Restrictions:
- MKT orders blocked outside regular trading hours (9:30-16:00 EST)
- Visual feedback: MKT option disabled in dropdown when outside hours
- Proper TIF handling: DAY during session, GTC outside session

✓ Trading Button State Management:
- Buttons disabled when target prices are invalid (0.0 for LMT orders)
- Reset button clears prices and blocks trading buttons until valid prices came from ticks


## Candle Chart

### Architecture

The chart uses a hybrid approach for real-time candle updates:

1. **Completed 5s bars** (from TWS API `reqRealTimeBars`):
   - Accurate OHLCV data from TWS
   - Stored in cache (`m_tickerData`)
   - Aggregated into larger timeframes (10s, 30s, 1m, 5m, 15m, 30m, 1H)
   - Updates chart when received

2. **Current dynamic candle** (from `reqTickByTickData`):
   - Built from live ticks (bid/ask midpoint)
   - **NOT stored in cache** (display only)
   - Replaced by accurate bar from `reqRealTimeBars` when completed
   - Handles async delays: starts new candle from previous close if no ticks

3. **Candle boundary timer** (aligned to 5s):
   - Detects transition to new candle period
   - If no ticks arrive, draws flat line from previous close
   - Ensures smooth display during low activity

### Data Flow

```
reqRealTimeBars (5s completed bars)
    ↓
TickerDataManager.onRealTimeBarReceived()
    ↓
Cache + Aggregation → barsUpdated() → ChartWidget


reqTickByTickData (live ticks)
    ↓
TickerDataManager.onTickByTickUpdate()
    ↓
Build dynamic candle → currentBarUpdated() → ChartWidget
    (NOT in cache!)
```

### Key Implementation Details

- TWS API limitation: `reqRealTimeBars` only supports 5s bars
- Larger timeframes built by aggregating 5s bars in memory
- Price lines (bid/ask/mid) also use `reqTickByTickData`
- Timer aligned to 5s boundaries for synchronization
- Historical data loaded via `reqHistoricalData` for initial chart

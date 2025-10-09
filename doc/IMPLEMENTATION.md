# Technical Implementation Details

## Order Tracking & Deduplication
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

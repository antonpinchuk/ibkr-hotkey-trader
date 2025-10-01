## Зроблено

2. MainWindow (mainwindow.h/cpp) - повний UI layout:
    - Top toolbar з тікером, trading кнопками та Settings
    - Menu bar (IBKR Hotkey Trader, File, Orders, Help)
    - 3-панельний layout (Left/Center/Right)
    - Hotkey handlers (Cmd+O, Cmd+P, Cmd+1-9, Cmd+Z/X/C/V, Esc, Cmd+K)
    - Enable/disable trading controls при підключенні
2. TradingManager (tradingmanager.h/cpp) - торгова логіка:
   - Opening positions (100%, 50%)
   - Adding to position (5-50%)
   - Closing positions (25-100%)
   - Order tracking і update logic
   - Budget checks і warnings
   - Auto-update buy orders коли Ask змінюється
   - Auto-update sell orders з подвоєнням Bid offset
3. Widgets (базові заглушки):
   - TickerListWidget - список символів зліва
   - ChartWidget - чарт з QCandlestickSeries
   - OrderHistoryWidget - історія з табами Current/All + статистика
4. Dialogs:
   - SettingsDialog - таби Trading, Limits, Connection (збереження/читання)
   - SymbolSearchDialog - пошук символів з autocomplete
 
## Що залишилось доробити:
   - Повна імплементація ChartWidget з real-time candlesticks
   - Повна імплементація OrderHistoryWidget з order tracking
   - IBKR API symbol search integration
   - Account info з IBKR
   - Error handling покращення

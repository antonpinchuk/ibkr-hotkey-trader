#ifndef ORDERPANEL_H
#define ORDERPANEL_H

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class OrderPanel : public QWidget
{
    Q_OBJECT

public:
    explicit OrderPanel(QWidget *parent = nullptr);

    // Event filter for detecting focus changes
    bool eventFilter(QObject* obj, QEvent* event) override;

    // Get current order type (LMT or MKT)
    QString orderType() const;
    void setOrderType(const QString& type);

    // Get/Set prices
    double buyPrice() const;
    double sellPrice() const;
    void setBuyPrice(double price);
    void setSellPrice(double price);

    // Enable/disable price fields based on order type
    void updatePriceFieldsState();

    // Reset user edit flags (call when ticker changes)
    void resetPriceFields();

    // Enable/disable MKT option based on trading hours
    void setMarketOrdersEnabled(bool enabled);

    // Enable/disable entire order panel (when no ticker or not connected)
    void setOrderPanelEnabled(bool enabled);

signals:
    void orderTypeChanged(const QString& type);
    void buyPriceChanged(double price);
    void sellPriceChanged(double price);

private slots:
    void onOrderTypeChanged(int index);
    void onBuyPriceEdited();
    void onSellPriceEdited();
    void onBuyPriceFocusChanged(bool hasFocus);
    void onSellPriceFocusChanged(bool hasFocus);

private:
    void updateResetButtonState();

    QComboBox* m_orderTypeCombo;
    QLineEdit* m_buyPriceEdit;
    QLineEdit* m_sellPriceEdit;
    QPushButton* m_resetButton;

    bool m_buyPriceUserEdited;  // Track if user manually edited buy price
    bool m_sellPriceUserEdited; // Track if user manually edited sell price
};

#endif // ORDERPANEL_H

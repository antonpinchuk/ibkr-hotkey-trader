#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onAccepted();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    QTabWidget *m_tabWidget;

    // Trading tab
    QComboBox *m_accountCombo;
    QLineEdit *m_budgetEdit;

    // Limits tab
    QSpinBox *m_askOffsetSpin;
    QSpinBox *m_bidOffsetSpin;

    // Hotkeys tab
    QSpinBox *m_hotkeyOpen100;
    QSpinBox *m_hotkeyOpen50;
    QSpinBox *m_hotkeyAdd1;
    QSpinBox *m_hotkeyAdd2;
    QSpinBox *m_hotkeyAdd3;
    QSpinBox *m_hotkeyAdd4;
    QSpinBox *m_hotkeyAdd5;
    QSpinBox *m_hotkeyAdd6;
    QSpinBox *m_hotkeyAdd7;
    QSpinBox *m_hotkeyAdd8;
    QSpinBox *m_hotkeyAdd9;
    QSpinBox *m_hotkeyAdd0;
    QSpinBox *m_hotkeyClose100;
    QSpinBox *m_hotkeyClose75;
    QSpinBox *m_hotkeyClose50;
    QSpinBox *m_hotkeyClose25;

    // Connection tab
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QSpinBox *m_clientIdSpin;
};

#endif // SETTINGSDIALOG_H
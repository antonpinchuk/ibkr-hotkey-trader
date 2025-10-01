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

    // Connection tab
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QSpinBox *m_clientIdSpin;
};

#endif // SETTINGSDIALOG_H
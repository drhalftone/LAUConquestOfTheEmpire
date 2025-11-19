#ifndef WALLETWINDOW_H
#define WALLETWINDOW_H

#include <QWidget>
#include <QMap>

class WalletWindow : public QWidget
{
    Q_OBJECT

public:
    explicit WalletWindow(QWidget *parent = nullptr);

    void updateWallets(const QMap<QChar, int> &wallets);
    void addToWallet(QChar player, int amount);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMap<QChar, int> m_wallets;  // Maps 'A'-'F' to their accumulated wealth
};

#endif // WALLETWINDOW_H

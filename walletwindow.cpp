#include "walletwindow.h"
#include <QPainter>
#include <QFont>

WalletWindow::WalletWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Player Wallets (Accumulated Taxes)");
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    resize(400, 200);

    // Initialize wallets to 0
    for (char c = 'A'; c <= 'F'; ++c) {
        m_wallets[QChar(c)] = 0;
    }
}

void WalletWindow::updateWallets(const QMap<QChar, int> &wallets)
{
    m_wallets = wallets;
    update();
}

void WalletWindow::addToWallet(QChar player, int amount)
{
    m_wallets[player] += amount;
    update();
}

void WalletWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw title
    QFont titleFont = painter.font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(rect().adjusted(0, 5, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "Player Wallets (Taxes)");

    // Calculate grid layout (2 rows, 3 columns)
    int startY = 40;
    int cellWidth = width() / 3;
    int cellHeight = (height() - startY) / 2;

    // Draw wallets in a 2x3 grid
    const QString players = "ABCDEF";
    for (int i = 0; i < 6; ++i) {
        int row = i / 3;
        int col = i % 3;

        int x = col * cellWidth;
        int y = startY + row * cellHeight;

        QChar player = players[i];

        // Get player color (dark)
        QColor playerColorDark;
        switch (player.toLatin1()) {
            case 'A': playerColorDark = Qt::red; break;
            case 'B': playerColorDark = Qt::green; break;
            case 'C': playerColorDark = Qt::blue; break;
            case 'D': playerColorDark = Qt::yellow; break;
            case 'E': playerColorDark = Qt::black; break;
            case 'F': playerColorDark = QColor(255, 165, 0); break; // Orange
            default: playerColorDark = Qt::gray; break;
        }

        // Get player color (light)
        QColor playerColorLight;
        switch (player.toLatin1()) {
            case 'A': playerColorLight = QColor(255, 200, 200); break; // Light red
            case 'B': playerColorLight = QColor(200, 255, 200); break; // Light green
            case 'C': playerColorLight = QColor(200, 200, 255); break; // Light blue
            case 'D': playerColorLight = QColor(255, 255, 200); break; // Light yellow
            case 'E': playerColorLight = QColor(220, 220, 220); break; // Light gray
            case 'F': playerColorLight = QColor(255, 220, 180); break; // Light orange
            default: playerColorLight = Qt::lightGray; break;
        }

        // Draw cell border with dark player color and fill with light player color
        painter.setPen(QPen(playerColorDark, 3));
        painter.setBrush(playerColorLight);
        painter.drawRect(x + 5, y + 5, cellWidth - 10, cellHeight - 10);

        // Draw player letter
        QFont playerFont = painter.font();
        playerFont.setPointSize(16);
        playerFont.setBold(true);
        painter.setFont(playerFont);

        painter.setPen(playerColorDark);
        QRect letterRect(x + 5, y + 5, cellWidth - 10, cellHeight / 2);
        painter.drawText(letterRect, Qt::AlignCenter, QString("Player %1").arg(player));

        // Draw wallet amount
        QFont walletFont = painter.font();
        walletFont.setPointSize(20);
        walletFont.setBold(true);
        painter.setFont(walletFont);

        painter.setPen(Qt::black);
        QRect walletRect(x + 5, y + 5 + cellHeight / 2, cellWidth - 10, cellHeight / 2);
        painter.drawText(walletRect, Qt::AlignCenter, QString::number(m_wallets[player]));
    }
}

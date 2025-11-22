#include "scorewindow.h"
#include <QPainter>
#include <QFont>

ScoreWindow::ScoreWindow(int numPlayers, QWidget *parent)
    : QWidget(parent)
    , m_numPlayers(numPlayers)
{
    setWindowTitle("Player Scores");
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);

    initializePlayers();
}

void ScoreWindow::initializePlayers()
{
    m_playerIds.clear();
    m_scores.clear();

    // Initialize player IDs and scores based on number of players
    for (int i = 0; i < m_numPlayers && i < 6; ++i) {
        QChar playerId = QChar('A' + i);
        m_playerIds.append(playerId);
        m_scores[playerId] = 0;
    }

    // Resize window based on number of players (minimum width per player)
    int windowWidth = qMax(400, m_numPlayers * 130);
    resize(windowWidth, 120);
}

void ScoreWindow::setNumPlayers(int numPlayers)
{
    m_numPlayers = numPlayers;
    initializePlayers();
    update();
}

void ScoreWindow::updateScores(const QMap<QChar, int> &scores)
{
    m_scores = scores;
    update();
}

void ScoreWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw title
    QFont titleFont = painter.font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(rect().adjusted(0, 5, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "Player Scores");

    // Calculate layout for single row based on number of players
    int startY = 30;
    int cellWidth = m_numPlayers > 0 ? width() / m_numPlayers : width();
    int cellHeight = height() - startY - 10;

    // Draw scores in a single row
    for (int i = 0; i < m_playerIds.size(); ++i) {
        int x = i * cellWidth;
        int y = startY;

        QChar player = m_playerIds[i];

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
        playerFont.setPointSize(11);
        playerFont.setBold(true);
        painter.setFont(playerFont);

        painter.setPen(playerColorDark);
        QRect letterRect(x + 5, y + 5, cellWidth - 10, cellHeight / 2);
        painter.drawText(letterRect, Qt::AlignCenter, QString("Player %1").arg(player));

        // Draw score
        QFont scoreFont = painter.font();
        scoreFont.setPointSize(14);
        scoreFont.setBold(true);
        painter.setFont(scoreFont);

        painter.setPen(Qt::black);
        QRect scoreRect(x + 5, y + 5 + cellHeight / 2, cellWidth - 10, cellHeight / 2);
        painter.drawText(scoreRect, Qt::AlignCenter, QString::number(m_scores[player]));
    }
}

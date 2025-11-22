#ifndef SCOREWINDOW_H
#define SCOREWINDOW_H

#include <QWidget>
#include <QMap>
#include <QList>

class ScoreWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ScoreWindow(int numPlayers = 6, QWidget *parent = nullptr);

    void updateScores(const QMap<QChar, int> &scores);
    void setNumPlayers(int numPlayers);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void initializePlayers();

    QMap<QChar, int> m_scores;  // Maps player IDs to their scores
    int m_numPlayers;           // Number of players in the game
    QList<QChar> m_playerIds;   // List of player IDs ('A', 'B', etc.)
};

#endif // SCOREWINDOW_H

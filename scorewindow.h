#ifndef SCOREWINDOW_H
#define SCOREWINDOW_H

#include <QWidget>
#include <QMap>

class ScoreWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ScoreWindow(QWidget *parent = nullptr);

    void updateScores(const QMap<QChar, int> &scores);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMap<QChar, int> m_scores;  // Maps 'A'-'F' to their scores
};

#endif // SCOREWINDOW_H

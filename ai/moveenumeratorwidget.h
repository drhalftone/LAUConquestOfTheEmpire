#ifndef MOVEENUMERATORWIDGET_H
#define MOVEENUMERATORWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>

class Player;
class MapGraph;

/**
 * @brief Widget for displaying move enumeration reports
 *
 * Shows all possible moves for a selected player, including
 * general moves and troop moves with validation.
 */
class MoveEnumeratorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MoveEnumeratorWidget(QWidget *parent = nullptr);
    ~MoveEnumeratorWidget();

    /**
     * @brief Set the players and map graph for enumeration
     */
    void setGameState(const QList<Player*> &players, MapGraph *graph);

public slots:
    /**
     * @brief Regenerate the report for the currently selected player
     */
    void refreshReport();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onPlayerChanged(int index);
    void onRefreshClicked();

private:
    void setupUI();
    QString generateReport(Player *player);
    void saveSettings();
    void loadSettings();

    // UI components
    QComboBox *m_playerCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QTextEdit *m_reportTextEdit = nullptr;
    QLabel *m_statsLabel = nullptr;

    // Game state references
    QList<Player*> m_players;
    MapGraph *m_graph = nullptr;
};

#endif // MOVEENUMERATORWIDGET_H

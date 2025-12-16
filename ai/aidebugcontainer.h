#ifndef AIDEBUGCONTAINER_H
#define AIDEBUGCONTAINER_H

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QMap>
#include "aidebugwidget.h"

class Player;
class AIPlayer;

/**
 * @brief Container dialog that holds multiple AIDebugWidget instances in tabs
 *
 * This consolidates all AI debug windows into a single tabbed dialog,
 * making it easier to monitor multiple AI players simultaneously.
 */
class AIDebugContainer : public QDialog
{
    Q_OBJECT

public:
    explicit AIDebugContainer(QWidget *parent = nullptr);
    ~AIDebugContainer();

    /**
     * @brief Add an AI player to the container
     * Creates a new tab with an AIDebugWidget for this player
     * @param ai The AI player to add
     * @param player The player (for getting name/id)
     */
    void addAIPlayer(AIPlayer *ai, Player *player);

    /**
     * @brief Remove an AI player from the container
     * @param playerId The player ID to remove
     */
    void removeAIPlayer(QChar playerId);

    /**
     * @brief Get the debug widget for a specific player
     * @param playerId The player ID
     * @return The AIDebugWidget, or nullptr if not found
     */
    AIDebugWidget* getDebugWidget(QChar playerId) const;

    /**
     * @brief Get number of AI players in the container
     */
    int count() const { return m_tabWidget->count(); }

    /**
     * @brief Clear all tabs
     */
    void clear();

    /**
     * @brief Switch to the tab for a specific player
     * @param playerId The player ID to show
     */
    void showPlayer(QChar playerId);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUI();
    void saveSettings();
    void loadSettings();

    QTabWidget *m_tabWidget;
    QMap<QChar, AIDebugWidget*> m_debugWidgets;  // playerId -> widget
    QMap<QChar, int> m_playerTabIndex;           // playerId -> tab index
};

#endif // AIDEBUGCONTAINER_H

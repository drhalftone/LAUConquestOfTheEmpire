#ifndef AIDEBUGWIDGET_H
#define AIDEBUGWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QListWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QProgressBar>
#include "aiplayer.h"

class AIDebugWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AIDebugWidget(QWidget *parent = nullptr);
    ~AIDebugWidget();

    // Connect to an AIPlayer instance
    void setAIPlayer(AIPlayer *ai);
    AIPlayer* getAIPlayer() const { return m_aiPlayer; }

    // Clear all displays
    void clear();

public slots:
    // === Slots connected to AIPlayer signals ===
    void onPhaseChanged(AIPlayer::Phase phase);
    void onStateUpdated(int wallet, int territoryCount, int pieceCount);
    void onLeadersUpdated(const QStringList &leaderDescriptions);
    void onMovesEvaluated(const QList<MoveEvaluation> &evaluations);
    void onMoveSelected(const MoveEvaluation &move);
    void onActionTaken(const QString &timestamp, const QString &description);
    void onCombatAnalysisUpdated(const QString &territory,
                                  int myForces, int enemyForces,
                                  int myAdvantage, int enemyAdvantage,
                                  int estimatedWinPercent);
    void onPurchasePlanUpdated(int budget, const PurchaseDecision &decision);
    void onWaitingForStep();

private slots:
    // === Control button handlers ===
    void onStepClicked();
    void onAutoClicked();
    void onPauseClicked();
    void onSpeedChanged(int value);
    void onStrategyChanged(int index);
    void onClearLogClicked();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUI();

    // UI Section creators
    QGroupBox* createStateSection();
    QGroupBox* createLeadersSection();
    QGroupBox* createLogSection();
    QGroupBox* createMoveEvalSection();
    QGroupBox* createCombatSection();
    QGroupBox* createPurchaseSection();
    QWidget* createControlsSection();

    // Helper to format phase name
    QString phaseToString(AIPlayer::Phase phase);

    // Save/load window geometry
    void saveSettings();
    void loadSettings();

    // === UI Components - Header ===
    QLabel *m_playerLabel;
    QLabel *m_phaseLabel;

    // === UI Components - State Section ===
    QLabel *m_walletLabel;
    QLabel *m_territoriesLabel;
    QLabel *m_piecesLabel;

    // === UI Components - Leaders Section ===
    QListWidget *m_leadersListWidget;

    // === UI Components - Log Section ===
    QTextEdit *m_logTextEdit;
    QPushButton *m_clearLogButton;

    // === UI Components - Move Evaluation Section ===
    QLabel *m_evalLeaderLabel;
    QTableWidget *m_moveTable;

    // === UI Components - Combat Section ===
    QGroupBox *m_combatGroupBox;
    QLabel *m_combatTerritoryLabel;
    QLabel *m_myForcesLabel;
    QLabel *m_enemyForcesLabel;
    QProgressBar *m_winChanceBar;

    // === UI Components - Purchase Section ===
    QGroupBox *m_purchaseGroupBox;
    QLabel *m_budgetLabel;
    QListWidget *m_purchaseListWidget;
    QLabel *m_remainingLabel;

    // === UI Components - Controls ===
    QPushButton *m_stepButton;
    QPushButton *m_autoButton;
    QPushButton *m_pauseButton;
    QSlider *m_speedSlider;
    QLabel *m_speedLabel;
    QComboBox *m_strategyCombo;

    // Reference to AI player
    AIPlayer *m_aiPlayer = nullptr;
};

#endif // AIDEBUGWIDGET_H

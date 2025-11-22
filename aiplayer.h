#ifndef AIPLAYER_H
#define AIPLAYER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QString>
#include <functional>

class Player;
class PlayerInfoWidget;
class MapWidget;
class CombatDialog;
class PurchaseDialog;
class GamePiece;

// Structure for evaluating potential moves
struct MoveEvaluation {
    QString leaderName;           // "Caesar" or "General 1-6"
    QString fromTerritory;
    QString targetTerritory;
    int score;                    // Higher = better
    QString moveType;             // "Attack", "Expand", "Reinforce", "Defend", "Stay"
    QString reason;               // Human-readable explanation
    bool isSelected = false;      // True if this move was chosen
};

// Structure for purchase decisions
struct PurchaseDecision {
    int infantry = 0;
    int cavalry = 0;
    int catapults = 0;
    QStringList cityTerritories;
    QStringList fortifyTerritories;
    int galleys = 0;
    int totalCost = 0;
};

class AIPlayer : public QObject
{
    Q_OBJECT

public:
    enum class Strategy {
        Random,      // Random valid moves
        Aggressive,  // Prioritize attacking enemies
        Defensive,   // Prioritize defending territories
        Economic     // Prioritize building cities and income
    };
    Q_ENUM(Strategy)

    enum class Phase {
        Idle,
        ReadingState,
        Movement,
        Combat,
        CityDestruction,
        Purchase,
        TurnComplete
    };
    Q_ENUM(Phase)

    explicit AIPlayer(Player *player,
                      PlayerInfoWidget *infoWidget,
                      MapWidget *mapWidget,
                      QObject *parent = nullptr);
    ~AIPlayer();

    // Configuration
    void setStrategy(Strategy strategy);
    Strategy getStrategy() const { return m_strategy; }

    void setDelayMs(int ms);
    int getDelayMs() const { return m_delayMs; }

    void setStepMode(bool enabled);
    bool isStepMode() const { return m_stepMode; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // Get associated player
    Player* getPlayer() const { return m_player; }
    QChar getPlayerId() const;

    // Current phase
    Phase getCurrentPhase() const { return m_currentPhase; }

public slots:
    // Main entry point - called when it's this player's turn
    void executeTurn();

    // Step mode controls
    void step();              // Execute one action (when in step mode)
    void setAutoRun(bool on); // Toggle continuous execution

    // Handle dialogs that appear during turn
    void handleCombatDialog(CombatDialog *dialog);
    void handlePurchaseDialog(PurchaseDialog *dialog);

    // === Legion Building Logic (public for PlayerInfoWidget access) ===
    // Decides which troops a general should take when moving
    // Returns list of troop unique IDs that should be checked in the dialog
    QList<int> decideLegionComposition(GamePiece *general, const QList<GamePiece*> &availableTroops);

    // Check if a general can move (all troops in their legion must have moves remaining)
    bool canGeneralMove(GamePiece *general) const;

signals:
    // Notify when turn is complete
    void turnComplete();

    // Debug signals for AIDebugWidget
    void phaseChanged(AIPlayer::Phase phase);
    void stateUpdated(int wallet, int territoryCount, int pieceCount);
    void leadersUpdated(const QStringList &leaderDescriptions);
    void movesEvaluated(const QList<MoveEvaluation> &evaluations);
    void moveSelected(const MoveEvaluation &move);
    void actionTaken(const QString &timestamp, const QString &description);
    void combatAnalysisUpdated(const QString &territory,
                               int myForces, int enemyForces,
                               int myAdvantage, int enemyAdvantage,
                               int estimatedWinPercent);
    void purchasePlanUpdated(int budget, const PurchaseDecision &decision);

    // Request UI to wait (step mode)
    void waitingForStep();

private slots:
    void continueExecution();  // Called by timer for delayed execution

private:
    // === State Reading ===
    struct LeaderInfo {
        GamePiece *piece;
        QString name;
        QString territory;
        int movesRemaining;
        int legionSize;
    };

    struct GameState {
        int wallet;
        QStringList ownedTerritories;
        QStringList enemyTerritories;
        int totalPieces;
        QList<LeaderInfo> leaders;
    };

    GameState readGameState();
    void emitStateUpdate(const GameState &state);

    // === Phase Execution ===
    void setPhase(Phase phase);
    void executeReadingStatePhase();
    void executeMovementPhase();
    void executeCombatPhase();
    void executePurchasePhase();

    // === Movement Logic ===
    QList<MoveEvaluation> evaluateMovesForLeader(GamePiece *leader, const GameState &state);
    MoveEvaluation selectBestMove(const QList<MoveEvaluation> &moves);
    void performMove(GamePiece *leader, const QString &targetTerritory);
    bool hasMovesRemaining(const GameState &state);

    // === Combat Logic ===
    void executeCombatInDialog(CombatDialog *dialog);
    void selectAndClickTarget(CombatDialog *dialog, bool isAttacking);

    // === Purchase Logic ===
    PurchaseDecision decidePurchases(int budget);
    void executePurchaseDecision(PurchaseDialog *dialog, const PurchaseDecision &decision);

    // === End Turn ===
    void executeEndTurn();

    // === Scoring Helpers ===
    int scoreAttackMove(const QString &target, const GameState &state);
    int scoreExpandMove(const QString &target, const GameState &state);
    int scoreDefendMove(const QString &target, const GameState &state);
    int getEnemyStrengthAt(const QString &territory);
    int getMyStrengthAt(const QString &territory);
    bool isAdjacentToEnemy(const QString &territory);

    // === Utility ===
    void log(const QString &message);
    void scheduleNextAction(std::function<void()> action);
    QString currentTimestamp() const;

    // References
    Player *m_player;
    PlayerInfoWidget *m_infoWidget;
    MapWidget *m_mapWidget;

    // Configuration
    Strategy m_strategy = Strategy::Random;
    int m_delayMs = 500;
    bool m_stepMode = false;
    bool m_enabled = true;

    // State
    Phase m_currentPhase = Phase::Idle;
    GameState m_lastGameState;
    bool m_autoRun = false;
    bool m_waitingForStep = false;

    // Timer for delayed execution
    QTimer *m_actionTimer;

    // Pending action
    std::function<void()> m_pendingAction;
};

#endif // AIPLAYER_H

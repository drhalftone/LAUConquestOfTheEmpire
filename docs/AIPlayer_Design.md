# AIPlayer System Design

## Overview

This document describes the design for an AI-controlled player system that:
1. **Reads game state** from the UI widgets (PlayerInfoWidget, MapWidget)
2. **Interacts with widgets** programmatically (simulating user actions)
3. **Provides debug visibility** through a dedicated debug window

This approach ensures that the AI exercises the full UI code path, making it ideal for automated testing.

---

## Goals

- **Test the UI**: AI actions go through the same code paths as human players
- **Debug visibility**: See what the AI is "thinking" in real-time
- **Configurable**: Adjustable speed, strategy, and step-by-step mode
- **Non-invasive**: Minimal changes to existing classes

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              Main Application                            │
│  ┌─────────────┐    ┌──────────────────┐    ┌─────────────────────┐     │
│  │   Player    │◄───│    AIPlayer      │───►│   AIDebugWidget     │     │
│  │  (data)     │    │  (controller)    │    │   (debug view)      │     │
│  └─────────────┘    └────────┬─────────┘    └─────────────────────┘     │
│                              │                                           │
│                              │ interacts with                            │
│                              ▼                                           │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                        UI Widgets                                 │   │
│  │  ┌─────────────────┐  ┌─────────────┐  ┌───────────────────┐     │   │
│  │  │PlayerInfoWidget │  │  MapWidget  │  │  CombatDialog     │     │   │
│  │  │ - end turn btn  │  │ - board     │  │  PurchaseDialog   │     │   │
│  │  │ - move methods  │  │ - state     │  │  (modal dialogs)  │     │   │
│  │  └─────────────────┘  └─────────────┘  └───────────────────┘     │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

### Data Flow

```
1. Turn starts → AIPlayer::executeTurn() called
2. AIPlayer reads state from Player/MapWidget
3. AIPlayer emits debug signals → AIDebugWidget displays
4. AIPlayer decides action based on strategy
5. AIPlayer calls PlayerInfoWidget methods to execute action
6. UI updates → AIPlayer reads new state
7. Repeat until turn complete
8. AIPlayer calls endTurn() → next player's turn
```

---

## Class Designs

### 1. AIPlayer Class

The main controller that makes decisions and interacts with UI widgets.

#### Header: `aiplayer.h`

```cpp
#ifndef AIPLAYER_H
#define AIPLAYER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QString>

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
    void setStrategy(Strategy strategy) { m_strategy = strategy; }
    Strategy getStrategy() const { return m_strategy; }

    void setDelayMs(int ms) { m_delayMs = ms; }
    int getDelayMs() const { return m_delayMs; }

    void setStepMode(bool enabled) { m_stepMode = enabled; }
    bool isStepMode() const { return m_stepMode; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // Get associated player
    Player* getPlayer() const { return m_player; }

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
    struct GameState {
        int wallet;
        QStringList ownedTerritories;
        QStringList enemyTerritories;
        int totalPieces;

        struct LeaderInfo {
            GamePiece *piece;
            QString name;
            QString territory;
            int movesRemaining;
            int legionSize;
        };
        QList<LeaderInfo> leaders;
    };

    GameState readGameState();
    void emitStateUpdate(const GameState &state);

    // === Movement Phase ===
    void executeMovementPhase();
    QList<MoveEvaluation> evaluateMovesForLeader(GamePiece *leader, const GameState &state);
    MoveEvaluation selectBestMove(const QList<MoveEvaluation> &moves);
    void performMove(GamePiece *leader, const QString &targetTerritory);
    bool hasMovesRemaining(const GameState &state);

    // === Combat Phase ===
    void executeCombatInDialog(CombatDialog *dialog);
    void selectAndClickTarget(CombatDialog *dialog, bool isAttacking);

    // === Purchase Phase ===
    PurchaseDecision decidePurchases(int budget,
                                     const QList<struct CityPlacementOption> &cityOptions,
                                     const QList<struct FortificationOption> &fortOptions,
                                     int availableInfantry,
                                     int availableCavalry,
                                     int availableCatapults);
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
    void scheduleNextAction();

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
    bool m_autoRun = false;
    bool m_waitingForStep = false;

    // Timer for delayed execution
    QTimer *m_actionTimer;

    // Pending actions queue
    std::function<void()> m_pendingAction;
};

#endif // AIPLAYER_H
```

---

### 2. AIDebugWidget Class

A window that displays the AI's internal state and decision-making process.

#### Header: `aidebugwidget.h`

```cpp
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
```

---

### 3. AIDebugWidget Visual Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│ AI Debug - Player A                                              [_][X] │
├─────────────────────────────────────────────────────────────────────────┤
│ Phase: [Movement        ]                                               │
├───────────────────────────────┬─────────────────────────────────────────┤
│ ┌─ Current State ───────────┐ │ ┌─ Action Log ────────────────────────┐ │
│ │ Wallet:      45 talents   │ │ │ 12:34:01 | Turn started             │ │
│ │ Territories: 4            │ │ │ 12:34:01 | Reading game state...    │ │
│ │ Pieces:      12           │ │ │ 12:34:01 | Found 3 leaders w/moves  │ │
│ └───────────────────────────┘ │ │ 12:34:02 | Evaluating Caesar moves  │ │
│ ┌─ Leaders ─────────────────┐ │ │ 12:34:02 | Best: Roma (score 85)    │ │
│ │ • Caesar @ Italia (2 mv)  │ │ │ 12:34:03 | Moving Caesar → Roma     │ │
│ │   Legion: 4 troops        │ │ │ 12:34:03 | Combat triggered!        │ │
│ │ • General 1 @ Gallia (2)  │ │ │ 12:34:04 | Attacking infantry...    │ │
│ │   Legion: 2 troops        │ │ │                                     │ │
│ │ • General 2 @ Roma (0)    │ │ │                          [Clear Log]│ │
│ │   Legion: 0 troops        │ │ └─────────────────────────────────────┘ │
│ └───────────────────────────┘ │                                         │
├───────────────────────────────┴─────────────────────────────────────────┤
│ ┌─ Move Evaluation ───────────────────────────────────────────────────┐ │
│ │ Evaluating: Caesar @ Italia                                         │ │
│ │ ┌──────────────┬───────┬───────────┬─────────────────────────────┐  │ │
│ │ │ Target       │ Score │ Type      │ Reason                      │  │ │
│ │ ├──────────────┼───────┼───────────┼─────────────────────────────┤  │ │
│ │ │ ► Roma       │  85   │ Attack    │ 3 enemy pieces, favorable   │  │ │
│ │ │   Sicilia    │  62   │ Expand    │ Unclaimed, value 10         │  │ │
│ │ │   Gallia     │  45   │ Reinforce │ Own territory, low defense  │  │ │
│ │ │   (stay)     │  20   │ Defend    │ No immediate threats        │  │ │
│ │ └──────────────┴───────┴───────────┴─────────────────────────────┘  │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
├───────────────────────────────┬─────────────────────────────────────────┤
│ ┌─ Combat Analysis ─────────┐ │ ┌─ Purchase Plan ─────────────────────┐ │
│ │ Territory: Roma           │ │ │ Budget: 45 talents                  │ │
│ │ My forces:    5 (+1 adv)  │ │ │ ┌─────────────────────────────────┐ │ │
│ │ Enemy forces: 3 (+0 adv)  │ │ │ │ • 2x Infantry     20            │ │ │
│ │                           │ │ │ │ • 1x Cavalry      20            │ │ │
│ │ Est. Win: ████████░░ 70%  │ │ │ │                                 │ │ │
│ │                           │ │ │ └─────────────────────────────────┘ │ │
│ │ Strategy: Target infantry │ │ │ Remaining: 5 talents                │ │
│ └───────────────────────────┘ │ └─────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────────┤
│ [► Step] [►► Auto] [⏸ Pause]  Speed: [████████░░] 500ms   Strategy: [▼]│
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Integration Points

### 4. Changes to PlayerInfoWidget

Add methods to allow programmatic movement:

```cpp
// Add to playerinfowidget.h - public section:

    // === AI Integration ===
    // Programmatically move a leader to a territory (same as context menu action)
    bool moveLeaderToTerritoryAI(GamePiece *leader, const QString &targetTerritory);

    // Get the end turn button for programmatic clicking
    QPushButton* getEndTurnButton() const { return m_endTurnButton; }

    // Trigger end turn programmatically
    void triggerEndTurn() { onEndTurnClicked(); }

    // Get list of valid move targets for a leader
    QStringList getValidMoveTargets(GamePiece *leader) const;

// Add to playerinfowidget.h - private section:
    QPushButton *m_endTurnButton;  // Store reference to end turn button
```

### 5. Changes to CombatDialog

Add methods to allow AI to interact with combat:

```cpp
// Add to combatdialog.h - public section:

    // === AI Integration ===
    // Get all targetable buttons for attacker
    QList<QPushButton*> getAttackerTargetButtons() const;

    // Get all targetable buttons for defender
    QList<QPushButton*> getDefenderTargetButtons() const;

    // Check if combat is still ongoing
    bool isCombatOngoing() const;

    // Check whose turn it is
    bool isAttackersTurn() const { return m_isAttackersTurn; }

    // Programmatically click a target button
    void clickTargetButton(QPushButton *button);

    // Programmatically retreat
    void clickRetreat() { onRetreatClicked(); }

    // Get troop counts for analysis
    int getAttackerTroopCount() const { return m_attackingTroopButtons.size(); }
    int getDefenderTroopCount() const { return m_defendingTroopButtons.size(); }
```

### 6. Changes to PurchaseDialog

Add methods to allow AI to make purchases:

```cpp
// Add to purchasedialog.h - public section:

    // === AI Integration ===
    // Set troop purchase quantities
    void setInfantryCount(int count);
    void setCavalryCount(int count);
    void setCatapultCount(int count);

    // Set city placement (by territory name)
    void selectCityPlacement(const QString &territoryName, bool fortified);

    // Set fortification for existing city
    void selectFortification(const QString &territoryName);

    // Set galley purchase at sea territory
    void setGalleyCount(const QString &seaTerritoryName, int count);

    // Confirm and close dialog
    void confirmPurchase() { onPurchaseClicked(); }

    // Get current spending
    int getCurrentSpending() const { return m_totalSpent; }
```

---

## Integration in main.cpp

```cpp
#include "aiplayer.h"
#include "aidebugwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // ... existing setup code ...

    // Create players (existing)
    QList<Player*> players;
    // ... player creation ...

    // Create AI players for testing
    QList<AIPlayer*> aiPlayers;
    QList<AIDebugWidget*> debugWidgets;

    for (int i = 0; i < players.size(); ++i) {
        // Create AI controller for each player
        AIPlayer *ai = new AIPlayer(players[i], playerInfoWidget, mapWidget);
        ai->setStrategy(AIPlayer::Strategy::Random);  // or Aggressive, etc.
        ai->setDelayMs(500);  // 500ms between actions for visibility
        aiPlayers.append(ai);

        // Create debug widget for each AI
        AIDebugWidget *debugWidget = new AIDebugWidget();
        debugWidget->setAIPlayer(ai);
        debugWidget->setWindowTitle(QString("AI Debug - Player %1").arg(players[i]->getId()));
        debugWidget->show();
        debugWidgets.append(debugWidget);
    }

    // Connect turn signals to AI
    // When a player's turn starts, trigger their AI
    for (int i = 0; i < players.size(); ++i) {
        QObject::connect(players[i], &Player::turnStarted,
                         aiPlayers[i], &AIPlayer::executeTurn);
    }

    // ... rest of main ...
}
```

---

## Implementation Plan

### Phase 1: Core Structure (Files to Create)

| File | Description |
|------|-------------|
| `aiplayer.h` | AIPlayer class header |
| `aiplayer.cpp` | AIPlayer implementation |
| `aidebugwidget.h` | Debug widget header |
| `aidebugwidget.cpp` | Debug widget implementation |

### Phase 2: Widget Integration (Files to Modify)

| File | Changes |
|------|---------|
| `playerinfowidget.h/cpp` | Add AI movement methods |
| `combatdialog.h/cpp` | Add AI interaction methods |
| `purchasedialog.h/cpp` | Add AI purchase methods |
| `LAUConquestOfTheEmpire.pro` | Add new source files |
| `main.cpp` | Wire up AI players |

### Implementation Order

1. **Create `aiplayer.h`** - Define the class structure and signals
2. **Create `aidebugwidget.h`** - Define the debug widget structure
3. **Create `aidebugwidget.cpp`** - Implement the UI layout
4. **Create `aiplayer.cpp`** - Implement basic state reading and logging
5. **Modify `playerinfowidget`** - Add `moveLeaderToTerritoryAI()` method
6. **Implement movement phase** in AIPlayer
7. **Modify `combatdialog`** - Add AI interaction methods
8. **Implement combat phase** in AIPlayer
9. **Modify `purchasedialog`** - Add AI interaction methods
10. **Implement purchase phase** in AIPlayer
11. **Update `main.cpp`** - Create and wire up AI players
12. **Update `.pro` file** - Add new source files
13. **Test and iterate**

---

## AI Strategies

### Random Strategy
- Picks random valid moves
- Good for basic testing / fuzzing
- No scoring, just random selection

### Aggressive Strategy
Scoring priorities:
1. Attack enemy territories (+100 base, +20 per enemy piece)
2. Capture undefended territories (+60)
3. Reinforce frontline (+40)
4. Build troops over cities

### Defensive Strategy
Scoring priorities:
1. Reinforce territories adjacent to enemies (+80)
2. Fortify cities (+60)
3. Expand to buffer zones (+40)
4. Build cities over troops

### Economic Strategy
Scoring priorities:
1. Capture high-value territories (+value * 10)
2. Build cities in owned territories (+70)
3. Build roads between cities (+50)
4. Minimal troop purchases

---

## Testing Scenarios

### Basic Turn Test
1. Start game with 2 AI players
2. Both set to Random strategy
3. Run for 10 turns
4. Verify no crashes, UI updates correctly

### Combat Test
1. Manually position pieces for guaranteed combat
2. Run AI turn
3. Verify combat dialog opens and resolves

### Purchase Test
1. Give AI player large wallet
2. End movement phase
3. Verify purchase dialog selections make sense

### Full Game Test
1. Run automated game to completion
2. Verify victory condition detected
3. Check for any memory leaks or UI glitches

---

## Future Enhancements

- **Save/Load AI state** for reproducible testing
- **Replay mode** to step through recorded decisions
- **Performance metrics** (territories/turn, combat win rate)
- **Multiple AI difficulties** (Easy/Medium/Hard)
- **Neural network integration** for learned strategies

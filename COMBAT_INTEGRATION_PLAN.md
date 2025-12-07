# Combat Integration Plan

## Overview

This document outlines the plan to integrate the `CombatDialog` into the main game's movement system. The Quick Battle mini-game has already validated the `AIPlayer` ↔ `CombatDialog` interface.

## Current State

### What Works
- `moveLeaderToTerritory()` in `playerinfowidget.cpp:1737` detects when moving into combat via `hasEnemies` and `movingIntoCombat` flags
- `CombatDialog` is fully functional with rolling dice, target selection, retreat, and result handling
- `AIPlayer::selectCombatTarget()` is implemented and prioritizes catapults
- AI vs AI combat works in Quick Battle

### What's Missing
- Combat is **never triggered** - pieces move and claim territory without fighting
- `AIPlayer::handleCombatDialog()` is a stub (line 698 in `aiplayer.cpp`)
- `AIPlayer::executeCombatPhase()` is a stub
- No way to identify the defender's AIPlayer instance for AI vs AI battles

## Implementation Steps

### Step 1: Add Helper to Find Defender

Add to `playerinfowidget.h`:
```cpp
private:
    Player* findDefenderAtTerritory(const QString &territory, Player *attacker);
```

Add to `playerinfowidget.cpp`:
```cpp
Player* PlayerInfoWidget::findDefenderAtTerritory(const QString &territory, Player *attacker)
{
    for (Player *player : m_players) {
        if (player != attacker && player->getId() != attacker->getId()) {
            QList<GamePiece*> pieces = player->getPiecesAtTerritory(territory);
            if (!pieces.isEmpty()) {
                return player;
            }
        }
    }
    return nullptr;
}
```

### Step 2: Store AIPlayer References for All Players

Currently `m_aiPlayer` only stores the current player's AI. For AI vs AI combat, we need access to both.

Option A: Store a map of player ID to AIPlayer:
```cpp
// In playerinfowidget.h
QMap<QChar, AIPlayer*> m_aiPlayers;

// Accessor
AIPlayer* getAIPlayerFor(QChar playerId) const;
```

Option B: Store AIPlayer pointer in Player class itself (cleaner):
```cpp
// In player.h
private:
    AIPlayer *m_aiPlayer = nullptr;
public:
    void setAIPlayer(AIPlayer *ai) { m_aiPlayer = ai; }
    AIPlayer* getAIPlayer() const { return m_aiPlayer; }
```

### Step 3: Trigger Combat in moveLeaderToTerritory

In `playerinfowidget.cpp`, after moving pieces (around line 1968), add combat trigger:

```cpp
// After: owningPlayer->claimTerritory(destinationTerritory);
// Add combat handling:

if (movingIntoCombat) {
    Player *defender = findDefenderAtTerritory(destinationTerritory, owningPlayer);
    if (defender) {
        qDebug() << "COMBAT! Attacker:" << owningPlayer->getId()
                 << "vs Defender:" << defender->getId()
                 << "at" << destinationTerritory;

        // Create combat dialog
        CombatDialog combatDialog(owningPlayer, defender, destinationTerritory, m_mapWidget, this);

        // Set up AI players if applicable
        AIPlayer *attackerAI = owningPlayer->getAIPlayer();  // or m_aiPlayers.value(owningPlayer->getId())
        AIPlayer *defenderAI = defender->getAIPlayer();

        if (attackerAI || defenderAI) {
            combatDialog.setupAIPlayers(attackerAI, defenderAI);
        }

        combatDialog.exec();

        // Handle result
        handleCombatResult(combatDialog.getCombatResult(), owningPlayer, defender,
                          leader, destinationTerritory, currentTerritory);
    }
}
```

### Step 4: Handle Combat Results

Add new method to `playerinfowidget.cpp`:

```cpp
void PlayerInfoWidget::handleCombatResult(CombatDialog::CombatResult result,
                                          Player *attacker, Player *defender,
                                          GamePiece *attackingLeader,
                                          const QString &combatTerritory,
                                          const QString &retreatTerritory)
{
    switch (result) {
        case CombatDialog::CombatResult::AttackerWins:
            // Territory already claimed during move
            // CombatDialog already removed defender's pieces
            qDebug() << "Attacker wins at" << combatTerritory;
            break;

        case CombatDialog::CombatResult::DefenderWins:
            // Attacker's pieces already removed by CombatDialog
            // Unclaim territory for attacker, restore for defender
            attacker->unclaimTerritory(combatTerritory);
            defender->claimTerritory(combatTerritory);
            qDebug() << "Defender wins at" << combatTerritory;
            break;

        case CombatDialog::CombatResult::AttackerRetreats:
            // Move attacker back to previous territory
            retreatAttacker(attackingLeader, retreatTerritory);
            attacker->unclaimTerritory(combatTerritory);
            defender->claimTerritory(combatTerritory);
            qDebug() << "Attacker retreats from" << combatTerritory << "to" << retreatTerritory;
            break;
    }

    // Refresh displays
    updateAllPlayers();
    if (m_mapWidget) {
        m_mapWidget->update();
    }
}
```

### Step 5: Implement Retreat Logic

```cpp
void PlayerInfoWidget::retreatAttacker(GamePiece *leader, const QString &retreatTerritory)
{
    if (!leader || !m_mapWidget) return;

    Position retreatPos = m_mapWidget->territoryNameToPosition(retreatTerritory);

    // Move leader back
    leader->setTerritoryName(retreatTerritory);
    leader->setPosition(retreatPos);

    // Move all troops in leader's legion back
    QList<int> legionIds;
    if (leader->getType() == GamePiece::Type::Caesar) {
        legionIds = static_cast<CaesarPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::General) {
        legionIds = static_cast<GeneralPiece*>(leader)->getLegion();
    }

    Player *owner = nullptr;
    for (Player *p : m_players) {
        if (p->getId() == leader->getPlayer()) {
            owner = p;
            break;
        }
    }

    if (owner) {
        for (GamePiece *piece : owner->getAllPieces()) {
            if (legionIds.contains(piece->getUniqueId())) {
                piece->setTerritoryName(retreatTerritory);
                piece->setPosition(retreatPos);
            }
        }
    }
}
```

### Step 6: Update AIPlayer Stubs

In `aiplayer.cpp`, update `handleCombatDialog()`:

```cpp
void AIPlayer::handleCombatDialog(CombatDialog *dialog)
{
    if (!dialog) return;
    log("AI handling combat dialog");
    // Combat is handled automatically via selectCombatTarget()
    // which CombatDialog calls during makeAIMove()
}
```

The `executeCombatPhase()` can be removed or left as a no-op since combat happens during movement.

## Testing Checklist

- [ ] Human attacker vs Human defender
- [ ] Human attacker vs AI defender
- [ ] AI attacker vs Human defender
- [ ] AI attacker vs AI defender
- [ ] Attacker wins - territory changes hands
- [ ] Defender wins - territory stays with defender
- [ ] Attacker retreats - pieces return to previous territory
- [ ] Caesar capture/kill dialog works
- [ ] General capture/kill dialog works
- [ ] City/fortification bonuses apply correctly
- [ ] Catapult bonuses apply correctly

## Files to Modify

| File | Changes |
|------|---------|
| `playerinfowidget.h` | Add `findDefenderAtTerritory()`, `handleCombatResult()`, `retreatAttacker()` declarations |
| `playerinfowidget.cpp` | Implement above methods, add combat trigger in `moveLeaderToTerritory()` |
| `player.h` | Add `m_aiPlayer` member and accessors (Option B) |
| `player.cpp` | Initialize `m_aiPlayer = nullptr` |
| `main.cpp` | Set AIPlayer on Player instances after creation |
| `aiplayer.cpp` | Update `handleCombatDialog()` stub |

## Notes

- The `lastTerritory` is already stored before movement (line 1939-1945), so retreat has a valid destination
- `CombatDialog` already handles piece removal during combat
- The rolling die widget issue is fixed (closes with dialog)
- AI target selection prioritizes catapults for the +1 bonus

## Related Files

- `quickbattle_main.cpp` - Reference implementation for combat flow
- `combatdialog.cpp` - Combat UI and resolution logic
- `aiplayer.cpp` - AI decision making
- `ai/combatsimulator.cpp` - Monte Carlo win probability simulation

---

## Combat Simulator (December 2024)

### Overview

The `CombatSimulator` class provides Monte Carlo simulation to predict combat outcomes based on army composition. It is integrated into both the `CombatDialog` (for real-time odds display) and `QuickBattle` mini-game.

### Features

1. **Monte Carlo Win Probability**: Runs 1000 simulations to estimate win chances
2. **Real-time Odds Display**: Shows "Win: XX%" for both attacker and defender in the combat dialog header
3. **Dynamic Updates**: Odds recalculate after each casualty
4. **AI Retreat Logic**: AI attacker retreats when win chance falls below 20%
5. **Galley Support**: Simulates naval combat with proper targeting order (troops before galleys)

### Hit Probabilities (d6 + advantage)

| Target | Threshold | Base Hit Probability | Expected Rolls |
|--------|-----------|---------------------|----------------|
| Infantry | 4+ | 50% (3/6) | 2 |
| Cavalry | 5+ | 33% (2/6) | 3 |
| Catapult | 6+ | 17% (1/6) | 6 |
| Galley | 3+ | 67% (4/6) | 1.5 |

**Note**: Galleys ignore the advantage modifier.

### Advantage Calculation

- Each catapult provides +1 advantage
- Defender in fortified city gets +1 advantage
- Net advantage = max(0, attacker_advantage - defender_advantage)

### Targeting Priority

The simulation uses optimal targeting order:
1. **Catapults first** - Removes enemy advantage
2. **Cavalry second** - Higher economic damage per roll (cost/rolls)
3. **Infantry third** - Easiest to kill but lowest value
4. **Galleys last** - Only targetable after all troops eliminated (sea combat only)

### Integration Points

#### CombatDialog (`combatdialog.cpp`)

```cpp
// Member variables
CombatSimulator m_combatSimulator;
QLabel *m_attackerOddsLabel;
QLabel *m_defenderOddsLabel;

// Called after each casualty via updateAdvantageDisplay()
void updateWinProbabilityDisplay();
```

The odds labels are color-coded:
- **Green (≥60%)** - Favorable odds
- **Yellow (40-60%)** - Even odds
- **Red (<40%)** - Unfavorable odds

#### AI Retreat Logic (`combatdialog.cpp:1066-1075`)

```cpp
if (m_attackerIsAI) {
    CombatProbability prob = m_combatSimulator.calculateWinProbability(1000);
    bool canRetreat = m_retreatButton && m_retreatButton->isEnabled();
    if (canRetreat && prob.attackerWinChance < 0.20) {
        QTimer::singleShot(m_aiDelayMs, this, &CombatDialog::onRetreatClicked);
    } else {
        QTimer::singleShot(m_aiDelayMs, this, &CombatDialog::makeAIMove);
    }
}
```

#### QuickBattle (`quickbattle_main.cpp`)

Pre-battle odds are displayed in the "Battle Starting" message box:

```cpp
CombatSimulator simulator;
simulator.initializeBattle(attackerArmy, defenderArmy, terrain);
CombatProbability prob = simulator.calculateWinProbability(1000);
```

### Quick Battle Mode

The `CombatDialog` has a `setQuickBattleMode(true)` option that:
- Skips the Caesar capture/takeover dialog
- Useful for isolated combat testing without game-wide side effects

### Files Modified

| File | Changes |
|------|---------|
| `ai/combatsimulator.h` | Added `galleys` field to `ArmyComposition`, `isSeaCombat` to `CombatTerrain` |
| `ai/combatsimulator.cpp` | Implemented full Monte Carlo simulation with galley support |
| `combatdialog.h` | Added `CombatSimulator`, odds labels, `setQuickBattleMode()` |
| `combatdialog.cpp` | Added `updateWinProbabilityDisplay()`, AI retreat logic |
| `quickbattle_main.cpp` | Added pre-battle odds display, quick battle mode |
| `purchasedialog.cpp` | Fixed combat-only mode to enable troop purchases |
| `QuickBattle.pro` | Added missing source files for AI modules |

### MATLAB Validation Script

A MATLAB script (`combat_simulation.m`) was created to validate the Monte Carlo results:

```matlab
% Simulates battles to compare catapult vs infantry/cavalry effectiveness
% Results confirm targeting priority: Catapults > Cavalry > Infantry
```

---

## AI Attack Decision Win Probability (December 2024)

### Overview

The AI decision-making system now incorporates win probability calculations to make smarter attack decisions. The AI will only attack territories where it has at least a 50% chance of winning.

### Key Features

1. **Single-General Attack Validation**: Each individual attack move is scored with win probability
2. **Multi-General Attack Planning**: Combined force projection is considered when planning coordinated attacks
3. **Dynamic Scoring**: Attack targets receive score bonuses/penalties based on win probability

### Implementation Details

#### Win Probability Helper (`ai/aidecisionmaker.cpp`)

```cpp
double AIDecisionMaker::calculateAttackWinProbability(
    int attackerInfantry, int attackerCavalry, int attackerCatapults,
    const QString &territory,
    Player *player,
    const QList<Player*> &allPlayers);
```

- Counts defender units and checks for fortified city
- Runs 500 Monte Carlo simulations (reduced from 1000 for speed)
- Returns probability from 0.0 to 1.0

#### Single-General Attacks (`scoreMove()`)

When scoring an attack on a defended territory:

| Win Probability | Score Adjustment | Behavior |
|-----------------|------------------|----------|
| < 50% | -500 penalty | Effectively invalidates the attack |
| 50-70% | No adjustment | Acceptable odds |
| ≥ 70% | +50 bonus | Encouraged attack |

#### Multi-General Attacks (`identifyTargets()`)

When identifying attack targets, the system:
1. Uses heat map data (`ourForce1Turn`) to get total force that can reach target
2. Estimates unit breakdown proportionally based on player's army composition
3. Calculates combined win probability

**Target Filtering:**
- < 50% combined win probability → Target is **not added** to attack list
- ≥ 50% → Target is added, allowing multiple generals to be assigned

**Score Adjustments:**
| Win Probability | Score Adjustment |
|-----------------|------------------|
| ≥ 80% | +80 bonus |
| ≥ 70% | +40 bonus |
| < 60% | -30 penalty |

### How Multi-General Attacks Work

The system allows coordinated multi-general attacks through this flow:

1. **Target Identification Phase** (`identifyTargets`):
   - Calculate max combined force that can reach each enemy territory
   - Only add territory as "Attack" target if combined force gives ≥50% win

2. **Movement Planning Phase** (`planMovement`):
   - Multiple generals can be assigned to the same validated target
   - Troops are allocated proportionally based on target priority

3. **Individual Move Scoring** (`scoreMove`):
   - Each general's move is still scored individually
   - Low win probability attacks get heavy penalties
   - But since target was already validated at planning level, coordinated attacks proceed

### Example Scenario

**Enemy has 4 infantry at Territory X**

- General A has 2 infantry → ~30% win probability (too low alone)
- General B has 3 infantry → ~45% win probability (too low alone)
- Combined: 5 infantry vs 4 → ~65% win probability (valid attack)

Result: Territory X is added as an Attack target, and both generals can be assigned to it.

### Files Modified

| File | Changes |
|------|---------|
| `ai/aidecisionmaker.h` | Added `calculateAttackWinProbability()` declaration |
| `ai/aidecisionmaker.cpp` | Added `#include "combatsimulator.h"`, implemented win probability helper, modified `scoreMove()` and `identifyTargets()` |

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

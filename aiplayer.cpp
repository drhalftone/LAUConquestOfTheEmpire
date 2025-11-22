#include "aiplayer.h"
#include "player.h"
#include "playerinfowidget.h"
#include "mapwidget.h"
#include "gamepiece.h"
#include <QDebug>
#include <QTime>
#include <QTimer>
#include <QRandomGenerator>
#include <QSet>

AIPlayer::AIPlayer(Player *player,
                   PlayerInfoWidget *infoWidget,
                   MapWidget *mapWidget,
                   QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_infoWidget(infoWidget)
    , m_mapWidget(mapWidget)
    , m_actionTimer(new QTimer(this))
{
    m_actionTimer->setSingleShot(true);
    connect(m_actionTimer, &QTimer::timeout, this, &AIPlayer::continueExecution);

    log(QString("AIPlayer created for Player %1").arg(getPlayerId()));
}

AIPlayer::~AIPlayer()
{
    log("AIPlayer destroyed");
}

QChar AIPlayer::getPlayerId() const
{
    return m_player ? m_player->getId() : '?';
}

void AIPlayer::setStrategy(Strategy strategy)
{
    m_strategy = strategy;
    QString strategyName;
    switch (strategy) {
        case Strategy::Random: strategyName = "Random"; break;
        case Strategy::Aggressive: strategyName = "Aggressive"; break;
        case Strategy::Defensive: strategyName = "Defensive"; break;
        case Strategy::Economic: strategyName = "Economic"; break;
    }
    log(QString("Strategy set to: %1").arg(strategyName));
}

void AIPlayer::setDelayMs(int ms)
{
    m_delayMs = qMax(0, ms);
    log(QString("Delay set to: %1ms").arg(m_delayMs));
}

void AIPlayer::setStepMode(bool enabled)
{
    m_stepMode = enabled;
    log(QString("Step mode: %1").arg(enabled ? "ON" : "OFF"));
}

void AIPlayer::setPhase(Phase phase)
{
    m_currentPhase = phase;
    emit phaseChanged(phase);

    QString phaseName;
    switch (phase) {
        case Phase::Idle: phaseName = "Idle"; break;
        case Phase::ReadingState: phaseName = "Reading State"; break;
        case Phase::Movement: phaseName = "Movement"; break;
        case Phase::Combat: phaseName = "Combat"; break;
        case Phase::CityDestruction: phaseName = "City Destruction"; break;
        case Phase::Purchase: phaseName = "Purchase"; break;
        case Phase::TurnComplete: phaseName = "Turn Complete"; break;
    }
    log(QString("Phase changed to: %1").arg(phaseName));
}

QString AIPlayer::currentTimestamp() const
{
    return QTime::currentTime().toString("hh:mm:ss");
}

void AIPlayer::log(const QString &message)
{
    QString timestamp = currentTimestamp();
    qDebug() << "[AIPlayer" << getPlayerId() << "]" << timestamp << message;
    emit actionTaken(timestamp, message);
}

void AIPlayer::scheduleNextAction(std::function<void()> action)
{
    m_pendingAction = action;

    if (m_stepMode && !m_autoRun) {
        m_waitingForStep = true;
        emit waitingForStep();
        log("Waiting for step...");
    } else {
        m_actionTimer->start(m_delayMs);
    }
}

void AIPlayer::continueExecution()
{
    if (m_pendingAction) {
        auto action = m_pendingAction;
        m_pendingAction = nullptr;
        action();
    }
}

void AIPlayer::step()
{
    if (m_waitingForStep && m_pendingAction) {
        m_waitingForStep = false;
        log("Step executed");
        continueExecution();
    }
}

void AIPlayer::setAutoRun(bool on)
{
    m_autoRun = on;
    log(QString("Auto-run: %1").arg(on ? "ON" : "OFF"));

    if (on && m_waitingForStep) {
        m_waitingForStep = false;
        m_actionTimer->start(m_delayMs);
    }
}

// ============================================================================
// Main Turn Execution
// ============================================================================

void AIPlayer::executeTurn()
{
    if (!m_enabled) {
        log("AI is disabled, skipping turn");
        return;
    }

    if (!m_player || !m_player->isMyTurn()) {
        log("Not my turn, skipping");
        return;
    }

    // Enable AI auto-mode to skip dialogs during this turn
    if (m_infoWidget) {
        m_infoWidget->setAIAutoMode(true, m_delayMs);
        m_infoWidget->setAIPlayer(this);  // Set reference for legion composition decisions
    }

    log("=== TURN STARTED ===");
    setPhase(Phase::ReadingState);

    scheduleNextAction([this]() {
        executeReadingStatePhase();
    });
}

void AIPlayer::executeReadingStatePhase()
{
    log("Reading game state...");

    m_lastGameState = readGameState();
    emitStateUpdate(m_lastGameState);

    log(QString("State: Wallet=%1, Territories=%2, Pieces=%3, Leaders=%4")
        .arg(m_lastGameState.wallet)
        .arg(m_lastGameState.ownedTerritories.size())
        .arg(m_lastGameState.totalPieces)
        .arg(m_lastGameState.leaders.size()));

    // Move to movement phase
    setPhase(Phase::Movement);
    scheduleNextAction([this]() {
        executeMovementPhase();
    });
}

void AIPlayer::executeMovementPhase()
{
    log("Executing movement phase...");

    // Re-read state to get current leader positions
    m_lastGameState = readGameState();

    // Find GENERALS (not Caesars) with moves remaining AND whose legion can move
    QList<LeaderInfo> generalsWithMoves;
    for (const LeaderInfo &leader : m_lastGameState.leaders) {
        if (leader.movesRemaining > 0 &&
            leader.piece &&
            leader.piece->getType() == GamePiece::Type::General) {
            // Also check if the general's legion can move (all troops have moves)
            if (canGeneralMove(leader.piece)) {
                generalsWithMoves.append(leader);
            } else {
                log(QString("%1: Skipped - troops in legion have no moves remaining").arg(leader.name));
            }
        }
    }

    if (generalsWithMoves.isEmpty()) {
        log("No generals with moves remaining (or all blocked by legion constraints)");
        // End turn - this will trigger combat detection, taxes, purchases
        executeEndTurn();
        return;
    }

    log(QString("Found %1 general(s) that can move").arg(generalsWithMoves.size()));

    // Process one general at a time (schedule next move after this one completes)
    LeaderInfo &general = generalsWithMoves.first();

    QList<MoveEvaluation> moves = evaluateMovesForLeader(general.piece, m_lastGameState);
    emit movesEvaluated(moves);

    if (moves.isEmpty()) {
        log(QString("%1: No moves available, skipping").arg(general.name));
        scheduleNextAction([this]() {
            executeMovementPhase();
        });
        return;
    }

    // Select a random move (no combat filtering - single player test mode)
    MoveEvaluation selectedMove = selectBestMove(moves);
    emit moveSelected(selectedMove);

    log(QString("Selected move for %1: %2 -> %3 (%4)")
        .arg(general.name)
        .arg(selectedMove.fromTerritory)
        .arg(selectedMove.targetTerritory)
        .arg(selectedMove.moveType));

    // Skip if "Stay" was selected
    if (selectedMove.targetTerritory == selectedMove.fromTerritory) {
        log(QString("%1: Staying at %2").arg(general.name).arg(selectedMove.fromTerritory));
        scheduleNextAction([this]() {
            executeMovementPhase();
        });
        return;
    }

    // Actually perform the move!
    bool moveSuccess = m_infoWidget->aiMoveLeaderToTerritory(general.piece, selectedMove.targetTerritory);

    if (moveSuccess) {
        log(QString("%1: Moved to %2 successfully!").arg(general.name).arg(selectedMove.targetTerritory));
    } else {
        log(QString("%1: Move to %2 FAILED").arg(general.name).arg(selectedMove.targetTerritory));
    }

    // Schedule next movement check (there may be more moves remaining)
    scheduleNextAction([this]() {
        executeMovementPhase();
    });
}

void AIPlayer::executeEndTurn()
{
    log("Ending turn...");
    setPhase(Phase::TurnComplete);

    if (!m_infoWidget) {
        log("ERROR: No PlayerInfoWidget reference, cannot end turn");
        emit turnComplete();
        return;
    }

    // Enable AI auto-mode to auto-dismiss dialogs
    m_infoWidget->setAIAutoMode(true, m_delayMs);
    m_infoWidget->setAIPlayer(this);  // Set reference for legion composition decisions

    log("Triggering End Turn button click...");

    // Use QTimer to give UI a moment to update, then trigger end turn
    QTimer::singleShot(100, this, [this]() {
        // Call the endTurn method which clicks the End Turn button
        m_infoWidget->endTurn();

        log("=== TURN COMPLETE ===");
        emit turnComplete();
    });
}

// ============================================================================
// State Reading
// ============================================================================

AIPlayer::GameState AIPlayer::readGameState()
{
    GameState state;

    if (!m_player || !m_infoWidget) {
        return state;
    }

    QChar playerId = m_player->getId();

    // =========================================================================
    // READ FROM UI (PlayerInfoWidget) instead of Player object directly
    // This ensures AI "sees" what a human would see in the UI
    // =========================================================================

    // Read wallet from UI
    int displayedWallet = m_infoWidget->getDisplayedWallet(playerId);
    if (displayedWallet >= 0) {
        state.wallet = displayedWallet;
        log(QString("UI Read: Wallet = %1 talents").arg(state.wallet));
    } else {
        // Fallback to Player object if UI read fails
        state.wallet = m_player->getWallet();
        log(QString("UI Read FAILED for wallet, using Player object: %1").arg(state.wallet));
    }

    // Read territory count from UI
    int displayedTerritoryCount = m_infoWidget->getDisplayedTerritoryCount(playerId);
    if (displayedTerritoryCount >= 0) {
        log(QString("UI Read: Territory count = %1").arg(displayedTerritoryCount));
    }

    // Read owned territories from UI
    QStringList displayedTerritories = m_infoWidget->getDisplayedTerritories(playerId);
    if (!displayedTerritories.isEmpty()) {
        state.ownedTerritories = displayedTerritories;
        log(QString("UI Read: %1 territories").arg(displayedTerritories.size()));
    } else {
        // Fallback to Player object
        state.ownedTerritories = m_player->getOwnedTerritories();
        log(QString("UI Read for territories returned empty, using Player object: %1").arg(state.ownedTerritories.size()));
    }

    // Read leader count from UI (this counts leaders only, not troops)
    int displayedPieceCount = m_infoWidget->getDisplayedPieceCount(playerId);
    log(QString("UI Read: Leader count = %1").arg(displayedPieceCount));

    // Total pieces still comes from Player (UI doesn't show troop totals easily)
    state.totalPieces = m_player->getTotalPieceCount();

    // Find enemy territories by scanning the map
    // (This info isn't directly in PlayerInfoWidget, so we read from MapWidget)
    if (m_mapWidget) {
        for (int row = 0; row < MapWidget::ROWS; ++row) {
            for (int col = 0; col < MapWidget::COLUMNS; ++col) {
                QChar owner = m_mapWidget->getTerritoryOwnerAt(row, col);
                if (owner != '\0' && owner != playerId) {
                    QString territoryName = m_mapWidget->getTerritoryNameAt(row, col);
                    if (!state.enemyTerritories.contains(territoryName)) {
                        state.enemyTerritories.append(territoryName);
                    }
                }
            }
        }
    }

    // =========================================================================
    // READ LEADER INFO FROM UI TABLES
    // =========================================================================
    QList<PlayerInfoWidget::DisplayedLeaderInfo> displayedLeaders = m_infoWidget->getDisplayedLeaders(playerId);
    log(QString("UI Read: Found %1 leaders in UI tables").arg(displayedLeaders.size()));

    for (const PlayerInfoWidget::DisplayedLeaderInfo &uiLeader : displayedLeaders) {
        LeaderInfo info;
        info.name = uiLeader.type;
        info.territory = uiLeader.territory;
        info.movesRemaining = uiLeader.movesRemaining;
        info.legionSize = 0;  // Not available from UI directly
        info.piece = nullptr;  // We'll try to find the actual piece

        // Try to find the actual GamePiece by matching serial number
        if (uiLeader.type == "Caesar") {
            for (CaesarPiece *caesar : m_player->getCaesars()) {
                if (caesar->getSerialNumber() == uiLeader.serialNumber) {
                    info.piece = caesar;
                    info.legionSize = caesar->getLegion().size();
                    break;
                }
            }
        } else if (uiLeader.type == "General") {
            for (GeneralPiece *general : m_player->getGenerals()) {
                if (general->getSerialNumber() == uiLeader.serialNumber) {
                    info.piece = general;
                    info.name = QString("General %1").arg(general->getNumber());
                    info.legionSize = general->getLegion().size();
                    break;
                }
            }
        }

        state.leaders.append(info);

        log(QString("  Leader: %1 @ %2, moves=%3, legion=%4")
            .arg(info.name)
            .arg(info.territory)
            .arg(info.movesRemaining)
            .arg(info.legionSize));
    }

    return state;
}

void AIPlayer::emitStateUpdate(const GameState &state)
{
    emit stateUpdated(state.wallet, state.ownedTerritories.size(), state.totalPieces);

    // Build leader descriptions
    QStringList leaderDescriptions;
    for (const LeaderInfo &leader : state.leaders) {
        QString desc = QString("%1 @ %2 (%3 moves, %4 troops)")
            .arg(leader.name)
            .arg(leader.territory)
            .arg(leader.movesRemaining)
            .arg(leader.legionSize);
        leaderDescriptions.append(desc);
    }
    emit leadersUpdated(leaderDescriptions);
}

// ============================================================================
// Move Evaluation
// ============================================================================

QList<MoveEvaluation> AIPlayer::evaluateMovesForLeader(GamePiece *leader, const GameState &state)
{
    QList<MoveEvaluation> moves;

    if (!leader || !m_infoWidget) {
        return moves;
    }

    QString currentTerritory = leader->getTerritoryName();
    QString leaderName = "Unknown";

    int legionSize = 0;
    if (leader->getType() == GamePiece::Type::Caesar) {
        leaderName = "Caesar";
        legionSize = static_cast<CaesarPiece*>(leader)->getLegion().size();
    } else if (leader->getType() == GamePiece::Type::General) {
        GeneralPiece *gen = qobject_cast<GeneralPiece*>(leader);
        if (gen) {
            leaderName = QString("General %1").arg(gen->getNumber());
            legionSize = gen->getLegion().size();
        }
    }

    // Add "stay" option
    MoveEvaluation stayMove;
    stayMove.leaderName = leaderName;
    stayMove.fromTerritory = currentTerritory;
    stayMove.targetTerritory = currentTerritory;
    stayMove.score = 10;  // Base score for staying
    stayMove.moveType = "Stay";
    stayMove.reason = "Hold current position";
    moves.append(stayMove);

    // Use PlayerInfoWidget::getMovesForLeader() to get all valid moves
    // This includes adjacent territories AND road-connected territories
    QList<PlayerInfoWidget::MoveOption> validMoves = m_infoWidget->getMovesForLeader(leader);

    for (const PlayerInfoWidget::MoveOption &option : validMoves) {
        MoveEvaluation move;
        move.leaderName = leaderName;
        move.fromTerritory = currentTerritory;
        move.targetTerritory = option.destinationTerritory;

        // Determine move type based on ownership and combat status
        if (option.hasCombat) {
            // Enemy territory or enemy troops present - attack
            move.score = scoreAttackMove(option.destinationTerritory, state);
            move.moveType = "Attack";
            move.reason = QString("Attack territory (owner: %1)%2")
                .arg(option.owner == '\0' ? "none" : QString(option.owner))
                .arg(option.troopInfo.isEmpty() ? "" : QString(" - %1").arg(option.troopInfo));
        } else if (option.isOwnTerritory) {
            // Own territory - reinforce
            move.score = scoreDefendMove(option.destinationTerritory, state);
            move.moveType = "Reinforce";
            move.reason = QString("Move to own territory%1")
                .arg(option.isViaRoad ? " [via road]" : "");
        } else if (option.owner == '\0') {
            // Unclaimed territory - expand (but only if general has troops)
            if (legionSize == 0) {
                // General without troops cannot claim territory - skip this move
                continue;
            }
            move.score = scoreExpandMove(option.destinationTerritory, state);
            move.moveType = "Expand";
            move.reason = QString("Claim unclaimed territory (value: %1)%2")
                .arg(option.territoryValue)
                .arg(option.isViaRoad ? " [via road]" : "");
        } else {
            // Other player's territory without their troops - still counts as attack
            move.score = scoreAttackMove(option.destinationTerritory, state);
            move.moveType = "Attack";
            move.reason = QString("Enter Player %1's territory").arg(option.owner);
        }

        moves.append(move);
    }

    return moves;
}

MoveEvaluation AIPlayer::selectBestMove(const QList<MoveEvaluation> &moves)
{
    if (moves.isEmpty()) {
        return MoveEvaluation();
    }

    if (m_strategy == Strategy::Random) {
        // Pure random selection
        int index = QRandomGenerator::global()->bounded(moves.size());
        MoveEvaluation selected = moves[index];
        selected.isSelected = true;
        return selected;
    }

    // Find the highest score
    int highestScore = moves[0].score;
    for (const MoveEvaluation &move : moves) {
        if (move.score > highestScore) {
            highestScore = move.score;
        }
    }

    // Collect all moves with the highest score
    QList<MoveEvaluation> topMoves;
    for (const MoveEvaluation &move : moves) {
        if (move.score == highestScore) {
            topMoves.append(move);
        }
    }

    // Pick randomly among the top scoring moves
    int index = QRandomGenerator::global()->bounded(topMoves.size());
    MoveEvaluation selected = topMoves[index];
    selected.isSelected = true;
    return selected;
}

int AIPlayer::scoreAttackMove(const QString &target, const GameState &state)
{
    Q_UNUSED(target)
    Q_UNUSED(state)

    int baseScore = 50;

    switch (m_strategy) {
        case Strategy::Aggressive:
            baseScore = 100;
            break;
        case Strategy::Defensive:
            baseScore = 30;
            break;
        case Strategy::Economic:
            baseScore = 40;
            break;
        default:
            break;
    }

    // Add randomness
    baseScore += QRandomGenerator::global()->bounded(20);

    return baseScore;
}

int AIPlayer::scoreExpandMove(const QString &target, const GameState &state)
{
    Q_UNUSED(state)

    // Score = territory value (5 or 10) + 100 base
    // This ensures unclaimed territories always score higher than owned ones
    int score = 100;

    if (m_mapWidget) {
        Position pos = m_mapWidget->territoryNameToPosition(target);
        if (pos.row >= 0) {
            int value = m_mapWidget->getTerritoryValueAt(pos.row, pos.col);
            score += value;  // Higher value = higher score (105 or 110)
        }
    }

    return score;
}

int AIPlayer::scoreDefendMove(const QString &target, const GameState &state)
{
    Q_UNUSED(target)
    Q_UNUSED(state)

    // Moving to own territory adds no income, but still better than staying
    // Score 50 so: Expand (105-110) > Defend (50) > Stay (10)
    return 50;
}

int AIPlayer::getEnemyStrengthAt(const QString &territory)
{
    Q_UNUSED(territory)
    // TODO: Implement by querying other players' pieces at this territory
    return 0;
}

int AIPlayer::getMyStrengthAt(const QString &territory)
{
    if (!m_player) return 0;
    return m_player->getPieceCountAtTerritory(territory);
}

bool AIPlayer::isAdjacentToEnemy(const QString &territory)
{
    Q_UNUSED(territory)
    // TODO: Implement
    return false;
}

// ============================================================================
// Combat & Purchase (stubs for now)
// ============================================================================

void AIPlayer::handleCombatDialog(CombatDialog *dialog)
{
    Q_UNUSED(dialog)
    log("Combat dialog opened - AI combat not yet implemented");
    // TODO: Implement combat AI
}

void AIPlayer::handlePurchaseDialog(PurchaseDialog *dialog)
{
    Q_UNUSED(dialog)
    log("Purchase dialog opened - AI purchase not yet implemented");
    // TODO: Implement purchase AI
}

void AIPlayer::executeCombatPhase()
{
    log("Combat phase - not yet implemented");
}

void AIPlayer::executePurchasePhase()
{
    log("Purchase phase - not yet implemented");
}

void AIPlayer::executeCombatInDialog(CombatDialog *dialog)
{
    Q_UNUSED(dialog)
}

void AIPlayer::selectAndClickTarget(CombatDialog *dialog, bool isAttacking)
{
    Q_UNUSED(dialog)
    Q_UNUSED(isAttacking)
}

PurchaseDecision AIPlayer::decidePurchases(int budget)
{
    Q_UNUSED(budget)
    return PurchaseDecision();
}

void AIPlayer::executePurchaseDecision(PurchaseDialog *dialog, const PurchaseDecision &decision)
{
    Q_UNUSED(dialog)
    Q_UNUSED(decision)
}

void AIPlayer::performMove(GamePiece *leader, const QString &targetTerritory)
{
    Q_UNUSED(leader)
    Q_UNUSED(targetTerritory)
    // TODO: Call PlayerInfoWidget method to perform actual move
    log(QString("Would move to: %1").arg(targetTerritory));
}

// ============================================================================
// Legion Building Logic
// ============================================================================

QList<int> AIPlayer::decideLegionComposition(GamePiece *general, const QList<GamePiece*> &availableTroops)
{
    QList<int> troopsToSelect;

    if (!general || !m_player) {
        return troopsToSelect;
    }

    // Cast to GeneralPiece to access getLegion()
    GeneralPiece *gen = qobject_cast<GeneralPiece*>(general);
    if (!gen) {
        log("decideLegionComposition: Not a GeneralPiece, returning empty list");
        return troopsToSelect;
    }

    // Get the general's current legion
    QList<int> currentLegion = gen->getLegion();
    int currentLegionSize = currentLegion.size();

    // Calculate quota: total troops / 6 generals
    int totalTroops = m_player->getInfantryCount() +
                      m_player->getCavalryCount() +
                      m_player->getCatapultCount();
    int numGenerals = m_player->getGenerals().size();
    if (numGenerals == 0) numGenerals = 1;  // Avoid division by zero

    int quota = totalTroops / numGenerals;
    // Handle remainder: generals with lower numbers get priority
    int remainder = totalTroops % numGenerals;
    int generalNumber = gen->getNumber();
    if (generalNumber <= remainder) {
        quota += 1;  // This general gets one extra from the remainder
    }

    log(QString("Legion Building: General %1 - Current legion: %2, Quota: %3, Total troops: %4")
        .arg(generalNumber).arg(currentLegionSize).arg(quota).arg(totalTroops));

    // Build set of all troop IDs in ANY general's legion (to identify assigned troops)
    QSet<int> assignedTroopIds;
    for (GeneralPiece *g : m_player->getGenerals()) {
        for (int id : g->getLegion()) {
            assignedTroopIds.insert(id);
        }
    }

    // Process each available troop
    for (GamePiece *troop : availableTroops) {
        int troopId = troop->getUniqueId();

        // Rule 1: If troop is in THIS general's legion, always select it (permanent)
        if (currentLegion.contains(troopId)) {
            troopsToSelect.append(troopId);
            log(QString("  Troop %1: In this general's legion - SELECTED (permanent)")
                .arg(troopId));
            continue;
        }

        // Rule 2: If troop is in ANOTHER general's legion, don't select it
        if (assignedTroopIds.contains(troopId)) {
            log(QString("  Troop %1: In another general's legion - SKIP")
                .arg(troopId));
            continue;
        }

        // Rule 3: Troop is unassigned - check if general has space AND is below quota
        bool hasSpace = currentLegionSize < 6;  // Max 6 troops per legion
        bool belowQuota = currentLegionSize < quota;

        if (hasSpace && belowQuota) {
            // Also check that troop has moves remaining
            if (troop->getMovesRemaining() > 0) {
                troopsToSelect.append(troopId);
                currentLegionSize++;  // Update for next iteration
                log(QString("  Troop %1: Unassigned, has space & below quota - SELECTED")
                    .arg(troopId));
            } else {
                log(QString("  Troop %1: Unassigned but no moves remaining - SKIP")
                    .arg(troopId));
            }
        } else {
            log(QString("  Troop %1: Unassigned but at capacity (space=%2, belowQuota=%3) - SKIP")
                .arg(troopId).arg(hasSpace).arg(belowQuota));
        }
    }

    log(QString("Legion Building: Selected %1 troops for General %2")
        .arg(troopsToSelect.size()).arg(generalNumber));

    return troopsToSelect;
}

bool AIPlayer::canGeneralMove(GamePiece *general) const
{
    if (!general || !m_player) {
        return false;
    }

    // Cast to GeneralPiece to access getLegion()
    GeneralPiece *gen = qobject_cast<GeneralPiece*>(general);
    if (!gen) {
        // Not a general - can't determine legion, assume can move
        return true;
    }

    // Get the general's legion
    QList<int> legion = gen->getLegion();

    // If legion is empty, general can move freely
    if (legion.isEmpty()) {
        return true;
    }

    // Check if ALL troops in the legion have moves remaining
    // We need to find each troop by ID and check its moves
    for (int troopId : legion) {
        GamePiece *troop = m_player->getPieceByUniqueId(troopId);
        if (troop && troop->getMovesRemaining() <= 0) {
            qDebug() << "canGeneralMove: Troop" << troopId << "has no moves remaining";
            return false;
        }
    }

    return true;
}

bool AIPlayer::hasMovesRemaining(const GameState &state)
{
    for (const LeaderInfo &leader : state.leaders) {
        if (leader.movesRemaining > 0) {
            return true;
        }
    }
    return false;
}

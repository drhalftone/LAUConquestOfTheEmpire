#include "aiplayer.h"
#include "player.h"
#include "playerinfowidget.h"
#ifdef USE_OPENGL_MAP
#include "gamemapwidget.h"
#else
#include "mapwidget.h"
#endif
#include "gamepiece.h"
#include "combatdialog.h"
#include "purchasedialog.h"
#include "building.h"
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

    // Reset planning state for new turn
    m_planCreated = false;
    m_currentPlan = MovementPlan();

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

    // Use risk-based decision maker if that strategy is selected
    if (m_strategy == Strategy::RiskBased) {
        executeMovementPhaseRiskBased();
        return;
    }

    // Original strategy-based movement logic
    // Find GENERALS (not Caesars) with moves remaining AND whose legion can move
    // NOTE: We use piece->getMovesRemaining() instead of UI-displayed movesRemaining
    // because the UI tables may not be refreshed after startTurn() resets moves
    QList<LeaderInfo> generalsWithMoves;
    for (const LeaderInfo &leader : m_lastGameState.leaders) {
        if (leader.piece &&
            leader.piece->getType() == GamePiece::Type::General &&
            leader.piece->getMovesRemaining() > 0) {
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

    // Log all available moves with scores for debugging
    log(QString("%1: Evaluating %2 move options:").arg(general.name).arg(moves.size()));
    for (const MoveEvaluation &m : moves) {
        log(QString("  -> %1: %2 (score=%3)")
            .arg(m.targetTerritory)
            .arg(m.moveType)
            .arg(m.score));
    }

    // Select best move based on strategy (picks highest score)
    MoveEvaluation selectedMove = selectBestMove(moves);
    emit moveSelected(selectedMove);

    log(QString("Selected move for %1: %2 -> %3 (%4, score=%5)")
        .arg(general.name)
        .arg(selectedMove.fromTerritory)
        .arg(selectedMove.targetTerritory)
        .arg(selectedMove.moveType)
        .arg(selectedMove.score));

    // Skip if "Stay" was selected
    if (selectedMove.targetTerritory == selectedMove.fromTerritory) {
        log(QString("%1: Staying at %2").arg(general.name).arg(selectedMove.fromTerritory));
        // Consume all moves so this general won't be considered again this turn
        general.piece->setMovesRemaining(0);
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

void AIPlayer::executeMovementPhaseRiskBased()
{
    static int moveIterationCount = 0;
    moveIterationCount++;
    log(QString("Executing PLANNED movement phase (iteration %1)...").arg(moveIterationCount));

    // Reset counter at start of new turn
    if (moveIterationCount > 100) {
        moveIterationCount = 1;  // Safety reset to prevent overflow
    }

    // Get all players from the info widget
    const QList<Player*> &allPlayers = m_infoWidget->getPlayers();
    MapGraph *graph = m_mapWidget->getGraph();

    if (!graph) {
        log("ERROR: No map graph available");
        executeEndTurn();
        return;
    }

    // Detect first move of turn: check if all generals have full moves (2.0)
    bool isFirstMoveOfTurn = true;
    for (GeneralPiece *gen : m_player->getGenerals()) {
        if (gen->getMovesRemaining() < 2.0) {
            isFirstMoveOfTurn = false;
            break;
        }
    }

    // On first move of turn, CREATE THE PLAN and print comprehensive situation report
    if (isFirstMoveOfTurn) {
        // Reset plan state for new turn
        m_planCreated = false;
        log("========== AI TURN START - SITUATION REPORT ==========");

        // Print our territories and their risk levels
        ReachabilityCalculator calc;
        QMap<QString, TerritoryRisk> riskMap = calc.assessAllTerritories(m_player, allPlayers, graph);

        log("--- OUR TERRITORIES ---");
        for (const QString &territory : m_player->getOwnedTerritories()) {
            QString riskStr = "UNKNOWN";
            int ourForce = 0, enemyForce = 0;
            if (riskMap.contains(territory)) {
                const TerritoryRisk &r = riskMap[territory];
                ourForce = r.ourMaxForce;
                enemyForce = r.enemyMaxForce;
                switch (r.risk) {
                    case RiskLevel::Safe: riskStr = "SAFE"; break;
                    case RiskLevel::Low: riskStr = "LOW"; break;
                    case RiskLevel::Medium: riskStr = "MEDIUM"; break;
                    case RiskLevel::High: riskStr = "HIGH"; break;
                    case RiskLevel::Unreachable: riskStr = "UNREACHABLE"; break;
                }
            }
            // Check for city
            City *city = m_player->getCityAtTerritory(territory);
            QString cityStr = "";
            if (city) {
                cityStr = city->isFortified() ? " [FORTIFIED CITY]" : " [City]";
            }
            log(QString("  %1%2: Risk=%3 (our force=%4, enemy force=%5)")
                .arg(territory).arg(cityStr).arg(riskStr).arg(ourForce).arg(enemyForce));
        }

        // Print our leaders and their positions AND what they can reach
        log("--- OUR LEADERS ---");
        for (CaesarPiece *caesar : m_player->getCaesars()) {
            QMap<QString, ReachInfo> reachable = calc.getReachableFrom(caesar, graph, m_player);
            QStringList reachNames;
            for (const QString &t : reachable.keys()) reachNames << t;
            log(QString("  Caesar at %1 (moves=%2, legion=%3) -> can reach: %4")
                .arg(caesar->getTerritoryName())
                .arg(caesar->getMovesRemaining())
                .arg(caesar->getLegion().size())
                .arg(reachNames.join(", ")));
        }
        for (GeneralPiece *gen : m_player->getGenerals()) {
            QMap<QString, ReachInfo> reachable = calc.getReachableFrom(gen, graph, m_player);
            QStringList reachNames;
            for (const QString &t : reachable.keys()) reachNames << t;
            log(QString("  General #%1 at %2 (moves=%3, legion=%4) -> can reach: %5")
                .arg(gen->getNumber())
                .arg(gen->getTerritoryName())
                .arg(gen->getMovesRemaining())
                .arg(gen->getLegion().size())
                .arg(reachNames.join(", ")));
        }

        // Print enemy positions (troops and leaders)
        log("--- ENEMY POSITIONS ---");
        for (Player *enemy : allPlayers) {
            if (enemy == m_player) continue;
            log(QString("  Player %1:").arg(enemy->getId()));
            for (CaesarPiece *caesar : enemy->getCaesars()) {
                log(QString("    Caesar at %1 (legion=%2)")
                    .arg(caesar->getTerritoryName()).arg(caesar->getLegion().size()));
            }
            for (GeneralPiece *gen : enemy->getGenerals()) {
                log(QString("    General #%1 at %2 (legion=%3)")
                    .arg(gen->getNumber()).arg(gen->getTerritoryName()).arg(gen->getLegion().size()));
            }
            // Count troops by territory
            QMap<QString, int> troopsByTerritory;
            for (InfantryPiece *inf : enemy->getInfantry()) {
                troopsByTerritory[inf->getTerritoryName()]++;
            }
            for (CavalryPiece *cav : enemy->getCavalry()) {
                troopsByTerritory[cav->getTerritoryName()]++;
            }
            for (CatapultPiece *cat : enemy->getCatapults()) {
                troopsByTerritory[cat->getTerritoryName()]++;
            }
            for (auto it = troopsByTerritory.begin(); it != troopsByTerritory.end(); ++it) {
                log(QString("    %1 troops at %2").arg(it.value()).arg(it.key()));
            }
        }

        // Debug: Show what enemies can reach our territories
        log("--- ENEMY THREAT ANALYSIS ---");
        for (Player *enemy : allPlayers) {
            if (enemy == m_player) continue;
            QMap<QString, ReachInfo> enemyReach = calc.getAllReachable(enemy, graph);
            log(QString("  Player %1 can reach %2 total territories").arg(enemy->getId()).arg(enemyReach.size()));

            // Show what each enemy general can reach
            for (GeneralPiece *gen : enemy->getGenerals()) {
                QMap<QString, ReachInfo> genReach = calc.getReachableFrom(gen, graph, enemy);
                QStringList reachNames;
                for (const QString &t : genReach.keys()) reachNames << t;
                log(QString("    General #%1 at %2 can reach: %3")
                    .arg(gen->getNumber()).arg(gen->getTerritoryName()).arg(reachNames.join(", ")));
            }

            // Check threats to our territories
            for (const QString &territory : m_player->getOwnedTerritories()) {
                if (enemyReach.contains(territory)) {
                    const ReachInfo &info = enemyReach[territory];
                    log(QString("  ** THREAT: Player %1 can reach %2 with force=%3")
                        .arg(enemy->getId()).arg(territory).arg(info.maxTroopStrength));
                }
            }
        }

        log("--- CREATING MOVEMENT PLAN ---");

        // CREATE THE PLAN at the start of the turn
        m_currentPlan = m_decisionMaker.planMovement(m_player, allPlayers, graph);
        m_planCreated = true;

        log(QString("Plan created: %1").arg(m_currentPlan.summary));
        for (const GeneralAssignment &assignment : m_currentPlan.assignments) {
            if (assignment.general && assignment.general->getType() == GamePiece::Type::General) {
                GeneralPiece *gen = static_cast<GeneralPiece*>(assignment.general);
                log(QString("  General #%1: %2 -> %3 (%4) with %5 troops")
                    .arg(gen->getNumber())
                    .arg(gen->getTerritoryName())
                    .arg(assignment.targetTerritory)
                    .arg(assignment.missionType)
                    .arg(assignment.troopsToTake));
            }
        }
    }

    // Get the next move from the plan (not greedy per-move scoring)
    ScoredMove bestMove = m_decisionMaker.getNextMoveFromPlan(m_currentPlan, m_player, graph);

    if (!bestMove.isValid()) {
        // Check if any generals still have moves remaining
        bool anyMovesLeft = false;
        for (GeneralPiece *gen : m_player->getGenerals()) {
            if (gen->getMovesRemaining() >= 1.0) {
                anyMovesLeft = true;
                break;
            }
        }

        if (anyMovesLeft) {
            // Plan exhausted but generals have moves - CREATE A NEW PLAN for remaining moves
            log("--- CREATING SECOND-ROUND PLAN (generals still have moves) ---");

            m_currentPlan = m_decisionMaker.planMovement(m_player, allPlayers, graph);

            log(QString("Second-round plan: %1").arg(m_currentPlan.summary));
            for (const GeneralAssignment &assignment : m_currentPlan.assignments) {
                if (assignment.general && assignment.general->getType() == GamePiece::Type::General) {
                    GeneralPiece *gen = static_cast<GeneralPiece*>(assignment.general);
                    log(QString("  General #%1: %2 -> %3 (%4) with %5 troops")
                        .arg(gen->getNumber())
                        .arg(gen->getTerritoryName())
                        .arg(assignment.targetTerritory)
                        .arg(assignment.missionType)
                        .arg(assignment.troopsToTake));
                }
            }

            // Try to get a move from the new plan
            bestMove = m_decisionMaker.getNextMoveFromPlan(m_currentPlan, m_player, graph);
        }

        if (!bestMove.isValid()) {
            log("No valid moves available - ending turn");
            for (GeneralPiece *gen : m_player->getGenerals()) {
                log(QString("  General #%1 at %2: %3 moves remaining")
                    .arg(gen->getNumber())
                    .arg(gen->getTerritoryName())
                    .arg(gen->getMovesRemaining()));
            }
            executeEndTurn();
            return;
        }
    }

    // Check if the move has a positive score
    if (bestMove.score <= 0) {
        log(QString("Best move has non-positive score (%1) - reason: %2").arg(bestMove.score).arg(bestMove.reason));
        executeEndTurn();
        return;
    }

    // Check if the leader can actually move (legion constraints)
    if (!canGeneralMove(bestMove.leader)) {
        log(QString("Best move leader cannot move (legion constraints), consuming moves"));
        bestMove.leader->setMovesRemaining(0);
        scheduleNextAction([this]() {
            executeMovementPhaseRiskBased();
        });
        return;
    }

    // Log the decision
    QString leaderName;
    if (bestMove.leader->getType() == GamePiece::Type::Caesar) {
        leaderName = "Caesar";
    } else if (bestMove.leader->getType() == GamePiece::Type::General) {
        GeneralPiece *gen = static_cast<GeneralPiece*>(bestMove.leader);
        leaderName = QString("General #%1").arg(gen->getNumber());
    } else {
        leaderName = "Galley";
    }

    // IMPORTANT: Validate that the destination is actually reachable in ONE move
    // The ReachabilityCalculator considers multi-hop moves, but we can only move
    // one step at a time. Check against getMovesForLeader.
    QList<PlayerInfoWidget::MoveOption> validMoves = m_infoWidget->getMovesForLeader(bestMove.leader);
    bool destinationIsValid = false;
    for (const auto &move : validMoves) {
        if (move.destinationTerritory == bestMove.destination) {
            destinationIsValid = true;
            break;
        }
    }

    if (!destinationIsValid) {
        log(QString("PLANNED: %1 wants %2 -> %3 but destination not reachable in one move!")
            .arg(leaderName)
            .arg(bestMove.leader->getTerritoryName())
            .arg(bestMove.destination));
        log(QString("  Available destinations: %1").arg(validMoves.size()));
        for (const auto &move : validMoves) {
            log(QString("    - %1").arg(move.destinationTerritory));
        }
        // Pick the best valid destination instead
        // Find the highest-scoring move that IS in validMoves
        QList<ScoredMove> allMoves = m_decisionMaker.getAllScoredMoves(m_player, allPlayers, graph);
        for (const ScoredMove &altMove : allMoves) {
            if (altMove.leader == bestMove.leader) {
                for (const auto &validMove : validMoves) {
                    if (validMove.destinationTerritory == altMove.destination) {
                        bestMove = altMove;
                        log(QString("  Falling back to: %1 (score=%2)")
                            .arg(altMove.destination).arg(altMove.score));
                        destinationIsValid = true;
                        break;
                    }
                }
                if (destinationIsValid) break;
            }
        }

        if (!destinationIsValid) {
            log(QString("  No valid moves for this leader, consuming moves"));
            bestMove.leader->setMovesRemaining(0);
            scheduleNextAction([this]() {
                executeMovementPhaseRiskBased();
            });
            return;
        }
    }

    log(QString("PLANNED: %1 at %2 -> %3 (score=%4, troops=%5)")
        .arg(leaderName)
        .arg(bestMove.leader->getTerritoryName())
        .arg(bestMove.destination)
        .arg(bestMove.score)
        .arg(bestMove.troopsCanBring));
    log(QString("  Reason: %1").arg(bestMove.reason));

    // Convert to MoveEvaluation for compatibility with existing signals
    MoveEvaluation evalMove;
    evalMove.leaderName = leaderName;
    evalMove.fromTerritory = bestMove.leader->getTerritoryName();
    evalMove.targetTerritory = bestMove.destination;
    evalMove.score = bestMove.score;
    evalMove.moveType = "Planned";
    evalMove.reason = bestMove.reason;
    evalMove.isSelected = true;
    emit moveSelected(evalMove);

    // Skip if staying in place
    if (bestMove.destination == bestMove.leader->getTerritoryName()) {
        log(QString("%1: Staying at %2").arg(leaderName).arg(bestMove.destination));
        bestMove.leader->setMovesRemaining(0);
        scheduleNextAction([this]() {
            executeMovementPhaseRiskBased();
        });
        return;
    }

    // CRITICAL SAFETY CHECK: Caesar is EXTREMELY valuable - losing Caesar loses the game!
    // Enemy capturing Caesar gets 100 tax bonus, so they WILL attack at equal strength.
    // Caesar should stay in fortified cities unless we've lost most generals.
    if (bestMove.leader->getType() == GamePiece::Type::Caesar) {
        QString caesarTerritory = bestMove.leader->getTerritoryName();
        CaesarPiece *caesar = static_cast<CaesarPiece*>(bestMove.leader);
        int numGenerals = m_player->getGenerals().size();

        // Check if Caesar is currently in a fortified city we own
        City *caesarCity = m_player->getCityAtTerritory(caesarTerritory);
        bool inFortifiedCity = caesarCity && caesarCity->isFortified();

        // Caesar should ONLY lead armies if we've lost most generals (desperate situation)
        // Standard practice: Caesar stays in fortified city
        const int MIN_GENERALS_BEFORE_CAESAR_MOVES = 2;  // Only use Caesar if < 2 generals left

        if (numGenerals >= MIN_GENERALS_BEFORE_CAESAR_MOVES) {
            log(QString("SAFETY: Caesar should NOT lead armies - we still have %1 generals").arg(numGenerals));
            log("  Caesar is too valuable to risk. Use generals instead!");
            bestMove.leader->setMovesRemaining(0);
            scheduleNextAction([this]() {
                executeMovementPhaseRiskBased();
            });
            return;
        }

        // If Caesar MUST move (desperate - few generals), require massive troop advantage
        // Enemy will attack Caesar at equal strength, so we need overwhelming force
        QSet<int> caesarLegion = QSet<int>(caesar->getLegion().begin(), caesar->getLegion().end());
        int availableTroopsCount = 0;

        // Count troops already in Caesar's legion with moves
        for (int troopId : caesar->getLegion()) {
            GamePiece *troop = m_player->getPieceByUniqueId(troopId);
            if (troop && troop->getMovesRemaining() > 0) {
                availableTroopsCount++;
            }
        }

        // Count unassigned troops at same territory with moves
        QSet<int> assignedTroopIds;
        for (GeneralPiece *g : m_player->getGenerals()) {
            for (int id : g->getLegion()) {
                assignedTroopIds.insert(id);
            }
        }

        // Check all troop types at same territory
        for (InfantryPiece *troop : m_player->getInfantryAtTerritory(caesarTerritory)) {
            if (troop->getMovesRemaining() > 0 &&
                !caesarLegion.contains(troop->getUniqueId()) &&
                !assignedTroopIds.contains(troop->getUniqueId())) {
                availableTroopsCount++;
            }
        }
        for (CavalryPiece *troop : m_player->getCavalryAtTerritory(caesarTerritory)) {
            if (troop->getMovesRemaining() > 0 &&
                !caesarLegion.contains(troop->getUniqueId()) &&
                !assignedTroopIds.contains(troop->getUniqueId())) {
                availableTroopsCount++;
            }
        }
        for (CatapultPiece *troop : m_player->getCatapultsAtTerritory(caesarTerritory)) {
            if (troop->getMovesRemaining() > 0 &&
                !caesarLegion.contains(troop->getUniqueId()) &&
                !assignedTroopIds.contains(troop->getUniqueId())) {
                availableTroopsCount++;
            }
        }

        // Caesar needs MASSIVE troop protection - enemy will attack at equal odds for 100 gold bonus
        // Require at least 5 troops (full legion minus 1) to even consider moving Caesar
        const int MIN_TROOPS_FOR_CAESAR = 5;
        if (availableTroopsCount < MIN_TROOPS_FOR_CAESAR) {
            log(QString("SAFETY: Caesar at %1 only has %2 troops available (need %3) - SKIPPING MOVE")
                .arg(caesarTerritory).arg(availableTroopsCount).arg(MIN_TROOPS_FOR_CAESAR));
            log("  Caesar is too valuable to move without overwhelming force!");
            bestMove.leader->setMovesRemaining(0);
            scheduleNextAction([this]() {
                executeMovementPhaseRiskBased();
            });
            return;
        }

        // Even with enough troops, warn that Caesar is being used
        log(QString("WARNING: Using Caesar to lead army (only %1 generals left) - HIGH RISK!")
            .arg(numGenerals));
    }

    // === TROOP TRANSFER LOGIC ===
    // If a general is returning home to pick up more troops, first transfer their
    // current troops to another general at the same location (if one exists)
    // This prevents troops from making the round trip unnecessarily
    QString homeProvince = m_player->getHomeProvinceName();
    bool isReturningHome = (bestMove.destination == homeProvince) &&
                           bestMove.reason.contains("home", Qt::CaseInsensitive);

    if (isReturningHome && bestMove.leader->getType() == GamePiece::Type::General) {
        GeneralPiece *general = static_cast<GeneralPiece*>(bestMove.leader);
        int currentTroops = general->getLegion().size();

        if (currentTroops > 0) {
            log(QString("General #%1 is returning home with %2 troops - checking for handoff opportunity...")
                .arg(general->getNumber()).arg(currentTroops));

            // Try to transfer troops to another leader at the same location
            bool transferred = transferTroopsToOtherGeneral(general);

            if (transferred) {
                log(QString("Troops handed off! General #%1 now has %2 troops and can travel light to home")
                    .arg(general->getNumber()).arg(general->getLegion().size()));
            }
        }
    }

    // Actually perform the move
    bool moveSuccess = m_infoWidget->aiMoveLeaderToTerritory(bestMove.leader, bestMove.destination);

    if (moveSuccess) {
        log(QString("%1: Moved to %2 successfully!").arg(leaderName).arg(bestMove.destination));

        // Check if we moved into combat - if so, consume all moves for leader and legion
        if (m_infoWidget->hasEnemyPiecesAt(bestMove.destination, m_player)) {
            log(QString("Moved into combat at %1 - consuming moves to prevent further movement").arg(bestMove.destination));
            // Consume all remaining moves for this leader
            bestMove.leader->setMovesRemaining(0);
            // Also consume moves for all troops in the legion
            QList<int> legionIds;
            if (bestMove.leader->getType() == GamePiece::Type::Caesar) {
                legionIds = static_cast<CaesarPiece*>(bestMove.leader)->getLegion();
            } else if (bestMove.leader->getType() == GamePiece::Type::General) {
                legionIds = static_cast<GeneralPiece*>(bestMove.leader)->getLegion();
            }
            for (int troopId : legionIds) {
                GamePiece *troop = m_player->getPieceByUniqueId(troopId);
                if (troop) {
                    troop->setMovesRemaining(0);
                }
            }
        }
    } else {
        log(QString("%1: Move to %2 FAILED - possibly cancelled or blocked").arg(leaderName).arg(bestMove.destination));
        log(QString("  Leader still at: %1 with %2 moves")
            .arg(bestMove.leader->getTerritoryName())
            .arg(bestMove.leader->getMovesRemaining()));
        // Consume moves to prevent infinite loop
        bestMove.leader->setMovesRemaining(0);
    }

    // Schedule next movement check
    scheduleNextAction([this]() {
        executeMovementPhaseRiskBased();
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

    // Find enemy territories by iterating through all players
    const QList<Player*> &allPlayers = m_infoWidget->getPlayers();
    for (Player *otherPlayer : allPlayers) {
        if (otherPlayer->getId() != playerId) {
            for (const QString &territoryName : otherPlayer->getOwnedTerritories()) {
                if (!state.enemyTerritories.contains(territoryName)) {
                    state.enemyTerritories.append(territoryName);
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
                    // Use actual piece moves (UI may not be updated after startTurn)
                    info.movesRemaining = static_cast<int>(caesar->getMovesRemaining());
                    break;
                }
            }
        } else if (uiLeader.type == "General") {
            for (GeneralPiece *general : m_player->getGenerals()) {
                if (general->getSerialNumber() == uiLeader.serialNumber) {
                    info.piece = general;
                    info.name = QString("General %1").arg(general->getNumber());
                    info.legionSize = general->getLegion().size();
                    // Use actual piece moves (UI may not be updated after startTurn)
                    info.movesRemaining = static_cast<int>(general->getMovesRemaining());
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
    QList<int> currentLegion;
    if (leader->getType() == GamePiece::Type::Caesar) {
        leaderName = "Caesar";
        currentLegion = static_cast<CaesarPiece*>(leader)->getLegion();
        legionSize = currentLegion.size();
    } else if (leader->getType() == GamePiece::Type::General) {
        GeneralPiece *gen = qobject_cast<GeneralPiece*>(leader);
        if (gen) {
            leaderName = QString("General %1").arg(gen->getNumber());
            currentLegion = gen->getLegion();
            legionSize = currentLegion.size();
        }
    }

    // Count unassigned troops in the same territory that could be picked up
    int availableUnassignedTroops = 0;
    if (m_player && legionSize == 0) {
        // Build set of all troop IDs in ANY general's legion
        QSet<int> assignedTroopIds;
        for (GeneralPiece *g : m_player->getGenerals()) {
            for (int id : g->getLegion()) {
                assignedTroopIds.insert(id);
            }
        }
        // Count unassigned troops in this territory with moves remaining
        QList<GamePiece*> troopsHere = m_player->getPiecesAtTerritory(currentTerritory);
        for (GamePiece *troop : troopsHere) {
            if (troop->getType() == GamePiece::Type::Infantry ||
                troop->getType() == GamePiece::Type::Cavalry ||
                troop->getType() == GamePiece::Type::Catapult) {
                if (!assignedTroopIds.contains(troop->getUniqueId()) && troop->getMovesRemaining() > 0) {
                    availableUnassignedTroops++;
                }
            }
        }
    }

    // Effective troop count includes current legion + available unassigned troops
    int effectiveTroopCount = legionSize + availableUnassignedTroops;

    // Check if general should return to capital to pick up troops
    QString homeTerritory = m_player ? m_player->getHomeProvinceName() : "";
    int unassignedAtHome = 0;
    bool shouldReturnToCapital = false;

    if (m_player && legionSize < 6) {  // Has space in legion
        // Calculate quota to see if below
        int totalTroops = m_player->getInfantryCount() + m_player->getCavalryCount() + m_player->getCatapultCount();
        int numGenerals = m_player->getGenerals().size();
        if (numGenerals == 0) numGenerals = 1;
        int quota = totalTroops / numGenerals;

        // Count unassigned troops at home territory
        QSet<int> assignedIds;
        for (GeneralPiece *g : m_player->getGenerals()) {
            for (int id : g->getLegion()) assignedIds.insert(id);
        }
        QList<GamePiece*> homeTroops = m_player->getPiecesAtTerritory(homeTerritory);
        for (GamePiece *troop : homeTroops) {
            if ((troop->getType() == GamePiece::Type::Infantry ||
                 troop->getType() == GamePiece::Type::Cavalry ||
                 troop->getType() == GamePiece::Type::Catapult) &&
                !assignedIds.contains(troop->getUniqueId())) {
                unassignedAtHome++;
            }
        }

        // Should return if below quota and there are troops waiting at home
        shouldReturnToCapital = (legionSize < quota) && (unassignedAtHome > 0) && (currentTerritory != homeTerritory);
        if (shouldReturnToCapital) {
            log(QString("  General %1 should return to capital (legion: %2, quota: %3, unassigned at home: %4)")
                .arg(leaderName).arg(legionSize).arg(quota).arg(unassignedAtHome));
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
        // Key distinction: hasCombat is true for both:
        //   1. Enemy troops present (actual combat needed)
        //   2. Enemy-owned but undefended (free capture)
        // Check troopInfo to distinguish between these cases
        bool hasEnemyTroops = !option.troopInfo.isEmpty();

        if (option.hasCombat && hasEnemyTroops) {
            // Enemy troops present - actual combat
            // Cannot attack without troops!
            if (effectiveTroopCount == 0) {
                continue;  // Skip this move - can't enter combat without troops
            }
            move.score = scoreAttackMove(option.destinationTerritory, state);
            move.moveType = "Attack";
            move.reason = QString("Attack territory (owner: %1) - %2")
                .arg(option.owner == '\0' ? "none" : QString(option.owner))
                .arg(option.troopInfo);
        } else if (option.hasCombat && !hasEnemyTroops) {
            // Enemy-owned territory but NO defenders - free capture!
            // Generals CAN capture undefended enemy territories without troops
            move.score = scoreExpandMove(option.destinationTerritory, state) + 50;  // Bonus for enemy territory
            move.moveType = "Capture";
            move.reason = QString("Capture UNDEFENDED enemy territory (owner: %1, value: %2)")
                .arg(option.owner)
                .arg(option.territoryValue);
        } else if (option.isOwnTerritory) {
            // Own territory - reinforce
            move.score = scoreDefendMove(option.destinationTerritory, state);
            move.moveType = "Reinforce";
            move.reason = QString("Move to own territory%1")
                .arg(option.isViaRoad ? " [via road]" : "");

            // Boost score if moving to home territory to pick up troops
            // But scale based on how desperately the general needs troops
            if (shouldReturnToCapital && option.destinationTerritory == homeTerritory) {
                // If general has very few troops (0-1), high priority to return
                // If general has 2+ troops, lower priority - they can fight
                if (legionSize <= 1) {
                    move.score = 200;  // Urgent - need troops badly
                } else if (legionSize == 2) {
                    move.score = 80;  // Moderate - could use more troops
                } else {
                    move.score = 45;  // Low - already have enough to fight (below Attack)
                }
                move.moveType = "ReturnHome";
                move.reason = QString("Return to capital to pick up troops (%1 unassigned, legion: %2)")
                    .arg(unassignedAtHome).arg(legionSize);
            }

            // If general has no troops and can't expand, don't waste moves reinforcing
            // Score lower than Stay (10) so they don't bounce between territories
            if (effectiveTroopCount == 0 && unassignedAtHome == 0) {
                move.score = 5;  // Lower than Stay
                move.reason = "No troops - staying is better";
            }
        } else if (option.owner == '\0') {
            // Unclaimed territory - expand
            // Generals CAN claim unclaimed territories without troops (no combat needed)
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
            baseScore = 55;  // Higher than before - Economic still wants to expand/attack
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

    if (m_mapWidget && m_mapWidget->getGraph()) {
        int value = m_mapWidget->getGraph()->getValue(target);
        score += value;  // Higher value = higher score (105 or 110)
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
    log("Combat dialog opened - AI handling combat");
}

int AIPlayer::selectCombatTarget(const QList<GamePiece::Type> &targetTypes)
{
    if (targetTypes.isEmpty()) {
        return -1;
    }

    // Prioritize catapults - they give +1 advantage bonus
    QList<int> catapultIndices;
    for (int i = 0; i < targetTypes.size(); ++i) {
        if (targetTypes[i] == GamePiece::Type::Catapult) {
            catapultIndices.append(i);
        }
    }

    // If catapults available, pick one randomly
    if (!catapultIndices.isEmpty()) {
        int idx = rand() % catapultIndices.size();
        log(QString("AI targeting catapult (priority target) %1 of %2")
            .arg(idx + 1).arg(catapultIndices.size()));
        return catapultIndices[idx];
    }

    // Otherwise pick random target
    int idx = rand() % targetTypes.size();
    log(QString("AI targeting random unit %1 of %2")
        .arg(idx + 1).arg(targetTypes.size()));
    return idx;
}

void AIPlayer::handlePurchaseDialog(PurchaseDialog *dialog)
{
    if (!dialog) {
        log("ERROR: No purchase dialog provided");
        return;
    }

    log("Purchase dialog opened - making AI purchase decisions");

    // Get available items from the dialog
    QList<PurchaseDialog::PurchaseMenuItem> availableItems = dialog->getAvailableItems();

    // Extract information needed for decision making
    int budget = 0;
    int inflationMultiplier = 1;
    QStringList territoriesForCities;
    QStringList territoriesForFortification;
    QStringList seaTerritoriesForGalleys;
    int currentGalleyCount = m_player->getGalleys().size();

    // Parse available items to understand what's available
    for (const auto &item : availableItems) {
        // Determine inflation from infantry price (base is 10)
        if (item.itemType == "Infantry" && item.currentPrice > 0) {
            inflationMultiplier = item.currentPrice / 10;
        }

        // Collect city placement options
        if (item.itemType == "City" && !item.location.isEmpty()) {
            territoriesForCities.append(item.location);
        }

        // Collect fortification options
        if (item.itemType == "Fortification" && !item.location.isEmpty()) {
            territoriesForFortification.append(item.location);
        }

        // Collect galley placement options
        if (item.itemType == "Galley" && !item.location.isEmpty()) {
            seaTerritoriesForGalleys.append(item.location);
        }
    }

    // Get budget from player's wallet
    budget = m_player->getWallet();

    log(QString("Purchase analysis: Budget=%1, Inflation=%2x, CitySpots=%3, GalleySpots=%4")
        .arg(budget)
        .arg(inflationMultiplier)
        .arg(territoriesForCities.size())
        .arg(seaTerritoriesForGalleys.size()));

    // Get purchase decision from AI decision maker
    const QList<Player*> &allPlayers = m_infoWidget->getPlayers();
    MapGraph *graph = m_mapWidget->getGraph();

    AIPurchaseDecision decision = m_decisionMaker.decidePurchases(
        m_player,
        allPlayers,
        graph,
        budget,
        inflationMultiplier,
        territoriesForCities,
        territoriesForFortification,
        seaTerritoriesForGalleys,
        currentGalleyCount
    );

    log(QString("Purchase decision: %1").arg(decision.reason));

    // Convert decision to the format expected by the dialog
    QMap<QString, int> purchases;

    // Cities to DESTROY (strategic decision - deny enemy the prize)
    for (const QString &territory : decision.citiesToDestroy) {
        purchases[QString("DestroyCity:%1").arg(territory)] = 1;
        log(QString("  -> DESTROY city at %1 (can't defend)").arg(territory));
    }

    if (decision.infantry > 0) {
        purchases["Infantry"] = decision.infantry;
        log(QString("  -> Infantry: %1").arg(decision.infantry));
    }
    if (decision.cavalry > 0) {
        purchases["Cavalry"] = decision.cavalry;
        log(QString("  -> Cavalry: %1").arg(decision.cavalry));
    }
    if (decision.catapults > 0) {
        purchases["Catapults"] = decision.catapults;
        log(QString("  -> Catapults: %1").arg(decision.catapults));
    }

    // Cities
    for (auto it = decision.cities.begin(); it != decision.cities.end(); ++it) {
        QString key = it.value() ? QString("FortifiedCity:%1").arg(it.key())
                                 : QString("City:%1").arg(it.key());
        purchases[key] = 1;
        log(QString("  -> City at %1%2").arg(it.key()).arg(it.value() ? " (fortified)" : ""));
    }

    // Fortifications
    for (const QString &territory : decision.fortifications) {
        purchases[QString("Fortification:%1").arg(territory)] = 1;
        log(QString("  -> Fortify %1").arg(territory));
    }

    // Galleys
    for (auto it = decision.galleys.begin(); it != decision.galleys.end(); ++it) {
        purchases[QString("Galley:%1").arg(it.key())] = it.value();
        log(QString("  -> Galley at %1: %2").arg(it.key()).arg(it.value()));
    }

    // Setup auto-mode to execute purchases
    dialog->setupAIAutoMode(m_delayMs, purchases);

    emit purchasePlanUpdated(budget, PurchaseDecision{
        decision.infantry,
        decision.cavalry,
        decision.catapults,
        {}, // cityTerritories - not used in signal
        {}, // fortifyTerritories - not used in signal
        0,  // galleys count
        decision.totalCost
    });
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

QList<int> AIPlayer::decideLegionComposition(GamePiece *leader, const QList<GamePiece*> &availableTroops)
{
    QList<int> troopsToSelect;

    if (!leader || !m_player) {
        return troopsToSelect;
    }

    // Get the leader's current legion - handle both Caesar and General
    QList<int> currentLegion;
    bool isCaesar = false;
    int leaderNumber = 0;

    if (leader->getType() == GamePiece::Type::Caesar) {
        CaesarPiece *caesar = static_cast<CaesarPiece*>(leader);
        currentLegion = caesar->getLegion();
        isCaesar = true;
        log("decideLegionComposition: Processing Caesar");
    } else if (leader->getType() == GamePiece::Type::General) {
        GeneralPiece *gen = static_cast<GeneralPiece*>(leader);
        currentLegion = gen->getLegion();
        leaderNumber = gen->getNumber();
    } else {
        log("decideLegionComposition: Unknown leader type, returning empty list");
        return troopsToSelect;
    }

    int currentLegionSize = currentLegion.size();
    QString leaderName = isCaesar ? "Caesar" : QString("General %1").arg(leaderNumber);

    // === NEW PLANNING-BASED APPROACH ===
    // Check if we have a plan and this general has an assignment
    int plannedTroops = 0;
    QList<int> plannedTroopIds;
    bool hasPlannedAssignment = false;
    QString missionType;

    if (m_planCreated && !m_currentPlan.isEmpty()) {
        for (const GeneralAssignment &assignment : m_currentPlan.assignments) {
            if (assignment.general == leader) {
                hasPlannedAssignment = true;
                plannedTroops = assignment.troopsToTake;
                plannedTroopIds = assignment.troopIds;
                missionType = assignment.missionType;
                log(QString("Legion Building: %1 has PLANNED assignment: %2 troops for %3")
                    .arg(leaderName).arg(plannedTroops).arg(missionType));
                break;
            }
        }
    }

    // ReturnHome mission: general should bring ALL their current troops with them!
    // Defend mission: general should bring their current troops PLUS any planned troops
    if (hasPlannedAssignment && (missionType == "ReturnHome" || missionType == "Defend")) {
        // Keep troops currently in the legion that still have moves remaining
        // Troops with 0 moves must be left behind - the general will continue without them
        for (int troopId : currentLegion) {
            for (GamePiece *troop : availableTroops) {
                if (troop->getUniqueId() == troopId && troop->getMovesRemaining() > 0) {
                    troopsToSelect.append(troopId);
                    break;
                }
            }
        }

        // Log if we had to leave troops behind
        if (troopsToSelect.size() < currentLegionSize) {
            log(QString("Legion Building: %1 leaving %2 troops behind (no moves remaining)")
                .arg(leaderName).arg(currentLegionSize - troopsToSelect.size()));
        }

        // For DEFEND missions, also pick up the planned troops (general might be picking up reinforcements)
        if (missionType == "Defend" && !plannedTroopIds.isEmpty()) {
            for (int troopId : plannedTroopIds) {
                if (troopsToSelect.contains(troopId)) continue;  // Already added

                bool available = false;
                for (GamePiece *troop : availableTroops) {
                    if (troop->getUniqueId() == troopId && troop->getMovesRemaining() > 0) {
                        available = true;
                        break;
                    }
                }
                if (available) {
                    troopsToSelect.append(troopId);
                }
            }

            // FALLBACK: If planned troops weren't available, pick up ANY available troops
            if (troopsToSelect.size() < plannedTroops) {
                for (GamePiece *troop : availableTroops) {
                    if (troopsToSelect.size() >= plannedTroops) break;
                    if (troopsToSelect.contains(troop->getUniqueId())) continue;
                    if (troop->getMovesRemaining() <= 0) continue;

                    troopsToSelect.append(troop->getUniqueId());
                }
                log(QString("Legion Building: %1 DEFENDING FALLBACK - picking up %2 available troops (planned %3 unavailable)")
                    .arg(leaderName).arg(troopsToSelect.size()).arg(plannedTroops));
            } else {
                log(QString("Legion Building: %1 DEFENDING - picking up %2 troops (had %3, adding %4 planned)")
                    .arg(leaderName)
                    .arg(troopsToSelect.size())
                    .arg(currentLegionSize)
                    .arg(plannedTroopIds.size()));
            }
            return troopsToSelect;
        }

        log(QString("Legion Building: %1 %2 - keeping %3 troops in legion")
            .arg(leaderName)
            .arg(missionType == "Defend" ? "DEFENDING" : "RETURNING HOME")
            .arg(troopsToSelect.size()));
        return troopsToSelect;
    }

    // If we have a planned assignment with specific troops, use those
    if (hasPlannedAssignment && !plannedTroopIds.isEmpty()) {
        // First, add troops already in this leader's legion that still have moves
        // Troops with 0 moves must be left behind
        for (int troopId : currentLegion) {
            for (GamePiece *troop : availableTroops) {
                if (troop->getUniqueId() == troopId && troop->getMovesRemaining() > 0) {
                    troopsToSelect.append(troopId);
                    break;
                }
            }
        }

        // Log if we had to leave troops behind
        if (troopsToSelect.size() < currentLegionSize) {
            log(QString("Legion Building: %1 leaving %2 troops behind (no moves remaining)")
                .arg(leaderName).arg(currentLegionSize - troopsToSelect.size()));
        }

        // Then add the specifically planned troops (if not already added)
        for (int troopId : plannedTroopIds) {
            if (troopsToSelect.contains(troopId)) continue;

            // Verify this troop is available
            bool available = false;
            for (GamePiece *troop : availableTroops) {
                if (troop->getUniqueId() == troopId && troop->getMovesRemaining() > 0) {
                    available = true;
                    break;
                }
            }
            if (available) {
                troopsToSelect.append(troopId);
            }
        }

        // If planned troops weren't available but we need troops (Defend/Attack missions),
        // pick up ANY available troops with moves remaining
        if (troopsToSelect.size() < plannedTroops &&
            (missionType == "Defend" || missionType == "Attack")) {
            for (GamePiece *troop : availableTroops) {
                if (troopsToSelect.size() >= plannedTroops) break;
                if (troopsToSelect.contains(troop->getUniqueId())) continue;
                if (troop->getMovesRemaining() <= 0) continue;

                troopsToSelect.append(troop->getUniqueId());
            }
            log(QString("Legion Building: %1 FALLBACK - picking up %2 available troops (planned %3 unavailable)")
                .arg(leaderName).arg(troopsToSelect.size()).arg(plannedTroops));
        } else {
            log(QString("Legion Building: %1 using PLANNED composition: %2 troops selected")
                .arg(leaderName).arg(troopsToSelect.size()));
        }
        return troopsToSelect;
    }

    // If no plan or expansion mission (0 troops), use minimal troops
    if (hasPlannedAssignment && plannedTroops == 0) {
        // Expansion mission - don't take any troops (save for attack missions)
        log(QString("Legion Building: %1 is on EXPANSION mission - taking NO troops").arg(leaderName));
        return troopsToSelect;  // Empty list
    }

    // === FALLBACK: Old quota-based approach if no plan ===
    // (This path is taken if plan creation failed or for Caesar)

    // CRITICAL: Caesar is EXTREMELY valuable - losing Caesar loses the game!
    int minTroops = isCaesar ? 6 : 0;

    // Calculate quota: total troops / (6 generals + 1 for Caesar)
    int totalTroops = m_player->getInfantryCount() +
                      m_player->getCavalryCount() +
                      m_player->getCatapultCount();
    int numGenerals = m_player->getGenerals().size();
    int numLeaders = numGenerals + 1;
    if (numLeaders == 0) numLeaders = 1;

    int quota = totalTroops / numLeaders;
    int remainder = totalTroops % numLeaders;
    if (leaderNumber <= remainder) {
        quota += 1;
    }

    if (isCaesar && quota < minTroops) {
        quota = minTroops;
    }

    log(QString("Legion Building: %1 - FALLBACK quota mode: Current=%2, Quota=%3")
        .arg(leaderName).arg(currentLegionSize).arg(quota));

    // Build set of all troop IDs in ANY leader's legion (to identify assigned troops)
    QSet<int> assignedTroopIds;
    // Include Caesar's legion (player should only have 1 Caesar, but use list for safety)
    for (CaesarPiece *playerCaesar : m_player->getCaesars()) {
        if (!isCaesar || playerCaesar != leader) {  // Don't exclude our own legion
            for (int id : playerCaesar->getLegion()) {
                assignedTroopIds.insert(id);
            }
        }
    }
    // Include all generals' legions
    for (GeneralPiece *g : m_player->getGenerals()) {
        if (isCaesar || g != leader) {  // Don't exclude our own legion
            for (int id : g->getLegion()) {
                assignedTroopIds.insert(id);
            }
        }
    }
    log(QString("  Total assigned troops across all generals: %1").arg(assignedTroopIds.size()));
    log(QString("  Available troops in dialog: %1").arg(availableTroops.size()));

    // Separate troops by category for balanced selection
    QList<GamePiece*> unassignedInfantry;
    QList<GamePiece*> unassignedCavalry;
    QList<GamePiece*> unassignedCatapults;

    // First pass: add permanent legion members and categorize unassigned troops
    for (GamePiece *troop : availableTroops) {
        int troopId = troop->getUniqueId();

        // Rule 1: If troop is in THIS general's legion and has moves, select it (permanent)
        if (currentLegion.contains(troopId)) {
            if (troop->getMovesRemaining() > 0) {
                troopsToSelect.append(troopId);
                log(QString("  Troop %1: In this general's legion - SELECTED (permanent)").arg(troopId));
            } else {
                log(QString("  Troop %1: In this general's legion but NO MOVES - SKIP").arg(troopId));
            }
            continue;
        }

        // Rule 2: If troop is in ANOTHER general's legion, skip it
        if (assignedTroopIds.contains(troopId)) {
            log(QString("  Troop %1: In another general's legion - SKIP").arg(troopId));
            continue;
        }

        // Rule 3: Categorize unassigned troops with moves remaining
        if (troop->getMovesRemaining() > 0) {
            if (troop->getType() == GamePiece::Type::Infantry) {
                unassignedInfantry.append(troop);
            } else if (troop->getType() == GamePiece::Type::Cavalry) {
                unassignedCavalry.append(troop);
            } else if (troop->getType() == GamePiece::Type::Catapult) {
                unassignedCatapults.append(troop);
            }
        } else {
            log(QString("  Troop %1: Unassigned but no moves remaining - SKIP").arg(troopId));
        }
    }

    log(QString("  Unassigned available: %1 infantry, %2 cavalry, %3 catapults")
        .arg(unassignedInfantry.size()).arg(unassignedCavalry.size()).arg(unassignedCatapults.size()));

    // Second pass: balanced selection from unassigned troops (catapult > cavalry > infantry priority)
    currentLegionSize = troopsToSelect.size();  // Update after adding permanent members
    int iInf = 0, iCav = 0, iCat = 0;

    while (currentLegionSize < 6 && currentLegionSize < quota) {
        bool addedAny = false;

        // Try to add one catapult
        if (iCat < unassignedCatapults.size() && currentLegionSize < 6 && currentLegionSize < quota) {
            GamePiece *troop = unassignedCatapults[iCat++];
            troopsToSelect.append(troop->getUniqueId());
            currentLegionSize++;
            log(QString("  Troop %1 (Catapult): Unassigned, balanced selection - SELECTED").arg(troop->getUniqueId()));
            addedAny = true;
        }

        // Try to add one cavalry
        if (iCav < unassignedCavalry.size() && currentLegionSize < 6 && currentLegionSize < quota) {
            GamePiece *troop = unassignedCavalry[iCav++];
            troopsToSelect.append(troop->getUniqueId());
            currentLegionSize++;
            log(QString("  Troop %1 (Cavalry): Unassigned, balanced selection - SELECTED").arg(troop->getUniqueId()));
            addedAny = true;
        }

        // Try to add one infantry
        if (iInf < unassignedInfantry.size() && currentLegionSize < 6 && currentLegionSize < quota) {
            GamePiece *troop = unassignedInfantry[iInf++];
            troopsToSelect.append(troop->getUniqueId());
            currentLegionSize++;
            log(QString("  Troop %1 (Infantry): Unassigned, balanced selection - SELECTED").arg(troop->getUniqueId()));
            addedAny = true;
        }

        // If we couldn't add any, we're out of unassigned troops
        if (!addedAny) break;
    }

    // CRITICAL: For Caesar, if we're below minTroops but there are still unassigned troops,
    // keep adding until we reach minTroops (even if over quota)
    if (isCaesar && currentLegionSize < minTroops) {
        log(QString("  WARNING: Caesar only has %1 troops, need at least %2 - adding more!")
            .arg(currentLegionSize).arg(minTroops));

        // Try to add more troops in priority order: catapult > cavalry > infantry
        while (currentLegionSize < minTroops && currentLegionSize < 6) {
            bool addedAny = false;

            if (iCat < unassignedCatapults.size()) {
                GamePiece *troop = unassignedCatapults[iCat++];
                troopsToSelect.append(troop->getUniqueId());
                currentLegionSize++;
                log(QString("  Troop %1 (Catapult): Emergency add for Caesar - SELECTED").arg(troop->getUniqueId()));
                addedAny = true;
            } else if (iCav < unassignedCavalry.size()) {
                GamePiece *troop = unassignedCavalry[iCav++];
                troopsToSelect.append(troop->getUniqueId());
                currentLegionSize++;
                log(QString("  Troop %1 (Cavalry): Emergency add for Caesar - SELECTED").arg(troop->getUniqueId()));
                addedAny = true;
            } else if (iInf < unassignedInfantry.size()) {
                GamePiece *troop = unassignedInfantry[iInf++];
                troopsToSelect.append(troop->getUniqueId());
                currentLegionSize++;
                log(QString("  Troop %1 (Infantry): Emergency add for Caesar - SELECTED").arg(troop->getUniqueId()));
                addedAny = true;
            }

            if (!addedAny) break;
        }

        if (currentLegionSize < minTroops) {
            log(QString("  CRITICAL WARNING: Caesar has only %1 troops, wanted %2 - no more available!")
                .arg(currentLegionSize).arg(minTroops));
        }
    }

    log(QString("Legion Building: Selected %1 troops for %2").arg(troopsToSelect.size()).arg(leaderName));
    return troopsToSelect;
}

bool AIPlayer::transferTroopsToOtherGeneral(GeneralPiece *fromGeneral)
{
    if (!fromGeneral || !m_player) {
        return false;
    }

    QString territory = fromGeneral->getTerritoryName();
    QList<int> fromLegion = fromGeneral->getLegion();

    if (fromLegion.isEmpty()) {
        log(QString("Transfer: General #%1 has no troops to transfer").arg(fromGeneral->getNumber()));
        return false;
    }

    // Find another general at the same territory who could use more troops
    GeneralPiece *bestRecipient = nullptr;
    int bestRecipientNeed = 0;  // How many more troops they could use (6 - current legion size)

    for (GeneralPiece *otherGen : m_player->getGenerals()) {
        if (otherGen == fromGeneral) continue;
        if (otherGen->getTerritoryName() != territory) continue;

        int currentLegionSize = otherGen->getLegion().size();
        int spaceAvailable = 6 - currentLegionSize;

        // Prefer generals who:
        // 1. Have space for more troops
        // 2. Have fewer troops than fromGeneral (so transfer makes sense)
        // 3. Are NOT heading home (have troops already or are at a strategic position)
        if (spaceAvailable > 0 && currentLegionSize < fromLegion.size()) {
            if (spaceAvailable > bestRecipientNeed) {
                bestRecipient = otherGen;
                bestRecipientNeed = spaceAvailable;
            }
        }
    }

    // Also check Caesar at the same territory
    for (CaesarPiece *caesar : m_player->getCaesars()) {
        if (caesar->getTerritoryName() != territory) continue;

        int currentLegionSize = caesar->getLegion().size();
        int spaceAvailable = 6 - currentLegionSize;

        // Caesar always gets priority for troops (he needs protection)
        if (spaceAvailable > 0) {
            // Transfer to Caesar even if he has more troops than fromGeneral
            // because Caesar's safety is paramount
            log(QString("Transfer: Found Caesar at %1 with space for %2 more troops")
                .arg(territory).arg(spaceAvailable));

            // Transfer troops to Caesar
            int transferred = 0;
            QList<int> newFromLegion = fromLegion;

            for (int troopId : fromLegion) {
                if (transferred >= spaceAvailable) break;

                // Move troop from general to Caesar
                newFromLegion.removeOne(troopId);
                caesar->addToLegion(troopId);
                transferred++;

                log(QString("  Transferred troop %1 to Caesar").arg(troopId));
            }

            fromGeneral->setLegion(newFromLegion);
            log(QString("Transfer: Gave %1 troops to Caesar, General #%2 now has %3 troops")
                .arg(transferred).arg(fromGeneral->getNumber()).arg(newFromLegion.size()));

            return transferred > 0;
        }
    }

    if (!bestRecipient) {
        log(QString("Transfer: No suitable recipient found at %1 for General #%2's troops")
            .arg(territory).arg(fromGeneral->getNumber()));
        return false;
    }

    // Transfer troops to the best recipient
    int transferred = 0;
    QList<int> newFromLegion = fromLegion;
    int maxToTransfer = qMin(bestRecipientNeed, fromLegion.size());

    // Keep at least some troops if we're not heading home
    // But if we ARE heading home to get more troops, transfer all
    QString homeProvince = m_player->getHomeProvinceName();
    bool headingHome = false;  // Caller should determine this - for now, transfer all

    for (int troopId : fromLegion) {
        if (transferred >= maxToTransfer) break;

        // Move troop from one general to another
        newFromLegion.removeOne(troopId);
        bestRecipient->addToLegion(troopId);
        transferred++;

        log(QString("  Transferred troop %1 to General #%2").arg(troopId).arg(bestRecipient->getNumber()));
    }

    fromGeneral->setLegion(newFromLegion);
    log(QString("Transfer: Gave %1 troops to General #%2, General #%3 now has %4 troops")
        .arg(transferred)
        .arg(bestRecipient->getNumber())
        .arg(fromGeneral->getNumber())
        .arg(newFromLegion.size()));

    return transferred > 0;
}

bool AIPlayer::canGeneralMove(GamePiece *general) const
{
    if (!general || !m_player) {
        return false;
    }

    // A leader can always move if they have moves remaining
    // They may not be able to bring all troops, but they can move alone
    if (general->getMovesRemaining() <= 0) {
        return false;
    }

    // Leader has moves - they can move (possibly without troops)
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

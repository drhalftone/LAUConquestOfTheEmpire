#include "aidebugwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QSettings>
#include <QCloseEvent>
#include <QFont>

AIDebugWidget::AIDebugWidget(QWidget *parent)
    : QWidget(parent)
    , m_aiPlayer(nullptr)
{
    setWindowTitle("AI Debug");
    setupUI();
    loadSettings();
}

AIDebugWidget::~AIDebugWidget()
{
    saveSettings();
}

void AIDebugWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // === Header Section ===
    QHBoxLayout *headerLayout = new QHBoxLayout();
    m_playerLabel = new QLabel("AI Player: -");
    m_playerLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_phaseLabel = new QLabel("Phase: Idle");
    m_phaseLabel->setStyleSheet("font-size: 12px; color: #666;");
    headerLayout->addWidget(m_playerLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_phaseLabel);
    mainLayout->addLayout(headerLayout);

    // === Main Content Area (Splitter) ===
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal);

    // Left panel: State + Leaders
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(createStateSection());
    leftLayout->addWidget(createLeadersSection());
    leftPanel->setMaximumWidth(250);

    // Middle panel: Log
    QWidget *middlePanel = new QWidget();
    QVBoxLayout *middleLayout = new QVBoxLayout(middlePanel);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->addWidget(createLogSection());

    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(middlePanel);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);

    mainLayout->addWidget(mainSplitter, 1);

    // === Move Evaluation Section ===
    mainLayout->addWidget(createMoveEvalSection());

    // === Bottom Row: Combat + Purchase ===
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(createCombatSection());
    bottomLayout->addWidget(createPurchaseSection());
    mainLayout->addLayout(bottomLayout);

    // === Controls Section ===
    mainLayout->addWidget(createControlsSection());

    setMinimumSize(700, 600);
    resize(800, 700);
}

QGroupBox* AIDebugWidget::createStateSection()
{
    QGroupBox *groupBox = new QGroupBox("Current State");
    QVBoxLayout *layout = new QVBoxLayout(groupBox);
    layout->setSpacing(4);

    m_walletLabel = new QLabel("Wallet: -");
    m_territoriesLabel = new QLabel("Territories: -");
    m_piecesLabel = new QLabel("Pieces: -");

    layout->addWidget(m_walletLabel);
    layout->addWidget(m_territoriesLabel);
    layout->addWidget(m_piecesLabel);

    return groupBox;
}

QGroupBox* AIDebugWidget::createLeadersSection()
{
    QGroupBox *groupBox = new QGroupBox("Leaders");
    QVBoxLayout *layout = new QVBoxLayout(groupBox);

    m_leadersListWidget = new QListWidget();
    m_leadersListWidget->setAlternatingRowColors(true);
    m_leadersListWidget->setMaximumHeight(150);
    layout->addWidget(m_leadersListWidget);

    return groupBox;
}

QGroupBox* AIDebugWidget::createLogSection()
{
    QGroupBox *groupBox = new QGroupBox("Action Log");
    QVBoxLayout *layout = new QVBoxLayout(groupBox);

    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Consolas", 9));
    m_logTextEdit->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");
    layout->addWidget(m_logTextEdit);

    m_clearLogButton = new QPushButton("Clear Log");
    connect(m_clearLogButton, &QPushButton::clicked, this, &AIDebugWidget::onClearLogClicked);
    layout->addWidget(m_clearLogButton);

    return groupBox;
}

QGroupBox* AIDebugWidget::createMoveEvalSection()
{
    QGroupBox *groupBox = new QGroupBox("Move Evaluation");
    QVBoxLayout *layout = new QVBoxLayout(groupBox);

    m_evalLeaderLabel = new QLabel("Evaluating: -");
    layout->addWidget(m_evalLeaderLabel);

    m_moveTable = new QTableWidget();
    m_moveTable->setColumnCount(4);
    m_moveTable->setHorizontalHeaderLabels({"Target", "Score", "Type", "Reason"});
    m_moveTable->horizontalHeader()->setStretchLastSection(true);
    m_moveTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_moveTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_moveTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_moveTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_moveTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_moveTable->setAlternatingRowColors(true);
    m_moveTable->setMaximumHeight(150);
    layout->addWidget(m_moveTable);

    return groupBox;
}

QGroupBox* AIDebugWidget::createCombatSection()
{
    m_combatGroupBox = new QGroupBox("Combat Analysis");
    QVBoxLayout *layout = new QVBoxLayout(m_combatGroupBox);
    layout->setSpacing(4);

    m_combatTerritoryLabel = new QLabel("Territory: -");
    m_myForcesLabel = new QLabel("My forces: -");
    m_enemyForcesLabel = new QLabel("Enemy forces: -");

    m_winChanceBar = new QProgressBar();
    m_winChanceBar->setRange(0, 100);
    m_winChanceBar->setValue(0);
    m_winChanceBar->setFormat("Win chance: %p%");
    m_winChanceBar->setTextVisible(true);

    layout->addWidget(m_combatTerritoryLabel);
    layout->addWidget(m_myForcesLabel);
    layout->addWidget(m_enemyForcesLabel);
    layout->addWidget(m_winChanceBar);
    layout->addStretch();

    return m_combatGroupBox;
}

QGroupBox* AIDebugWidget::createPurchaseSection()
{
    m_purchaseGroupBox = new QGroupBox("Purchase Plan");
    QVBoxLayout *layout = new QVBoxLayout(m_purchaseGroupBox);
    layout->setSpacing(4);

    m_budgetLabel = new QLabel("Budget: -");
    layout->addWidget(m_budgetLabel);

    m_purchaseListWidget = new QListWidget();
    m_purchaseListWidget->setMaximumHeight(80);
    layout->addWidget(m_purchaseListWidget);

    m_remainingLabel = new QLabel("Remaining: -");
    layout->addWidget(m_remainingLabel);
    layout->addStretch();

    return m_purchaseGroupBox;
}

QWidget* AIDebugWidget::createControlsSection()
{
    QWidget *controlsWidget = new QWidget();
    controlsWidget->setStyleSheet("background-color: #f0f0f0; border-radius: 4px;");
    QHBoxLayout *layout = new QHBoxLayout(controlsWidget);
    layout->setContentsMargins(8, 8, 8, 8);

    // Step button
    m_stepButton = new QPushButton("Step");
    m_stepButton->setToolTip("Execute one action");
    connect(m_stepButton, &QPushButton::clicked, this, &AIDebugWidget::onStepClicked);
    layout->addWidget(m_stepButton);

    // Auto button
    m_autoButton = new QPushButton("Auto");
    m_autoButton->setToolTip("Run continuously");
    m_autoButton->setCheckable(true);
    connect(m_autoButton, &QPushButton::clicked, this, &AIDebugWidget::onAutoClicked);
    layout->addWidget(m_autoButton);

    // Pause button
    m_pauseButton = new QPushButton("Pause");
    m_pauseButton->setToolTip("Pause execution");
    connect(m_pauseButton, &QPushButton::clicked, this, &AIDebugWidget::onPauseClicked);
    layout->addWidget(m_pauseButton);

    layout->addSpacing(20);

    // Speed slider
    QLabel *speedTextLabel = new QLabel("Speed:");
    layout->addWidget(speedTextLabel);

    m_speedSlider = new QSlider(Qt::Horizontal);
    m_speedSlider->setRange(50, 2000);
    m_speedSlider->setValue(500);
    m_speedSlider->setMaximumWidth(150);
    m_speedSlider->setToolTip("Delay between actions (ms)");
    connect(m_speedSlider, &QSlider::valueChanged, this, &AIDebugWidget::onSpeedChanged);
    layout->addWidget(m_speedSlider);

    m_speedLabel = new QLabel("500ms");
    m_speedLabel->setMinimumWidth(50);
    layout->addWidget(m_speedLabel);

    layout->addSpacing(20);

    // Strategy combo
    QLabel *strategyTextLabel = new QLabel("Strategy:");
    layout->addWidget(strategyTextLabel);

    m_strategyCombo = new QComboBox();
    m_strategyCombo->addItem("Random", static_cast<int>(AIPlayer::Strategy::Random));
    m_strategyCombo->addItem("Aggressive", static_cast<int>(AIPlayer::Strategy::Aggressive));
    m_strategyCombo->addItem("Defensive", static_cast<int>(AIPlayer::Strategy::Defensive));
    m_strategyCombo->addItem("Economic", static_cast<int>(AIPlayer::Strategy::Economic));
    connect(m_strategyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AIDebugWidget::onStrategyChanged);
    layout->addWidget(m_strategyCombo);

    layout->addStretch();

    return controlsWidget;
}

void AIDebugWidget::setAIPlayer(AIPlayer *ai)
{
    // Disconnect old player
    if (m_aiPlayer) {
        disconnect(m_aiPlayer, nullptr, this, nullptr);
    }

    m_aiPlayer = ai;

    if (m_aiPlayer) {
        // Connect signals
        connect(m_aiPlayer, &AIPlayer::phaseChanged,
                this, &AIDebugWidget::onPhaseChanged);
        connect(m_aiPlayer, &AIPlayer::stateUpdated,
                this, &AIDebugWidget::onStateUpdated);
        connect(m_aiPlayer, &AIPlayer::leadersUpdated,
                this, &AIDebugWidget::onLeadersUpdated);
        connect(m_aiPlayer, &AIPlayer::movesEvaluated,
                this, &AIDebugWidget::onMovesEvaluated);
        connect(m_aiPlayer, &AIPlayer::moveSelected,
                this, &AIDebugWidget::onMoveSelected);
        connect(m_aiPlayer, &AIPlayer::actionTaken,
                this, &AIDebugWidget::onActionTaken);
        connect(m_aiPlayer, &AIPlayer::combatAnalysisUpdated,
                this, &AIDebugWidget::onCombatAnalysisUpdated);
        connect(m_aiPlayer, &AIPlayer::purchasePlanUpdated,
                this, &AIDebugWidget::onPurchasePlanUpdated);
        connect(m_aiPlayer, &AIPlayer::waitingForStep,
                this, &AIDebugWidget::onWaitingForStep);

        // Update window title
        setWindowTitle(QString("AI Debug - Player %1").arg(m_aiPlayer->getPlayerId()));
        m_playerLabel->setText(QString("AI Player: %1").arg(m_aiPlayer->getPlayerId()));

        // Set initial values from AI
        m_speedSlider->setValue(m_aiPlayer->getDelayMs());

        int strategyIndex = m_strategyCombo->findData(static_cast<int>(m_aiPlayer->getStrategy()));
        if (strategyIndex >= 0) {
            m_strategyCombo->setCurrentIndex(strategyIndex);
        }
    } else {
        setWindowTitle("AI Debug");
        m_playerLabel->setText("AI Player: -");
    }
}

void AIDebugWidget::clear()
{
    m_logTextEdit->clear();
    m_moveTable->setRowCount(0);
    m_leadersListWidget->clear();
    m_purchaseListWidget->clear();

    m_phaseLabel->setText("Phase: Idle");
    m_walletLabel->setText("Wallet: -");
    m_territoriesLabel->setText("Territories: -");
    m_piecesLabel->setText("Pieces: -");
    m_evalLeaderLabel->setText("Evaluating: -");
    m_combatTerritoryLabel->setText("Territory: -");
    m_myForcesLabel->setText("My forces: -");
    m_enemyForcesLabel->setText("Enemy forces: -");
    m_winChanceBar->setValue(0);
    m_budgetLabel->setText("Budget: -");
    m_remainingLabel->setText("Remaining: -");
}

QString AIDebugWidget::phaseToString(AIPlayer::Phase phase)
{
    switch (phase) {
        case AIPlayer::Phase::Idle: return "Idle";
        case AIPlayer::Phase::ReadingState: return "Reading State";
        case AIPlayer::Phase::Movement: return "Movement";
        case AIPlayer::Phase::Combat: return "Combat";
        case AIPlayer::Phase::CityDestruction: return "City Destruction";
        case AIPlayer::Phase::Purchase: return "Purchase";
        case AIPlayer::Phase::TurnComplete: return "Turn Complete";
    }
    return "Unknown";
}

// ============================================================================
// Slots - AIPlayer signals
// ============================================================================

void AIDebugWidget::onPhaseChanged(AIPlayer::Phase phase)
{
    QString phaseStr = phaseToString(phase);
    m_phaseLabel->setText(QString("Phase: %1").arg(phaseStr));

    // Highlight phase label based on phase
    QString color = "#666";
    switch (phase) {
        case AIPlayer::Phase::Movement: color = "#2196F3"; break;  // Blue
        case AIPlayer::Phase::Combat: color = "#f44336"; break;    // Red
        case AIPlayer::Phase::Purchase: color = "#4CAF50"; break;  // Green
        case AIPlayer::Phase::TurnComplete: color = "#9C27B0"; break;  // Purple
        default: break;
    }
    m_phaseLabel->setStyleSheet(QString("font-size: 12px; color: %1; font-weight: bold;").arg(color));
}

void AIDebugWidget::onStateUpdated(int wallet, int territoryCount, int pieceCount)
{
    m_walletLabel->setText(QString("Wallet: %1 talents").arg(wallet));
    m_territoriesLabel->setText(QString("Territories: %1").arg(territoryCount));
    m_piecesLabel->setText(QString("Pieces: %1").arg(pieceCount));
}

void AIDebugWidget::onLeadersUpdated(const QStringList &leaderDescriptions)
{
    m_leadersListWidget->clear();
    for (const QString &desc : leaderDescriptions) {
        m_leadersListWidget->addItem(desc);
    }
}

void AIDebugWidget::onMovesEvaluated(const QList<MoveEvaluation> &evaluations)
{
    m_moveTable->setRowCount(0);

    if (evaluations.isEmpty()) {
        m_evalLeaderLabel->setText("Evaluating: No moves available");
        return;
    }

    m_evalLeaderLabel->setText(QString("Evaluating: %1 @ %2")
        .arg(evaluations[0].leaderName)
        .arg(evaluations[0].fromTerritory));

    for (const MoveEvaluation &eval : evaluations) {
        int row = m_moveTable->rowCount();
        m_moveTable->insertRow(row);

        QTableWidgetItem *targetItem = new QTableWidgetItem(eval.targetTerritory);
        QTableWidgetItem *scoreItem = new QTableWidgetItem(QString::number(eval.score));
        QTableWidgetItem *typeItem = new QTableWidgetItem(eval.moveType);
        QTableWidgetItem *reasonItem = new QTableWidgetItem(eval.reason);

        // Color code by move type
        QColor bgColor = Qt::white;
        if (eval.moveType == "Attack") bgColor = QColor(255, 200, 200);
        else if (eval.moveType == "Expand") bgColor = QColor(200, 255, 200);
        else if (eval.moveType == "Reinforce") bgColor = QColor(200, 200, 255);
        else if (eval.moveType == "Stay") bgColor = QColor(240, 240, 240);

        targetItem->setBackground(bgColor);
        scoreItem->setBackground(bgColor);
        typeItem->setBackground(bgColor);
        reasonItem->setBackground(bgColor);

        m_moveTable->setItem(row, 0, targetItem);
        m_moveTable->setItem(row, 1, scoreItem);
        m_moveTable->setItem(row, 2, typeItem);
        m_moveTable->setItem(row, 3, reasonItem);
    }
}

void AIDebugWidget::onMoveSelected(const MoveEvaluation &move)
{
    // Highlight the selected move in the table
    for (int row = 0; row < m_moveTable->rowCount(); ++row) {
        QTableWidgetItem *targetItem = m_moveTable->item(row, 0);
        if (targetItem && targetItem->text() == move.targetTerritory) {
            m_moveTable->selectRow(row);
            // Add arrow indicator
            targetItem->setText(QString("> %1").arg(move.targetTerritory));
            break;
        }
    }
}

void AIDebugWidget::onActionTaken(const QString &timestamp, const QString &description)
{
    // Color code different types of messages
    QString color = "#d4d4d4";  // Default light gray
    if (description.contains("TURN STARTED") || description.contains("TURN COMPLETE")) {
        color = "#FFD700";  // Gold
    } else if (description.contains("Phase")) {
        color = "#87CEEB";  // Sky blue
    } else if (description.contains("Best move") || description.contains("Selected")) {
        color = "#90EE90";  // Light green
    } else if (description.contains("Combat") || description.contains("Attack")) {
        color = "#FF6B6B";  // Light red
    }

    QString html = QString("<span style='color: #888;'>%1</span> | <span style='color: %2;'>%3</span><br>")
        .arg(timestamp)
        .arg(color)
        .arg(description.toHtmlEscaped());

    m_logTextEdit->insertHtml(html);
    m_logTextEdit->ensureCursorVisible();

    // Auto-scroll to bottom
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}

void AIDebugWidget::onCombatAnalysisUpdated(const QString &territory,
                                            int myForces, int enemyForces,
                                            int myAdvantage, int enemyAdvantage,
                                            int estimatedWinPercent)
{
    m_combatTerritoryLabel->setText(QString("Territory: %1").arg(territory));
    m_myForcesLabel->setText(QString("My forces: %1 (adv: %2%3)")
        .arg(myForces)
        .arg(myAdvantage >= 0 ? "+" : "")
        .arg(myAdvantage));
    m_enemyForcesLabel->setText(QString("Enemy forces: %1 (adv: %2%3)")
        .arg(enemyForces)
        .arg(enemyAdvantage >= 0 ? "+" : "")
        .arg(enemyAdvantage));
    m_winChanceBar->setValue(estimatedWinPercent);
}

void AIDebugWidget::onPurchasePlanUpdated(int budget, const PurchaseDecision &decision)
{
    m_budgetLabel->setText(QString("Budget: %1 talents").arg(budget));

    m_purchaseListWidget->clear();
    if (decision.infantry > 0) {
        m_purchaseListWidget->addItem(QString("%1x Infantry").arg(decision.infantry));
    }
    if (decision.cavalry > 0) {
        m_purchaseListWidget->addItem(QString("%1x Cavalry").arg(decision.cavalry));
    }
    if (decision.catapults > 0) {
        m_purchaseListWidget->addItem(QString("%1x Catapults").arg(decision.catapults));
    }
    if (decision.galleys > 0) {
        m_purchaseListWidget->addItem(QString("%1x Galleys").arg(decision.galleys));
    }
    for (const QString &city : decision.cityTerritories) {
        m_purchaseListWidget->addItem(QString("City @ %1").arg(city));
    }
    for (const QString &fort : decision.fortifyTerritories) {
        m_purchaseListWidget->addItem(QString("Fortify @ %1").arg(fort));
    }

    int remaining = budget - decision.totalCost;
    m_remainingLabel->setText(QString("Remaining: %1 talents").arg(remaining));
}

void AIDebugWidget::onWaitingForStep()
{
    m_stepButton->setStyleSheet("background-color: #FFEB3B; font-weight: bold;");
}

// ============================================================================
// Slots - Control buttons
// ============================================================================

void AIDebugWidget::onStepClicked()
{
    m_stepButton->setStyleSheet("");  // Reset highlight
    if (m_aiPlayer) {
        m_aiPlayer->step();
    }
}

void AIDebugWidget::onAutoClicked()
{
    if (m_aiPlayer) {
        bool autoOn = m_autoButton->isChecked();
        m_aiPlayer->setAutoRun(autoOn);
        m_autoButton->setText(autoOn ? "Auto (ON)" : "Auto");
    }
}

void AIDebugWidget::onPauseClicked()
{
    if (m_aiPlayer) {
        m_aiPlayer->setAutoRun(false);
        m_autoButton->setChecked(false);
        m_autoButton->setText("Auto");
    }
}

void AIDebugWidget::onSpeedChanged(int value)
{
    m_speedLabel->setText(QString("%1ms").arg(value));
    if (m_aiPlayer) {
        m_aiPlayer->setDelayMs(value);
    }
}

void AIDebugWidget::onStrategyChanged(int index)
{
    if (m_aiPlayer && index >= 0) {
        int strategyValue = m_strategyCombo->itemData(index).toInt();
        m_aiPlayer->setStrategy(static_cast<AIPlayer::Strategy>(strategyValue));
    }
}

void AIDebugWidget::onClearLogClicked()
{
    m_logTextEdit->clear();
}

// ============================================================================
// Settings
// ============================================================================

void AIDebugWidget::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void AIDebugWidget::saveSettings()
{
    QSettings settings("LAU", "ConquestOfTheEmpire");
    settings.beginGroup("AIDebugWidget");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("speed", m_speedSlider->value());
    settings.setValue("strategy", m_strategyCombo->currentIndex());
    settings.endGroup();
}

void AIDebugWidget::loadSettings()
{
    QSettings settings("LAU", "ConquestOfTheEmpire");
    settings.beginGroup("AIDebugWidget");
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("speed")) {
        m_speedSlider->setValue(settings.value("speed").toInt());
    }
    if (settings.contains("strategy")) {
        m_strategyCombo->setCurrentIndex(settings.value("strategy").toInt());
    }
    settings.endGroup();
}

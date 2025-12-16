#include "moveenumeratorwidget.h"
#include "player.h"
#include "mapgraph.h"
#include "gamepiece.h"
#include "moveenumerator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>
#include <QCloseEvent>
#include <algorithm>

MoveEnumeratorWidget::MoveEnumeratorWidget(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle("Move Enumerator");
    setAttribute(Qt::WA_DeleteOnClose, false);
    setupUI();
    loadSettings();
}

MoveEnumeratorWidget::~MoveEnumeratorWidget()
{
    saveSettings();
}

void MoveEnumeratorWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Top controls
    QHBoxLayout *controlsLayout = new QHBoxLayout();

    QLabel *playerLabel = new QLabel("Player:");
    m_playerCombo = new QComboBox();
    m_playerCombo->setMinimumWidth(100);

    m_refreshButton = new QPushButton("Refresh");

    m_statsLabel = new QLabel();

    controlsLayout->addWidget(playerLabel);
    controlsLayout->addWidget(m_playerCombo);
    controlsLayout->addWidget(m_refreshButton);
    controlsLayout->addStretch();
    controlsLayout->addWidget(m_statsLabel);

    mainLayout->addLayout(controlsLayout);

    // Report text area
    m_reportTextEdit = new QTextEdit();
    m_reportTextEdit->setReadOnly(true);
    m_reportTextEdit->setFontFamily("Courier New");
    m_reportTextEdit->setLineWrapMode(QTextEdit::NoWrap);

    mainLayout->addWidget(m_reportTextEdit);

    // Connections
    connect(m_playerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MoveEnumeratorWidget::onPlayerChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &MoveEnumeratorWidget::onRefreshClicked);

    resize(800, 600);
}

void MoveEnumeratorWidget::setGameState(const QList<Player*> &players, MapGraph *graph)
{
    m_players = players;
    m_graph = graph;

    // Populate player combo
    m_playerCombo->blockSignals(true);
    m_playerCombo->clear();
    for (Player *p : players) {
        m_playerCombo->addItem(QString("Player %1").arg(p->getId()), QVariant::fromValue(p->getId()));
    }
    m_playerCombo->blockSignals(false);

    // Generate report for first player
    if (!players.isEmpty()) {
        refreshReport();
    }
}

void MoveEnumeratorWidget::refreshReport()
{
    if (m_players.isEmpty() || !m_graph) {
        m_reportTextEdit->setPlainText("No game state available.");
        return;
    }

    int index = m_playerCombo->currentIndex();
    if (index < 0 || index >= m_players.size()) {
        return;
    }

    Player *player = m_players[index];
    QString report = generateReport(player);
    m_reportTextEdit->setPlainText(report);
}

void MoveEnumeratorWidget::onPlayerChanged(int index)
{
    Q_UNUSED(index);
    refreshReport();
}

void MoveEnumeratorWidget::onRefreshClicked()
{
    refreshReport();
}

QString MoveEnumeratorWidget::generateReport(Player *player)
{
    if (!player || !m_graph) {
        return "Invalid player or graph.";
    }

    MoveEnumerator enumerator;
    TurnMoveEnumeration enumeration = enumerator.enumerateAllMoves(player, m_players, m_graph);

    QString report;
    QTextStream out(&report);

    out << "=== MOVE ENUMERATION REPORT ===\n";
    out << "Player: " << player->getId() << "\n";
    out << "\n";

    // Summary statistics
    out << "--- SUMMARY ---\n";
    out << "Generals (incl. Caesar): " << enumeration.totalGenerals() << "\n";
    out << "Total General Move Combinations: " << enumeration.totalGeneralMoveCount() << "\n";
    out << "Troops: " << enumeration.totalTroops() << "\n";
    out << "Total Troop Move Combinations: " << enumeration.totalTroopMoveCount() << "\n";
    out << "\n";

    // General moves
    out << "=== GENERAL MOVES ===\n\n";

    for (const GeneralMoveSet &gms : enumeration.generalMoveSets) {
        QString generalName;
        if (gms.general->getType() == GamePiece::Type::Caesar) {
            generalName = "Caesar";
        } else {
            GeneralPiece *gen = qobject_cast<GeneralPiece*>(gms.general);
            generalName = QString("General %1").arg(gen ? gen->getNumber() : 0);
        }

        out << "--- " << generalName << " at " << gms.startTerritory << " ---\n";
        out << "Possible moves: " << gms.count() << "\n";

        for (const GeneralMove &gm : gms.possibleMoves) {
            QString move1Str = gm.move1.isStay()
                ? QString("STAY at %1").arg(gm.move1.source)
                : QString("%1 -> %2").arg(gm.move1.source, gm.move1.sink);

            QString move2Str = gm.move2.isStay()
                ? QString("STAY at %1").arg(gm.move2.source)
                : QString("%1 -> %2").arg(gm.move2.source, gm.move2.sink);

            out << "  [" << move1Str << "] , [" << move2Str << "]\n";
        }
        out << "\n";
    }

    // Troop moves
    out << "=== TROOP MOVES ===\n\n";

    for (const TroopMoveSet &tms : enumeration.troopMoveSets) {
        QString troopName;
        GamePiece::Type type = tms.troop->getType();
        switch (type) {
            case GamePiece::Type::Infantry:
                troopName = QString("Infantry #%1").arg(tms.troop->getUniqueId());
                break;
            case GamePiece::Type::Cavalry:
                troopName = QString("Cavalry #%1").arg(tms.troop->getUniqueId());
                break;
            case GamePiece::Type::Catapult:
                troopName = QString("Catapult #%1").arg(tms.troop->getUniqueId());
                break;
            default:
                troopName = QString("Unknown #%1").arg(tms.troop->getUniqueId());
                break;
        }

        out << "--- " << troopName << " at " << tms.startTerritory << " ---\n";
        out << "Possible moves: " << tms.count() << "\n";

        if (tms.isCavalry()) {
            // Cavalry has 2 transitions
            for (const CavalryMove &cm : tms.possibleCavalryMoves) {
                QString t1Str = cm.transition1.isStay()
                    ? QString("STAY at %1").arg(cm.transition1.source)
                    : QString("%1 -> %2").arg(cm.transition1.source, cm.transition1.sink);

                QString t2Str = cm.transition2.isStay()
                    ? QString("STAY at %1").arg(cm.transition2.source)
                    : QString("%1 -> %2").arg(cm.transition2.source, cm.transition2.sink);

                out << "  [" << t1Str << "] , [" << t2Str << "]\n";
            }
        } else {
            // Infantry/Catapult has 1 transition
            for (const TroopMove &tm : tms.possibleMoves) {
                QString tStr = tm.transition.isStay()
                    ? QString("STAY at %1").arg(tm.transition.source)
                    : QString("%1 -> %2").arg(tm.transition.source, tm.transition.sink);

                out << "  [" << tStr << "]\n";
            }
        }
        out << "\n";
    }

    // Reachability heat map with breakdown
    out << "=== REACHABILITY HEAT MAP ===\n\n";
    out << "Max troops that can reach each territory (I=Infantry, C=Cavalry, T=Catapult):\n\n";

    QMap<QString, ReachabilityBreakdown> breakdown = enumeration.getReachabilityBreakdown();

    // Sort by total count descending
    QList<QPair<QString, ReachabilityBreakdown>> sorted;
    for (auto it = breakdown.constBegin(); it != breakdown.constEnd(); ++it) {
        sorted.append(qMakePair(it.key(), it.value()));
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.second.total() > b.second.total();
    });

    for (const auto &pair : sorted) {
        const ReachabilityBreakdown &rb = pair.second;
        QString breakdown_str = QString("I:%1 C:%2 T:%3")
            .arg(rb.infantry, 2)
            .arg(rb.cavalry, 2)
            .arg(rb.catapults, 2);

        QString bar;
        for (int i = 0; i < rb.total(); ++i) {
            bar += "*";
        }
        out << QString("%1: %2 = %3  %4\n")
            .arg(pair.first, -20)
            .arg(breakdown_str)
            .arg(rb.total(), 3)
            .arg(bar);
    }
    out << "\n";

    // Update stats label
    m_statsLabel->setText(QString("Generals: %1 moves | Troops: %2 moves")
        .arg(enumeration.totalGeneralMoveCount())
        .arg(enumeration.totalTroopMoveCount()));

    return report;
}

void MoveEnumeratorWidget::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void MoveEnumeratorWidget::saveSettings()
{
    QSettings settings("LAU", "ConquestOfTheEmpire");
    settings.beginGroup("MoveEnumeratorWidget");
    settings.setValue("geometry", saveGeometry());
    settings.endGroup();
}

void MoveEnumeratorWidget::loadSettings()
{
    QSettings settings("LAU", "ConquestOfTheEmpire");
    settings.beginGroup("MoveEnumeratorWidget");
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    settings.endGroup();
}

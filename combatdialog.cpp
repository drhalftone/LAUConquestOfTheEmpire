#include "combatdialog.h"
#include <QDebug>
#include <QMessageBox>
#include <QSet>
#include <cstdlib>  // for rand()

CombatDialog::CombatDialog(Player *attackingPlayer,
                           Player *defendingPlayer,
                           const QString &combatTerritoryName,
                           MapWidget *mapWidget,
                           QWidget *parent)
    : QDialog(parent)
    , m_attackingPlayer(attackingPlayer)
    , m_defendingPlayer(defendingPlayer)
    , m_combatTerritoryName(combatTerritoryName)
    , m_mapWidget(mapWidget)
    , m_selectedTarget(nullptr)
    , m_isAttackersTurn(true)
    , m_retreatButton(nullptr)
    , m_attackingHeader(nullptr)
    , m_defendingHeader(nullptr)
    , m_combatResult(CombatResult::DefenderWins)  // Default to defender wins
    , m_rollingDie(nullptr)
    , m_pendingGalleyButton(nullptr)
{
    setWindowTitle("Combat Resolution");

    // Create rolling die widget early - needed before buttons are created
    m_rollingDie = new LAURollingDieWidget(1, this);
    m_rollingDie->hide();
    connect(m_rollingDie, &LAURollingDieWidget::rollComplete, this, &CombatDialog::onRollComplete);

    // Get all pieces at combat territory
    m_attackingPieces = m_attackingPlayer->getPiecesAtTerritory(combatTerritoryName);
    m_defendingPieces = m_defendingPlayer->getPiecesAtTerritory(combatTerritoryName);

    // Check if defender has any troops (not just leaders)
    bool defenderHasTroops = false;
    for (GamePiece *piece : m_defendingPieces) {
        GamePiece::Type type = piece->getType();
        if (type == GamePiece::Type::Infantry ||
            type == GamePiece::Type::Cavalry ||
            type == GamePiece::Type::Catapult) {
            defenderHasTroops = true;
            break;
        }
    }

    // Check if attacker has any troops (not just leaders)
    bool attackerHasTroops = false;
    for (GamePiece *piece : m_attackingPieces) {
        GamePiece::Type type = piece->getType();
        if (type == GamePiece::Type::Infantry ||
            type == GamePiece::Type::Cavalry ||
            type == GamePiece::Type::Catapult) {
            attackerHasTroops = true;
            break;
        }
    }

    // If defender has no troops (only leaders), attacker wins automatically
    if (!defenderHasTroops && attackerHasTroops) {
        qDebug() << "Defender has no troops - attacker wins automatically";

        // Call checkCombatEnd immediately which will handle the victory
        // We'll do this after the dialog is set up, so create a minimal dialog first
        QVBoxLayout *tempLayout = new QVBoxLayout(this);

        // Add victory icon and message in horizontal layout
        QHBoxLayout *messageLayout = new QHBoxLayout();
        QLabel *iconLabel = new QLabel();
        QPixmap victoryPixmap(":/images/victoryIcon.png");
        iconLabel->setPixmap(victoryPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setAlignment(Qt::AlignCenter);
        messageLayout->addWidget(iconLabel);

        QLabel *messageLabel = new QLabel("Defender has no troops to defend with!\n\nAttacker wins by default!");
        messageLabel->setAlignment(Qt::AlignCenter);
        messageLabel->setStyleSheet("font-size: 14pt; padding: 20px;");
        messageLayout->addWidget(messageLabel);

        tempLayout->addLayout(messageLayout);

        QPushButton *okButton = new QPushButton("Continue");
        connect(okButton, &QPushButton::clicked, [this]() {
            // Manually trigger combat end for defender with no troops
            checkCombatEnd();
        });
        tempLayout->addWidget(okButton);

        return;  // Skip normal combat dialog setup
    }

    // If attacker has no troops (only leaders), defender wins automatically
    if (!attackerHasTroops && defenderHasTroops) {
        qDebug() << "Attacker has no troops - defender wins automatically";

        QVBoxLayout *tempLayout = new QVBoxLayout(this);

        // Add victory icon and message in horizontal layout
        QHBoxLayout *messageLayout = new QHBoxLayout();
        QLabel *iconLabel = new QLabel();
        QPixmap victoryPixmap(":/images/victoryIcon.png");
        iconLabel->setPixmap(victoryPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setAlignment(Qt::AlignCenter);
        messageLayout->addWidget(iconLabel);

        QLabel *messageLabel = new QLabel("Attacker has no troops!\n\nDefender wins by default!");
        messageLabel->setAlignment(Qt::AlignCenter);
        messageLabel->setStyleSheet("font-size: 14pt; padding: 20px;");
        messageLayout->addWidget(messageLabel);

        tempLayout->addLayout(messageLayout);

        QPushButton *okButton = new QPushButton("Continue");
        connect(okButton, &QPushButton::clicked, [this]() {
            // Manually trigger combat end for attacker with no troops
            checkCombatEnd();
        });
        tempLayout->addWidget(okButton);

        return;  // Skip normal combat dialog setup
    }

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // Combat area - split into left (attacking) and right (defending)
    QHBoxLayout *combatLayout = new QHBoxLayout();

    // Left side - Attacking
    QWidget *attackingSide = createAttackingSide();
    combatLayout->addWidget(attackingSide, 0);  // 0 = don't stretch

    // Divider
    QFrame *divider = new QFrame();
    divider->setFrameShape(QFrame::VLine);
    divider->setFrameShadow(QFrame::Sunken);
    divider->setLineWidth(2);
    combatLayout->addWidget(divider, 0);  // 0 = don't stretch

    // Right side - Defending
    QWidget *defendingSide = createDefendingSide();
    combatLayout->addWidget(defendingSide, 0);  // 0 = don't stretch

    // Add stretch to push everything to the left
    combatLayout->addStretch(1);

    mainLayout->addLayout(combatLayout);

    // Bottom buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_retreatButton = new QPushButton("Retreat");
    connect(m_retreatButton, &QPushButton::clicked, this, &CombatDialog::onRetreatClicked);
    buttonLayout->addWidget(m_retreatButton);

    mainLayout->addLayout(buttonLayout);

    // Update advantage display
    updateAdvantageDisplay();

    // Initialize button states - only defender's troops are clickable
    setAttackingButtonsEnabled(false);  // Disable attacker's troops (not used in combat)
    setDefendingButtonsEnabled(true);   // Enable defender's troops for attacker to target
}

QWidget* CombatDialog::createAttackingSide()
{
    QWidget *widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(widget);

    // Header
    m_attackingHeader = new QLabel(QString("Player %1").arg(m_attackingPlayer->getId()));
    m_attackingHeader->setStyleSheet("font-weight: bold; font-size: 12pt;");
    m_attackingHeader->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_attackingHeader);

    // Scroll area for legions
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *scrollContent = new QWidget();
    QHBoxLayout *legionsLayout = new QHBoxLayout(scrollContent);

    // First, collect all galleys and find generals/caesars on each galley
    QList<GalleyPiece*> galleys;
    QSet<int> leadersOnGalleys;  // Track which leaders are on galleys

    for (GamePiece *piece : m_attackingPieces) {
        if (piece->getType() == GamePiece::Type::Galley) {
            GalleyPiece *galley = static_cast<GalleyPiece*>(piece);
            galleys.append(galley);
        }
    }

    // Find which leaders are on galleys
    for (GamePiece *piece : m_attackingPieces) {
        if (piece->isOnGalley()) {
            leadersOnGalleys.insert(piece->getUniqueId());
        }
    }

    // Create galley widgets (with generals underneath)
    for (GalleyPiece *galley : galleys) {
        QWidget *galleyWidget = createGalleyWidget(galley, true);
        legionsLayout->addWidget(galleyWidget);
    }

    // Add leaders NOT on galleys (Caesar, General)
    for (GamePiece *piece : m_attackingPieces) {
        if (leadersOnGalleys.contains(piece->getUniqueId())) {
            continue;  // Skip leaders on galleys (already shown under galley)
        }

        if (piece->getType() == GamePiece::Type::Caesar) {
            CaesarPiece *caesar = static_cast<CaesarPiece*>(piece);
            QGroupBox *legionGroupBox = createLegionGroupBox(caesar, caesar->getLegion(), true);
            m_attackingGroupBoxes.append(legionGroupBox);
            legionsLayout->addWidget(legionGroupBox);
        } else if (piece->getType() == GamePiece::Type::General) {
            GeneralPiece *general = static_cast<GeneralPiece*>(piece);
            QGroupBox *legionGroupBox = createLegionGroupBox(general, general->getLegion(), true);
            m_attackingGroupBoxes.append(legionGroupBox);
            legionsLayout->addWidget(legionGroupBox);
        }
    }

    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea);

    return widget;
}

QWidget* CombatDialog::createDefendingSide()
{
    QWidget *widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(widget);

    // Header
    m_defendingHeader = new QLabel(QString("Player %1").arg(m_defendingPlayer->getId()));
    m_defendingHeader->setStyleSheet("font-weight: bold; font-size: 12pt;");
    m_defendingHeader->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_defendingHeader);

    // Scroll area for legions
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *scrollContent = new QWidget();
    QHBoxLayout *legionsLayout = new QHBoxLayout(scrollContent);

    // First, collect all galleys and find generals/caesars on each galley
    QList<GalleyPiece*> galleys;
    QSet<int> leadersOnGalleys;  // Track which leaders are on galleys

    for (GamePiece *piece : m_defendingPieces) {
        if (piece->getType() == GamePiece::Type::Galley) {
            GalleyPiece *galley = static_cast<GalleyPiece*>(piece);
            galleys.append(galley);
        }
    }

    // Find which leaders are on galleys
    for (GamePiece *piece : m_defendingPieces) {
        if (piece->isOnGalley()) {
            leadersOnGalleys.insert(piece->getUniqueId());
        }
    }

    // Create galley widgets (with generals underneath)
    for (GalleyPiece *galley : galleys) {
        QWidget *galleyWidget = createGalleyWidget(galley, false);
        legionsLayout->addWidget(galleyWidget);
    }

    // Find all leaders NOT on galleys (Caesar, General) and unled troops
    QList<GamePiece*> unledTroops;

    for (GamePiece *piece : m_defendingPieces) {
        if (leadersOnGalleys.contains(piece->getUniqueId())) {
            continue;  // Skip leaders on galleys (already shown under galley)
        }

        if (piece->getType() == GamePiece::Type::Caesar) {
            CaesarPiece *caesar = static_cast<CaesarPiece*>(piece);
            QGroupBox *legionGroupBox = createLegionGroupBox(caesar, caesar->getLegion(), false);
            m_defendingGroupBoxes.append(legionGroupBox);
            legionsLayout->addWidget(legionGroupBox);
        } else if (piece->getType() == GamePiece::Type::General) {
            GeneralPiece *general = static_cast<GeneralPiece*>(piece);
            QGroupBox *legionGroupBox = createLegionGroupBox(general, general->getLegion(), false);
            m_defendingGroupBoxes.append(legionGroupBox);
            legionsLayout->addWidget(legionGroupBox);
        } else if (piece->getType() == GamePiece::Type::Infantry ||
                   piece->getType() == GamePiece::Type::Cavalry ||
                   piece->getType() == GamePiece::Type::Catapult) {
            // Check if this troop is part of any legion
            bool inLegion = false;
            for (GamePiece *leader : m_defendingPieces) {
                QList<int> legion;
                if (leader->getType() == GamePiece::Type::Caesar) {
                    legion = static_cast<CaesarPiece*>(leader)->getLegion();
                } else if (leader->getType() == GamePiece::Type::General) {
                    legion = static_cast<GeneralPiece*>(leader)->getLegion();
                } else if (leader->getType() == GamePiece::Type::Galley) {
                    legion = static_cast<GalleyPiece*>(leader)->getLegion();
                }

                if (legion.contains(piece->getUniqueId())) {
                    inLegion = true;
                    break;
                }
            }

            if (!inLegion) {
                unledTroops.append(piece);
            }
        }
    }

    // Add unled troops columns (max 6 troops per column)
    if (!unledTroops.isEmpty()) {
        int troopsPerColumn = 6;
        int totalUnledTroops = unledTroops.size();

        for (int i = 0; i < totalUnledTroops; i += troopsPerColumn) {
            // Get the next batch of troops (up to 6)
            QList<GamePiece*> troopBatch;
            for (int j = i; j < i + troopsPerColumn && j < totalUnledTroops; ++j) {
                troopBatch.append(unledTroops[j]);
            }

            // Create a group box for this batch
            QGroupBox *unledGroupBox = createUnledTroopsGroupBox(troopBatch, false);
            m_defendingGroupBoxes.append(unledGroupBox);
            legionsLayout->addWidget(unledGroupBox);
        }
    }

    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea);

    return widget;
}

QGroupBox* CombatDialog::createLegionGroupBox(GamePiece *leader, const QList<int> &legionIds, bool isAttacker)
{
    // Create group box title
    QString leaderName;
    if (leader->getType() == GamePiece::Type::Caesar) {
        leaderName = QString("Caesar %1").arg(leader->getPlayer());
    } else if (leader->getType() == GamePiece::Type::General) {
        GeneralPiece *general = static_cast<GeneralPiece*>(leader);
        leaderName = QString("General %1 #%2").arg(leader->getPlayer()).arg(general->getNumber());
    } else if (leader->getType() == GamePiece::Type::Galley) {
        leaderName = QString("Galley %1").arg(leader->getPlayer());
    }

    // Create group box with leader name as title
    QGroupBox *groupBox = new QGroupBox(leaderName);
    groupBox->setFixedWidth(150);
    QVBoxLayout *layout = new QVBoxLayout(groupBox);
    layout->setSpacing(5);

    // Add last territory info (for retreat)
    Position lastTerritory = {-1, -1};
    bool hasLastTerritory = false;
    if (leader->getType() == GamePiece::Type::Caesar) {
        CaesarPiece *caesar = static_cast<CaesarPiece*>(leader);
        hasLastTerritory = caesar->hasLastTerritory();
        if (hasLastTerritory) {
            lastTerritory = caesar->getLastTerritory();
        }
    } else if (leader->getType() == GamePiece::Type::General) {
        GeneralPiece *general = static_cast<GeneralPiece*>(leader);
        hasLastTerritory = general->hasLastTerritory();
        if (hasLastTerritory) {
            lastTerritory = general->getLastTerritory();
        }
    } else if (leader->getType() == GamePiece::Type::Galley) {
        GalleyPiece *galley = static_cast<GalleyPiece*>(leader);
        hasLastTerritory = galley->hasLastTerritory();
        if (hasLastTerritory) {
            lastTerritory = galley->getLastTerritory();
        }
    }

    if (hasLastTerritory && m_mapWidget) {
        QString lastTerritoryName = m_mapWidget->getTerritoryNameAt(lastTerritory.row, lastTerritory.col);
        QLabel *retreatLabel = new QLabel(QString("%1 [%2,%3]")
                                              .arg(lastTerritoryName)
                                              .arg(lastTerritory.row)
                                              .arg(lastTerritory.col));
        retreatLabel->setStyleSheet("font-size: 9pt; color: #666; font-style: italic; padding: 2px;");
        retreatLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(retreatLabel);
    } else {
        // Show current territory for defenders (no retreat available)
        if (m_mapWidget) {
            QLabel *retreatLabel = new QLabel(m_combatTerritoryName);
            retreatLabel->setStyleSheet("font-size: 9pt; color: #666; font-style: italic; padding: 2px;");
            retreatLabel->setAlignment(Qt::AlignCenter);
            layout->addWidget(retreatLabel);
        }
    }

    // Add troops in legion
    Player *owningPlayer = (leader->getPlayer() == m_attackingPlayer->getId()) ? m_attackingPlayer : m_defendingPlayer;
    QList<GamePiece*> allPieces = owningPlayer->getPiecesAtTerritory(m_combatTerritoryName);

    for (int pieceId : legionIds) {
        for (GamePiece *piece : allPieces) {
            if (piece->getUniqueId() == pieceId) {
                QString troopType;
                QString iconPath;
                if (piece->getType() == GamePiece::Type::Infantry) {
                    troopType = "Infantry";
                    iconPath = ":/images/infantryIcon.png";
                } else if (piece->getType() == GamePiece::Type::Cavalry) {
                    troopType = "Cavalry";
                    iconPath = ":/images/cavalryIcon.png";
                } else if (piece->getType() == GamePiece::Type::Catapult) {
                    troopType = "Catapult";
                    iconPath = ":/images/catapultIcon.png";
                }

                QPushButton *troopButton = new QPushButton(QString("%1\nID: %2").arg(troopType).arg(piece->getSerialNumber()));
                troopButton->setIcon(QIcon(iconPath));
                troopButton->setIconSize(QSize(24, 24));
                troopButton->setMinimumHeight(40);
                troopButton->setMaximumWidth(140);  // Fixed width for buttons

                QColor color = getTroopColor(piece->getType());
                troopButton->setStyleSheet(QString("background-color: %1;").arg(color.name()));

                // Store button-to-piece mapping
                if (isAttacker) {
                    m_attackingTroopButtons[troopButton] = piece;
                } else {
                    m_defendingTroopButtons[troopButton] = piece;
                }
                // Connect button click to rolling die widget
                connect(troopButton, &QPushButton::clicked, m_rollingDie, &LAURollingDieWidget::startRoll);

                layout->addWidget(troopButton);
                break;
            }
        }
    }

    layout->addStretch();

    return groupBox;
}

QGroupBox* CombatDialog::createUnledTroopsGroupBox(const QList<GamePiece*> &troops, bool isAttacker)
{
    // Create group box with "Unled Troops" as title
    QGroupBox *groupBox = new QGroupBox("Unled Troops");
    groupBox->setFixedWidth(150);
    QVBoxLayout *layout = new QVBoxLayout(groupBox);
    layout->setSpacing(5);

    // Add territory info (current combat location)
    if (m_mapWidget) {
        QLabel *territoryLabel = new QLabel(m_combatTerritoryName);
        territoryLabel->setStyleSheet("font-size: 9pt; color: #666; font-style: italic; padding: 2px;");
        territoryLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(territoryLabel);
    }

    // Add each troop
    for (GamePiece *piece : troops) {
        QString troopType;
        QString iconPath;
        if (piece->getType() == GamePiece::Type::Infantry) {
            troopType = "Infantry";
            iconPath = ":/images/infantryIcon.png";
        } else if (piece->getType() == GamePiece::Type::Cavalry) {
            troopType = "Cavalry";
            iconPath = ":/images/cavalryIcon.png";
        } else if (piece->getType() == GamePiece::Type::Catapult) {
            troopType = "Catapult";
            iconPath = ":/images/catapultIcon.png";
        }

        QPushButton *troopButton = new QPushButton(QString("%1\nID: %2").arg(troopType).arg(piece->getSerialNumber()));
        troopButton->setIcon(QIcon(iconPath));
        troopButton->setIconSize(QSize(24, 24));
        troopButton->setMinimumHeight(40);
        troopButton->setMaximumWidth(140);  // Fixed width for buttons

        QColor color = getTroopColor(piece->getType());
        troopButton->setStyleSheet(QString("background-color: %1;").arg(color.name()));

        // Store button-to-piece mapping
        if (isAttacker) {
            m_attackingTroopButtons[troopButton] = piece;
        } else {
            m_defendingTroopButtons[troopButton] = piece;
        }
        // Connect button click to rolling die widget
        connect(troopButton, &QPushButton::clicked, m_rollingDie, &LAURollingDieWidget::startRoll);

        layout->addWidget(troopButton);
    }

    layout->addStretch();

    return groupBox;
}

QWidget* CombatDialog::createGalleyWidget(GalleyPiece *galley, bool isAttacker)
{
    QWidget *widget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(widget);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(2, 2, 2, 2);

    // Create galley button at top
    QPushButton *galleyButton = new QPushButton(QString("Galley\nID: %1").arg(galley->getSerialNumber()));
    galleyButton->setIcon(QIcon(":/images/galleyIcon.png"));
    galleyButton->setIconSize(QSize(32, 32));
    galleyButton->setMinimumHeight(50);
    galleyButton->setStyleSheet("background-color: #4682B4; color: white; font-weight: bold;");  // Steel blue

    // Track the galley button - galleys need validation before rolling (check for passengers)
    if (isAttacker) {
        m_attackingGalleyButtons[galleyButton] = galley;
        connect(galleyButton, &QPushButton::clicked, this, &CombatDialog::onAttackingGalleyClicked);
    } else {
        m_defendingGalleyButtons[galleyButton] = galley;
        connect(galleyButton, &QPushButton::clicked, this, &CombatDialog::onDefendingGalleyClicked);
    }

    mainLayout->addWidget(galleyButton);

    // Find all leaders (Caesar/General) on this galley
    QList<GamePiece*> &pieces = isAttacker ? m_attackingPieces : m_defendingPieces;
    QList<GamePiece*> leadersOnThisGalley;

    for (GamePiece *piece : pieces) {
        if (piece->isOnGalley() && piece->getOnGalley() == galley->getSerialNumber()) {
            if (piece->getType() == GamePiece::Type::Caesar ||
                piece->getType() == GamePiece::Type::General) {
                leadersOnThisGalley.append(piece);
            }
        }
    }

    // Create horizontal layout for generals underneath the galley
    QHBoxLayout *generalsLayout = new QHBoxLayout();
    generalsLayout->setSpacing(2);

    if (leadersOnThisGalley.isEmpty()) {
        // No passengers - create empty placeholder
        QGroupBox *emptyBox = createEmptyGeneralGroupBox();
        generalsLayout->addWidget(emptyBox);
    } else {
        // Add each leader with their legion
        for (GamePiece *leader : leadersOnThisGalley) {
            QList<int> legionIds;
            if (leader->getType() == GamePiece::Type::Caesar) {
                legionIds = static_cast<CaesarPiece*>(leader)->getLegion();
            } else if (leader->getType() == GamePiece::Type::General) {
                legionIds = static_cast<GeneralPiece*>(leader)->getLegion();
            }

            QGroupBox *legionGroupBox = createLegionGroupBox(leader, legionIds, isAttacker);
            if (isAttacker) {
                m_attackingGroupBoxes.append(legionGroupBox);
            } else {
                m_defendingGroupBoxes.append(legionGroupBox);
            }
            generalsLayout->addWidget(legionGroupBox);
        }
    }

    mainLayout->addLayout(generalsLayout);

    // Update galley button enabled state based on whether it has TROOPS
    // Generals can "swim" - they don't protect the galley from being targeted
    // Galley is targetable when it has no troops, even if general is still aboard
    bool hasTroops = false;

    // Check if there are any troops on this galley
    for (GamePiece *piece : pieces) {
        if (piece->isOnGalley() && piece->getOnGalley() == galley->getSerialNumber()) {
            if (piece->getType() == GamePiece::Type::Infantry ||
                piece->getType() == GamePiece::Type::Cavalry ||
                piece->getType() == GamePiece::Type::Catapult) {
                hasTroops = true;
                break;
            }
        }
    }

    // Store whether galley is targetable (will be used during combat)
    // "hasPassengers" here really means "has troops" - generals don't count
    galleyButton->setProperty("hasPassengers", hasTroops);

    return widget;
}

QGroupBox* CombatDialog::createEmptyGeneralGroupBox()
{
    QGroupBox *groupBox = new QGroupBox("(Empty)");
    groupBox->setFixedWidth(150);
    QVBoxLayout *layout = new QVBoxLayout(groupBox);

    QLabel *emptyLabel = new QLabel("No passengers");
    emptyLabel->setStyleSheet("font-size: 9pt; color: #888; font-style: italic;");
    emptyLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(emptyLabel);

    layout->addStretch();

    return groupBox;
}

void CombatDialog::setAttackingButtonsEnabled(bool enabled)
{
    for (QGroupBox *groupBox : m_attackingGroupBoxes) {
        groupBox->setEnabled(enabled);
    }

    // Enable/disable galley buttons - only enable if they have no passengers
    for (auto it = m_attackingGalleyButtons.begin(); it != m_attackingGalleyButtons.end(); ++it) {
        QPushButton *button = it.key();
        bool hasPassengers = button->property("hasPassengers").toBool();
        // Galley button is only enabled if the side is enabled AND it has no passengers
        button->setEnabled(enabled && !hasPassengers);
    }
}

void CombatDialog::setDefendingButtonsEnabled(bool enabled)
{
    for (QGroupBox *groupBox : m_defendingGroupBoxes) {
        groupBox->setEnabled(enabled);
    }

    // Enable/disable galley buttons - only enable if they have no passengers
    for (auto it = m_defendingGalleyButtons.begin(); it != m_defendingGalleyButtons.end(); ++it) {
        QPushButton *button = it.key();
        bool hasPassengers = button->property("hasPassengers").toBool();
        // Galley button is only enabled if the side is enabled AND it has no passengers
        button->setEnabled(enabled && !hasPassengers);
    }
}

void CombatDialog::updateGalleyPassengerStatus(const QString &galleySerialNumber, bool isAttacker)
{
    if (galleySerialNumber.isEmpty()) return;

    // Find the galley button with this serial number
    QMap<QPushButton*, GalleyPiece*> &galleyButtons = isAttacker ? m_attackingGalleyButtons : m_defendingGalleyButtons;

    for (auto it = galleyButtons.begin(); it != galleyButtons.end(); ++it) {
        GalleyPiece *galley = it.value();
        if (galley && galley->getSerialNumber() == galleySerialNumber) {
            QPushButton *button = it.key();

            // Check if there are any remaining TROOPS on this galley
            // Generals can "swim" - they don't protect the galley
            // Galley is targetable when it has no troops, even if general is still aboard
            bool hasTroops = false;
            QMap<QPushButton*, GamePiece*> &troopButtons = isAttacker ? m_attackingTroopButtons : m_defendingTroopButtons;
            for (auto troopIt = troopButtons.begin(); troopIt != troopButtons.end(); ++troopIt) {
                GamePiece *piece = troopIt.value();
                if (piece && piece->isOnGalley() && piece->getOnGalley() == galleySerialNumber) {
                    hasTroops = true;
                    break;
                }
            }

            // Update the button property - galley is targetable when no troops aboard
            // (generals can swim, they're dealt with at end of combat)
            button->setProperty("hasPassengers", hasTroops);

            qDebug() << "Updated galley" << galleySerialNumber << "hasTroops:" << hasTroops;
            break;
        }
    }
}
void CombatDialog::onAttackingGalleyClicked()
{
    QPushButton *clickedButton = qobject_cast<QPushButton*>(sender());
    if (!clickedButton) return;

    GalleyPiece *galley = m_attackingGalleyButtons[clickedButton];
    if (!galley) return;

    // Check if galley still has troops - if so, can't target it
    bool hasTroops = clickedButton->property("hasPassengers").toBool();
    if (hasTroops) {
        QMessageBox::warning(this, "Cannot Target Galley",
            "This galley still has troops aboard!\n\n"
            "You must destroy all troops on the galley before you can sink it.\n"
            "(Generals can swim - they don't protect the galley)");
        return;
    }

    qDebug() << "Defender attacking attacking galley ID" << galley->getSerialNumber();

    // Disable all buttons during combat resolution
    setDefendingButtonsEnabled(false);
    setAttackingButtonsEnabled(false);

    // Store button for onRollComplete and start the die roll
    m_pendingGalleyButton = clickedButton;
    m_rollingDie->startRoll();
}

void CombatDialog::onDefendingGalleyClicked()
{
    QPushButton *clickedButton = qobject_cast<QPushButton*>(sender());
    if (!clickedButton) return;

    GalleyPiece *galley = m_defendingGalleyButtons[clickedButton];
    if (!galley) return;

    // Check if galley still has troops - if so, can't target it
    bool hasTroops = clickedButton->property("hasPassengers").toBool();
    if (hasTroops) {
        QMessageBox::warning(this, "Cannot Target Galley",
            "This galley still has troops aboard!\n\n"
            "You must destroy all troops on the galley before you can sink it.\n"
            "(Generals can swim - they don't protect the galley)");
        return;
    }

    qDebug() << "Attacker attacking defending galley ID" << galley->getSerialNumber();

    // Disable all buttons during combat resolution
    setDefendingButtonsEnabled(false);
    setAttackingButtonsEnabled(false);

    // Store button for onRollComplete and start the die roll
    m_pendingGalleyButton = clickedButton;
    m_rollingDie->startRoll();
}

void CombatDialog::onRollComplete(int dieValue, QObject *senderObj)
{
    QPushButton *clickedButton = qobject_cast<QPushButton*>(senderObj);

    // If no sender from die widget, check if we have a pending galley button
    if (!clickedButton && m_pendingGalleyButton) {
        clickedButton = m_pendingGalleyButton;
        m_pendingGalleyButton = nullptr;
    }

    if (!clickedButton) {
        qDebug() << "onRollComplete: No button found!";
        return;
    }

    qDebug() << "Roll complete! Die value:" << dieValue;

    // Disable buttons during processing
    setDefendingButtonsEnabled(false);
    setAttackingButtonsEnabled(false);

    // Determine what type of target was clicked
    bool isDefendingTroop = m_defendingTroopButtons.contains(clickedButton);
    bool isAttackingTroop = m_attackingTroopButtons.contains(clickedButton);
    bool isDefendingGalley = m_defendingGalleyButtons.contains(clickedButton);
    bool isAttackingGalley = m_attackingGalleyButtons.contains(clickedButton);

    if (isDefendingTroop) {
        // Attacker attacked a defending troop
        GamePiece *defendingPiece = m_defendingTroopButtons[clickedButton];
        if (!defendingPiece) return;

        // Calculate attacker's advantage and resolve
        int advantage = getNetAdvantage(true);
        bool isHit = resolveAttack(defendingPiece->getType(), advantage, dieValue);

        QString resultMessage;
        if (isHit) {
            resultMessage = QString("HIT! Roll: %1 + Advantage: %2 = %3\n\nDefending troop (ID: %4) has been destroyed.")
                                .arg(dieValue).arg(advantage).arg(dieValue + advantage).arg(defendingPiece->getSerialNumber());
        } else {
            resultMessage = QString("MISS! Roll: %1 + Advantage: %2 = %3\n\nDefending troop (ID: %4) survived.")
                                .arg(dieValue).arg(advantage).arg(dieValue + advantage).arg(defendingPiece->getSerialNumber());
        }
        QMessageBox::information(this, "Combat Result", resultMessage);

        if (isHit) {
            QString galleySerial = defendingPiece->isOnGalley() ? defendingPiece->getOnGalley() : QString();
            removeTroopButton(clickedButton);
            if (defendingPiece->getType() == GamePiece::Type::Infantry) {
                m_defendingPlayer->removeInfantry(static_cast<InfantryPiece*>(defendingPiece));
            } else if (defendingPiece->getType() == GamePiece::Type::Cavalry) {
                m_defendingPlayer->removeCavalry(static_cast<CavalryPiece*>(defendingPiece));
            } else if (defendingPiece->getType() == GamePiece::Type::Catapult) {
                m_defendingPlayer->removeCatapult(static_cast<CatapultPiece*>(defendingPiece));
            }
            defendingPiece->deleteLater();
            if (!galleySerial.isEmpty()) {
                updateGalleyPassengerStatus(galleySerial, false);
            }
            updateAdvantageDisplay();
            if (checkCombatEnd()) return;
        }
        // Switch to defender's turn
        setAttackingButtonsEnabled(true);
        setDefendingButtonsEnabled(false);

    } else if (isAttackingTroop) {
        // Defender attacked an attacking troop
        GamePiece *attackingPiece = m_attackingTroopButtons[clickedButton];
        if (!attackingPiece) return;

        // Calculate defender's advantage and resolve
        int advantage = getNetAdvantage(false);
        bool isHit = resolveAttack(attackingPiece->getType(), advantage, dieValue);

        QString resultMessage;
        if (isHit) {
            resultMessage = QString("HIT! Roll: %1 + Advantage: %2 = %3\n\nAttacking troop (ID: %4) has been destroyed.")
                                .arg(dieValue).arg(advantage).arg(dieValue + advantage).arg(attackingPiece->getSerialNumber());
        } else {
            resultMessage = QString("MISS! Roll: %1 + Advantage: %2 = %3\n\nAttacking troop (ID: %4) survived.")
                                .arg(dieValue).arg(advantage).arg(dieValue + advantage).arg(attackingPiece->getSerialNumber());
        }
        QMessageBox::information(this, "Combat Result", resultMessage);

        if (isHit) {
            QString galleySerial = attackingPiece->isOnGalley() ? attackingPiece->getOnGalley() : QString();
            removeTroopButton(clickedButton);
            if (attackingPiece->getType() == GamePiece::Type::Infantry) {
                m_attackingPlayer->removeInfantry(static_cast<InfantryPiece*>(attackingPiece));
            } else if (attackingPiece->getType() == GamePiece::Type::Cavalry) {
                m_attackingPlayer->removeCavalry(static_cast<CavalryPiece*>(attackingPiece));
            } else if (attackingPiece->getType() == GamePiece::Type::Catapult) {
                m_attackingPlayer->removeCatapult(static_cast<CatapultPiece*>(attackingPiece));
            }
            attackingPiece->deleteLater();
            if (!galleySerial.isEmpty()) {
                updateGalleyPassengerStatus(galleySerial, true);
            }
            updateAdvantageDisplay();
            if (checkCombatEnd()) return;
        }
        // Switch back to attacker's turn
        setDefendingButtonsEnabled(true);
        setAttackingButtonsEnabled(false);

    } else if (isDefendingGalley) {
        // Attacker attacked a defending galley
        GalleyPiece *galley = m_defendingGalleyButtons[clickedButton];
        if (!galley) return;

        // Galley combat: need 4+ to hit (no advantage modifier)
        bool isHit = (dieValue >= 4);

        QString resultMessage;
        if (isHit) {
            resultMessage = QString("SUNK! Roll: %1 (needed 4+)\n\nDefending galley (ID: %2) has been destroyed.")
                                .arg(dieValue).arg(galley->getSerialNumber());
        } else {
            resultMessage = QString("MISS! Roll: %1 (needed 4+)\n\nDefending galley (ID: %2) survived.")
                                .arg(dieValue).arg(galley->getSerialNumber());
        }
        QMessageBox::information(this, "Naval Combat Result", resultMessage);

        if (isHit) {
            clickedButton->hide();
            clickedButton->deleteLater();
            m_defendingGalleyButtons.remove(clickedButton);
            m_defendingPlayer->removeGalley(galley);
            galley->deleteLater();
            if (checkCombatEnd()) return;
        }
        // Switch to defender's turn
        setAttackingButtonsEnabled(true);
        setDefendingButtonsEnabled(false);

    } else if (isAttackingGalley) {
        // Defender attacked an attacking galley
        GalleyPiece *galley = m_attackingGalleyButtons[clickedButton];
        if (!galley) return;

        // Galley combat: need 4+ to hit (no advantage modifier)
        bool isHit = (dieValue >= 4);

        QString resultMessage;
        if (isHit) {
            resultMessage = QString("SUNK! Roll: %1 (needed 4+)\n\nAttacking galley (ID: %2) has been destroyed.")
                                .arg(dieValue).arg(galley->getSerialNumber());
        } else {
            resultMessage = QString("MISS! Roll: %1 (needed 4+)\n\nAttacking galley (ID: %2) survived.")
                                .arg(dieValue).arg(galley->getSerialNumber());
        }
        QMessageBox::information(this, "Naval Combat Result", resultMessage);

        if (isHit) {
            clickedButton->hide();
            clickedButton->deleteLater();
            m_attackingGalleyButtons.remove(clickedButton);
            m_attackingPlayer->removeGalley(galley);
            galley->deleteLater();
            if (checkCombatEnd()) return;
        }
        // Switch back to attacker's turn
        setDefendingButtonsEnabled(true);
        setAttackingButtonsEnabled(false);
    }
}

void CombatDialog::onRetreatClicked()
{
    // Move all attacking leaders (and their troops) back to their last territory
    // Use fresh lists from player to avoid dangling pointers

    qDebug() << "=== RETREAT CLICKED ===";
    qDebug() << "Combat territory:" << m_combatTerritoryName;

    // Get all attacking pieces at combat territory for troop lookup
    QList<GamePiece*> allAttackingPieces = m_attackingPlayer->getPiecesAtTerritory(m_combatTerritoryName);
    qDebug() << "Found" << allAttackingPieces.size() << "attacking pieces at combat territory";

    // Process generals
    for (GeneralPiece *general : m_attackingPlayer->getGenerals()) {
        qDebug() << "Checking general #" << general->getNumber() << "at territory:" << general->getTerritoryName() << "hasLastTerritory:" << general->hasLastTerritory();
        if (general && general->getTerritoryName() == m_combatTerritoryName && general->hasLastTerritory()) {
            Position retreatPosition = general->getLastTerritory();
            QString retreatTerritoryName = m_mapWidget->getTerritoryNameAt(retreatPosition.row, retreatPosition.col);

            qDebug() << "Retreating general to" << retreatTerritoryName << "at" << retreatPosition.row << retreatPosition.col;
            general->setPosition(retreatPosition);
            general->setTerritoryName(retreatTerritoryName);

            // Move all troops in this general's legion
            QList<int> legion = general->getLegion();
            qDebug() << "  General's legion has" << legion.size() << "troops:" << legion;
            for (GamePiece *piece : allAttackingPieces) {
                if (piece && legion.contains(piece->getUniqueId())) {
                    qDebug() << "    Retreating troop ID:" << piece->getUniqueId();
                    piece->setPosition(retreatPosition);
                    piece->setTerritoryName(retreatTerritoryName);
                }
            }
        }
    }

    // Process caesars
    for (CaesarPiece *caesar : m_attackingPlayer->getCaesars()) {
        if (caesar && caesar->getTerritoryName() == m_combatTerritoryName && caesar->hasLastTerritory()) {
            Position retreatPosition = caesar->getLastTerritory();
            QString retreatTerritoryName = m_mapWidget->getTerritoryNameAt(retreatPosition.row, retreatPosition.col);

            qDebug() << "Retreating caesar to" << retreatTerritoryName << "at" << retreatPosition.row << retreatPosition.col;
            caesar->setPosition(retreatPosition);
            caesar->setTerritoryName(retreatTerritoryName);

            // Move all troops in this caesar's legion
            QList<int> legion = caesar->getLegion();
            qDebug() << "  Caesar's legion has" << legion.size() << "troops:" << legion;
            for (GamePiece *piece : allAttackingPieces) {
                if (piece && legion.contains(piece->getUniqueId())) {
                    qDebug() << "    Retreating troop ID:" << piece->getUniqueId();
                    piece->setPosition(retreatPosition);
                    piece->setTerritoryName(retreatTerritoryName);
                }
            }
        }
    }

    // Process galleys
    for (GalleyPiece *galley : m_attackingPlayer->getGalleys()) {
        if (galley && galley->getTerritoryName() == m_combatTerritoryName && galley->hasLastTerritory()) {
            Position retreatPosition = galley->getLastTerritory();
            QString retreatTerritoryName = m_mapWidget->getTerritoryNameAt(retreatPosition.row, retreatPosition.col);

            qDebug() << "Retreating galley to" << retreatTerritoryName << "at" << retreatPosition.row << retreatPosition.col;
            galley->setPosition(retreatPosition);
            galley->setTerritoryName(retreatTerritoryName);

            // Move all troops in this galley's legion
            QList<int> legion = galley->getLegion();
            qDebug() << "  Galley's legion has" << legion.size() << "troops:" << legion;
            for (GamePiece *piece : allAttackingPieces) {
                if (piece && legion.contains(piece->getUniqueId())) {
                    qDebug() << "    Retreating troop ID:" << piece->getUniqueId();
                    piece->setPosition(retreatPosition);
                    piece->setTerritoryName(retreatTerritoryName);
                }
            }
        }
    }

    // Unclaim the combat territory from the attacker (they retreated and left)
    if (m_attackingPlayer->ownsTerritory(m_combatTerritoryName)) {
        qDebug() << "Unclaiming territory" << m_combatTerritoryName << "from retreating attacker" << m_attackingPlayer->getId();
        m_attackingPlayer->unclaimTerritory(m_combatTerritoryName);
    }

    QMessageBox retreatMsg(this);
    retreatMsg.setWindowTitle("Retreat");
    retreatMsg.setText("Attacker has retreated! Surviving troops have returned to their previous territory.");
    retreatMsg.setIconPixmap(QPixmap(":/images/retreatIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    retreatMsg.setStandardButtons(QMessageBox::Ok);
    retreatMsg.exec();

    m_combatResult = CombatResult::AttackerRetreats;
    accept();
}

int CombatDialog::calculateAttackerAdvantage() const
{
    int advantage = 0;

    // Count catapults in attacking troops
    for (GamePiece *piece : m_attackingTroopButtons.values()) {
        if (piece && piece->getType() == GamePiece::Type::Catapult) {
            advantage++;
        }
    }

    return advantage;
}

int CombatDialog::calculateDefenderAdvantage() const
{
    int advantage = 0;

    // Count catapults in defending troops
    for (GamePiece *piece : m_defendingTroopButtons.values()) {
        if (piece && piece->getType() == GamePiece::Type::Catapult) {
            advantage++;
        }
    }

    // Check for walled city (fortified city) in defending territory
    if (m_mapWidget) {
        City *city = m_defendingPlayer->getCityAtTerritory(m_combatTerritoryName);
        qDebug() << "Checking for city at territory" << m_combatTerritoryName;
        qDebug() << "City found:" << (city != nullptr);
        if (city) {
            qDebug() << "City is fortified:" << city->isFortified();
            if (city->isFortified()) {
                advantage++;
                qDebug() << "Adding +1 advantage for walled city";
            }
        }
    }

    return advantage;
}

int CombatDialog::getNetAdvantage(bool forAttacker) const
{
    int attackerAdv = calculateAttackerAdvantage();
    int defenderAdv = calculateDefenderAdvantage();
    int difference = attackerAdv - defenderAdv;

    if (forAttacker) {
        return (difference > 0) ? difference : 0;  // Only positive for attacker
    } else {
        return (difference < 0) ? -difference : 0;  // Only positive for defender
    }
}

void CombatDialog::updateAdvantageDisplay()
{
    int attackerNetAdv = getNetAdvantage(true);
    int defenderNetAdv = getNetAdvantage(false);

    qDebug() << "updateAdvantageDisplay - Attacker net advantage:" << attackerNetAdv;
    qDebug() << "updateAdvantageDisplay - Defender net advantage:" << defenderNetAdv;

    // Update attacking header
    if (m_attackingHeader) {
        QString headerText = QString("Player %1").arg(m_attackingPlayer->getId());
        if (attackerNetAdv > 0) {
            headerText += QString(" (+%1)").arg(attackerNetAdv);
        }
        m_attackingHeader->setText(headerText);
    }

    // Update defending header
    if (m_defendingHeader) {
        QString headerText = QString("Player %1").arg(m_defendingPlayer->getId());
        if (defenderNetAdv > 0) {
            headerText += QString(" (+%1)").arg(defenderNetAdv);
        }
        m_defendingHeader->setText(headerText);
    }
}

bool CombatDialog::resolveAttack(GamePiece::Type targetType, int attackerAdvantage, int dieRoll)
{
    // Use provided die roll instead of generating one
    int modifiedRoll = dieRoll + attackerAdvantage;

    qDebug() << "Roll:" << dieRoll << "+ Advantage:" << attackerAdvantage << "= Total:" << modifiedRoll;

    // Determine hit threshold based on target type
    int hitThreshold;
    if (targetType == GamePiece::Type::Infantry) {
        hitThreshold = 4;
    } else if (targetType == GamePiece::Type::Cavalry) {
        hitThreshold = 5;
    } else if (targetType == GamePiece::Type::Catapult) {
        hitThreshold = 6;
    } else {
        return false;  // Leaders can't be targeted
    }

    bool isHit = (modifiedRoll >= hitThreshold);
    qDebug() << "Target type:" << (int)targetType << "Threshold:" << hitThreshold << "Hit:" << isHit;

    return isHit;
}

void CombatDialog::removeTroopButton(QPushButton *button)
{
    if (!button) return;

    // Remove from tracking maps
    m_attackingTroopButtons.remove(button);
    m_defendingTroopButtons.remove(button);

    // Hide and delete the button
    button->setVisible(false);
    button->deleteLater();
}

bool CombatDialog::checkCombatEnd()
{
    // Convert territory name to position for piece operations (temporary until GamePiece uses territory names)
    Position combatPosition = m_mapWidget->territoryNameToPosition(m_combatTerritoryName);

    qDebug() << "checkCombatEnd called";
    bool attackerHasTroops = !m_attackingTroopButtons.isEmpty();
    bool defenderHasTroops = !m_defendingTroopButtons.isEmpty();

    // Also check for galleys - generals on galleys are protected until the galley is sunk
    bool attackerHasGalleys = !m_attackingGalleyButtons.isEmpty();
    bool defenderHasGalleys = !m_defendingGalleyButtons.isEmpty();

    qDebug() << "Attacker has troops:" << attackerHasTroops << "galleys:" << attackerHasGalleys;
    qDebug() << "Defender has troops:" << defenderHasTroops << "galleys:" << defenderHasGalleys;

    // Defender is only defeated when they have no troops AND no galleys
    // (Generals on galleys are protected by the galley)
    if (!defenderHasTroops && !defenderHasGalleys) {
        qDebug() << "Defender defeated - processing victory";

        // Remove all defeated defending troops first
        qDebug() << "Attacker wins - removing all defeated defending troops";

        // Remove all defeated troops (they were already eliminated during combat)
        QList<GamePiece*> defeatedTroops;

        // Get infantry at combat position
        for (InfantryPiece *infantry : m_defendingPlayer->getInfantry()) {
            if (infantry && infantry->getTerritoryName() == m_combatTerritoryName) {
                defeatedTroops.append(infantry);
            }
        }

        // Get cavalry at combat position
        for (CavalryPiece *cavalry : m_defendingPlayer->getCavalry()) {
            if (cavalry && cavalry->getTerritoryName() == m_combatTerritoryName) {
                defeatedTroops.append(cavalry);
            }
        }

        // Get catapults at combat position
        for (CatapultPiece *catapult : m_defendingPlayer->getCatapults()) {
            if (catapult && catapult->getTerritoryName() == m_combatTerritoryName) {
                defeatedTroops.append(catapult);
            }
        }

        // Remove and delete all defeated troops
        for (GamePiece *troop : defeatedTroops) {
            GamePiece::Type type = troop->getType();
            if (type == GamePiece::Type::Infantry) {
                m_defendingPlayer->removeInfantry(static_cast<InfantryPiece*>(troop));
            } else if (type == GamePiece::Type::Cavalry) {
                m_defendingPlayer->removeCavalry(static_cast<CavalryPiece*>(troop));
            } else if (type == GamePiece::Type::Catapult) {
                m_defendingPlayer->removeCatapult(static_cast<CatapultPiece*>(troop));
            }
            troop->deleteLater();
        }
        qDebug() << "Removed" << defeatedTroops.size() << "defeated troops";

        // Check for defeated Caesar first - this is a complete takeover!
        QList<CaesarPiece*> defeatedCaesars;
        const QList<CaesarPiece*> &defendingCaesars = m_defendingPlayer->getCaesars();
        for (CaesarPiece *caesar : defendingCaesars) {
            if (caesar && caesar->getTerritoryName() == m_combatTerritoryName) {
                defeatedCaesars.append(caesar);
            }
        }

        // If Caesar was defeated, complete takeover occurs
        if (!defeatedCaesars.isEmpty()) {
            qDebug() << "Caesar captured! Complete takeover initiated.";

            QMessageBox::information(this, "Caesar Captured!",
                QString("Player %1's Caesar has been captured by Player %2!\n\n"
                        "Player %2 takes over ALL of Player %1's:\n"
                        "• Territories\n"
                        "• Cities\n"
                        "• Pieces (except Caesar - killed)\n"
                        "• Money + 100 talent bonus")
                .arg(m_defendingPlayer->getId())
                .arg(m_attackingPlayer->getId()));

            // Transfer all money + 100 bonus
            int capturedMoney = m_defendingPlayer->getWallet();
            m_defendingPlayer->spendMoney(capturedMoney);  // Take all money
            m_attackingPlayer->addMoney(capturedMoney + 100);  // Add money + bonus

            // Transfer all territories
            QList<QString> territories = m_defendingPlayer->getOwnedTerritories();
            for (const QString &territory : territories) {
                m_defendingPlayer->unclaimTerritory(territory);
                m_attackingPlayer->claimTerritory(territory);
            }

            // Transfer all cities
            QList<City*> cities = m_defendingPlayer->getCities();
            for (City *city : cities) {
                m_defendingPlayer->removeCity(city);
                city->setOwner(m_attackingPlayer->getId());
                m_attackingPlayer->addCity(city);
            }

            // Transfer all generals (they become active generals of the winner)
            QList<GeneralPiece*> generals = m_defendingPlayer->getGenerals();
            for (GeneralPiece *general : generals) {
                m_defendingPlayer->removeGeneral(general);
                general->setPlayer(m_attackingPlayer->getId());
                general->setParent(m_attackingPlayer);
                m_attackingPlayer->addGeneral(general);
            }

            // Transfer captured generals
            QList<GeneralPiece*> capturedGenerals = m_defendingPlayer->getCapturedGenerals();
            for (GeneralPiece *general : capturedGenerals) {
                m_defendingPlayer->removeCapturedGeneral(general);

                // Check if this general originally belonged to the attacker (winner)
                if (general->getPlayer() == m_attackingPlayer->getId()) {
                    // Return to original owner as active general
                    general->clearCaptured();
                    general->setParent(m_attackingPlayer);
                    m_attackingPlayer->addGeneral(general);
                } else {
                    // Transfer as captured general to winner
                    general->setCapturedBy(m_attackingPlayer->getId());
                    general->setParent(m_attackingPlayer);
                    m_attackingPlayer->addCapturedGeneral(general);
                }
            }

            // Transfer all troops
            // Infantry
            QList<InfantryPiece*> infantry = m_defendingPlayer->getInfantry();
            for (InfantryPiece *inf : infantry) {
                m_defendingPlayer->removeInfantry(inf);
                inf->setPlayer(m_attackingPlayer->getId());
                inf->setParent(m_attackingPlayer);
                m_attackingPlayer->addInfantry(inf);
            }

            // Cavalry
            QList<CavalryPiece*> cavalry = m_defendingPlayer->getCavalry();
            for (CavalryPiece *cav : cavalry) {
                m_defendingPlayer->removeCavalry(cav);
                cav->setPlayer(m_attackingPlayer->getId());
                cav->setParent(m_attackingPlayer);
                m_attackingPlayer->addCavalry(cav);
            }

            // Catapults
            QList<CatapultPiece*> catapults = m_defendingPlayer->getCatapults();
            for (CatapultPiece *cat : catapults) {
                m_defendingPlayer->removeCatapult(cat);
                cat->setPlayer(m_attackingPlayer->getId());
                cat->setParent(m_attackingPlayer);
                m_attackingPlayer->addCatapult(cat);
            }

            // Galleys
            QList<GalleyPiece*> galleys = m_defendingPlayer->getGalleys();
            for (GalleyPiece *galley : galleys) {
                m_defendingPlayer->removeGalley(galley);
                galley->setPlayer(m_attackingPlayer->getId());
                galley->setParent(m_attackingPlayer);
                m_attackingPlayer->addGalley(galley);
            }

            // Kill all defeated Caesars
            for (CaesarPiece *caesar : defeatedCaesars) {
                m_defendingPlayer->removeCaesar(caesar);
                caesar->deleteLater();
            }

            QMessageBox eliminationMsg(this);
            eliminationMsg.setWindowTitle("Complete Takeover");
            eliminationMsg.setText(QString("Player %1 has been eliminated!\n\n"
                        "Player %2 gained:\n"
                        "• %3 territories\n"
                        "• %4 cities\n"
                        "• %5 generals\n"
                        "• %6 troops\n"
                        "• %7 talents")
                .arg(m_defendingPlayer->getId())
                .arg(m_attackingPlayer->getId())
                .arg(territories.size())
                .arg(cities.size())
                .arg(generals.size())
                .arg(infantry.size() + cavalry.size() + catapults.size() + galleys.size())
                .arg(capturedMoney + 100));
            eliminationMsg.setIconPixmap(QPixmap(":/images/deadIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            eliminationMsg.setStandardButtons(QMessageBox::Ok);
            eliminationMsg.exec();

            accept();
            return true;
        }

        // Handle defeated generals (only if no Caesar was captured)
        QList<GeneralPiece*> defeatedGenerals;

        // Get fresh list of generals from the defending player (still at this position)
        qDebug() << "Collecting defeated generals";
        const QList<GeneralPiece*> &defendingGenerals = m_defendingPlayer->getGenerals();
        for (GeneralPiece *general : defendingGenerals) {
            if (general && general->getTerritoryName() == m_combatTerritoryName) {
                defeatedGenerals.append(general);
            }
        }

        // Process each defeated general
        for (GeneralPiece *general : defeatedGenerals) {
            qDebug() << "Processing defeated general" << general->getPlayer() << "#" << general->getNumber();

            QMessageBox msgBox(this);
            msgBox.setWindowTitle("Capture or Kill General?");
            msgBox.setText(QString("Defender's General %1 #%2 has been defeated.\n\nDo you want to capture this general?")
                .arg(general->getPlayer())
                .arg(general->getNumber()));
            QPushButton *captureButton = msgBox.addButton("Capture", QMessageBox::YesRole);
            QPushButton *killButton = msgBox.addButton("Kill", QMessageBox::NoRole);
            msgBox.exec();

            QMessageBox::StandardButton choice = (msgBox.clickedButton() == captureButton)
                ? QMessageBox::Yes : QMessageBox::No;

            if (choice == QMessageBox::Yes) {
                qDebug() << "Capturing general";
                // Capture the general
                // Remove from defender's generals list first
                m_defendingPlayer->removeGeneral(general);
                // Mark as captured
                general->setCapturedBy(m_attackingPlayer->getId());
                // Move to attacker's position
                general->setPosition(combatPosition);
                // Add to attacker's captured list
                m_attackingPlayer->addCapturedGeneral(general);
                qDebug() << "General captured successfully";
            } else {
                qDebug() << "Killing general";
                // Kill the general - remove and delete later
                m_defendingPlayer->removeGeneral(general);
                general->deleteLater();
                qDebug() << "General killed successfully";
            }
        }

        // Transfer territory ownership (but not for sea territories)
        QString territoryName = m_combatTerritoryName;
        Position combatPos = m_mapWidget->territoryNameToPosition(territoryName);
        bool isSea = m_mapWidget->isSeaTerritory(combatPos.row, combatPos.col);

        if (!isSea) {
            // Remove territory from defender
            m_defendingPlayer->unclaimTerritory(territoryName);

            // Add territory to attacker
            m_attackingPlayer->claimTerritory(territoryName);
        }

        // Transfer any cities at this territory from defender to attacker
        City *city = m_defendingPlayer->getCityAtTerritory(m_combatTerritoryName);
        if (city) {
            // Remove from defender
            m_defendingPlayer->removeCity(city);
            // Change ownership to attacker
            city->setOwner(m_attackingPlayer->getId());
            // Add to attacker
            m_attackingPlayer->addCity(city);
        }

        // Move all surviving attacking troops to the conquered territory position
        for (GamePiece *piece : m_attackingTroopButtons.values()) {
            if (piece) {
                piece->setPosition(combatPosition);
            }
        }

        // Also move any attacking leaders (generals, caesars, galleys) to the conquered position
        // Use fresh lists from the player to avoid dangling pointers
        for (GeneralPiece *general : m_attackingPlayer->getGenerals()) {
            if (general && general->getTerritoryName() == m_combatTerritoryName) {
                general->setPosition(combatPosition);  // Already there, but ensure it's set
            }
        }
        for (CaesarPiece *caesar : m_attackingPlayer->getCaesars()) {
            if (caesar && caesar->getTerritoryName() == m_combatTerritoryName) {
                caesar->setPosition(combatPosition);
            }
        }
        for (GalleyPiece *galley : m_attackingPlayer->getGalleys()) {
            if (galley && galley->getTerritoryName() == m_combatTerritoryName) {
                galley->setPosition(combatPosition);
            }
        }

        QString conquestMessage = QString("Attacker Wins!\n\nTerritory %1 has been conquered by Player %2!")
                .arg(territoryName)
                .arg(m_attackingPlayer->getId());

        if (city) {
            conquestMessage += QString("\n\n%1 has been captured!")
                .arg(city->isFortified() ? "Walled City" : "City");
        }

        QMessageBox attackerWinsMsg(this);
        attackerWinsMsg.setWindowTitle("Combat Over");
        attackerWinsMsg.setText(conquestMessage);
        attackerWinsMsg.setIconPixmap(QPixmap(":/images/victoryIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        attackerWinsMsg.setStandardButtons(QMessageBox::Ok);
        attackerWinsMsg.exec();

        m_combatResult = CombatResult::AttackerWins;
        accept();
        return true;
    }

    // Attacker is only defeated when they have no troops AND no galleys
    if (!attackerHasTroops && !attackerHasGalleys) {
        // Defender wins - remove all defeated attacking troops first
        qDebug() << "Defender wins - removing all defeated attacking troops";

        // Remove all defeated troops (they were already eliminated during combat)
        // We need to check fresh lists from the player since m_attackingTroopButtons may be stale
        QList<GamePiece*> defeatedTroops;

        // Get infantry at combat position
        for (InfantryPiece *infantry : m_attackingPlayer->getInfantry()) {
            if (infantry && infantry->getTerritoryName() == m_combatTerritoryName) {
                defeatedTroops.append(infantry);
            }
        }

        // Get cavalry at combat position
        for (CavalryPiece *cavalry : m_attackingPlayer->getCavalry()) {
            if (cavalry && cavalry->getTerritoryName() == m_combatTerritoryName) {
                defeatedTroops.append(cavalry);
            }
        }

        // Get catapults at combat position
        for (CatapultPiece *catapult : m_attackingPlayer->getCatapults()) {
            if (catapult && catapult->getTerritoryName() == m_combatTerritoryName) {
                defeatedTroops.append(catapult);
            }
        }

        // Remove and delete all defeated troops
        for (GamePiece *troop : defeatedTroops) {
            GamePiece::Type type = troop->getType();
            if (type == GamePiece::Type::Infantry) {
                m_attackingPlayer->removeInfantry(static_cast<InfantryPiece*>(troop));
            } else if (type == GamePiece::Type::Cavalry) {
                m_attackingPlayer->removeCavalry(static_cast<CavalryPiece*>(troop));
            } else if (type == GamePiece::Type::Catapult) {
                m_attackingPlayer->removeCatapult(static_cast<CatapultPiece*>(troop));
            }
            troop->deleteLater();
        }
        qDebug() << "Removed" << defeatedTroops.size() << "defeated troops";

        // Check for defeated Caesar first - this is a complete takeover!
        QList<CaesarPiece*> defeatedCaesars;
        const QList<CaesarPiece*> &attackingCaesars = m_attackingPlayer->getCaesars();
        for (CaesarPiece *caesar : attackingCaesars) {
            if (caesar && caesar->getTerritoryName() == m_combatTerritoryName) {
                defeatedCaesars.append(caesar);
            }
        }

        // If Caesar was defeated, complete takeover occurs
        if (!defeatedCaesars.isEmpty()) {
            qDebug() << "Caesar captured! Complete takeover initiated.";

            QMessageBox::information(this, "Caesar Captured!",
                QString("Player %1's Caesar has been captured by Player %2!\n\n"
                        "Player %2 takes over ALL of Player %1's:\n"
                        "• Territories\n"
                        "• Cities\n"
                        "• Pieces (except Caesar - killed)\n"
                        "• Money + 100 talent bonus")
                .arg(m_attackingPlayer->getId())
                .arg(m_defendingPlayer->getId()));

            // Transfer all money + 100 bonus
            int capturedMoney = m_attackingPlayer->getWallet();
            m_attackingPlayer->spendMoney(capturedMoney);  // Take all money
            m_defendingPlayer->addMoney(capturedMoney + 100);  // Add money + bonus

            // Transfer all territories
            QList<QString> territories = m_attackingPlayer->getOwnedTerritories();
            for (const QString &territory : territories) {
                m_attackingPlayer->unclaimTerritory(territory);
                m_defendingPlayer->claimTerritory(territory);
            }

            // Transfer all cities
            QList<City*> cities = m_attackingPlayer->getCities();
            for (City *city : cities) {
                m_attackingPlayer->removeCity(city);
                city->setOwner(m_defendingPlayer->getId());
                m_defendingPlayer->addCity(city);
            }

            // Transfer all generals (they become active generals of the winner)
            QList<GeneralPiece*> generals = m_attackingPlayer->getGenerals();
            for (GeneralPiece *general : generals) {
                m_attackingPlayer->removeGeneral(general);
                general->setPlayer(m_defendingPlayer->getId());
                general->setParent(m_defendingPlayer);
                m_defendingPlayer->addGeneral(general);
            }

            // Transfer captured generals
            QList<GeneralPiece*> capturedGenerals = m_attackingPlayer->getCapturedGenerals();
            for (GeneralPiece *general : capturedGenerals) {
                m_attackingPlayer->removeCapturedGeneral(general);

                // Check if this general originally belonged to the defender (winner)
                if (general->getPlayer() == m_defendingPlayer->getId()) {
                    // Return to original owner as active general
                    general->clearCaptured();
                    general->setParent(m_defendingPlayer);
                    m_defendingPlayer->addGeneral(general);
                } else {
                    // Transfer as captured general to winner
                    general->setCapturedBy(m_defendingPlayer->getId());
                    general->setParent(m_defendingPlayer);
                    m_defendingPlayer->addCapturedGeneral(general);
                }
            }

            // Transfer all troops
            // Infantry
            QList<InfantryPiece*> infantry = m_attackingPlayer->getInfantry();
            for (InfantryPiece *inf : infantry) {
                m_attackingPlayer->removeInfantry(inf);
                inf->setPlayer(m_defendingPlayer->getId());
                inf->setParent(m_defendingPlayer);
                m_defendingPlayer->addInfantry(inf);
            }

            // Cavalry
            QList<CavalryPiece*> cavalry = m_attackingPlayer->getCavalry();
            for (CavalryPiece *cav : cavalry) {
                m_attackingPlayer->removeCavalry(cav);
                cav->setPlayer(m_defendingPlayer->getId());
                cav->setParent(m_defendingPlayer);
                m_defendingPlayer->addCavalry(cav);
            }

            // Catapults
            QList<CatapultPiece*> catapults = m_attackingPlayer->getCatapults();
            for (CatapultPiece *cat : catapults) {
                m_attackingPlayer->removeCatapult(cat);
                cat->setPlayer(m_defendingPlayer->getId());
                cat->setParent(m_defendingPlayer);
                m_defendingPlayer->addCatapult(cat);
            }

            // Galleys
            QList<GalleyPiece*> galleys = m_attackingPlayer->getGalleys();
            for (GalleyPiece *galley : galleys) {
                m_attackingPlayer->removeGalley(galley);
                galley->setPlayer(m_defendingPlayer->getId());
                galley->setParent(m_defendingPlayer);
                m_defendingPlayer->addGalley(galley);
            }

            // Kill all defeated Caesars
            for (CaesarPiece *caesar : defeatedCaesars) {
                m_attackingPlayer->removeCaesar(caesar);
                caesar->deleteLater();
            }

            QMessageBox::information(this, "Complete Takeover",
                QString("Player %1 has been eliminated!\n\n"
                        "Player %2 gained:\n"
                        "• %3 territories\n"
                        "• %4 cities\n"
                        "• %5 generals\n"
                        "• %6 troops\n"
                        "• %7 talents")
                .arg(m_attackingPlayer->getId())
                .arg(m_defendingPlayer->getId())
                .arg(territories.size())
                .arg(cities.size())
                .arg(generals.size())
                .arg(infantry.size() + cavalry.size() + catapults.size() + galleys.size())
                .arg(capturedMoney + 100));

            accept();
            return true;
        }

        // Handle defeated generals (only if no Caesar was captured)
        QList<GeneralPiece*> defeatedGenerals;

        // Get fresh list of generals from the attacking player (still at this position)
        qDebug() << "Collecting defeated generals";
        const QList<GeneralPiece*> &attackingGenerals = m_attackingPlayer->getGenerals();
        for (GeneralPiece *general : attackingGenerals) {
            if (general && general->getTerritoryName() == m_combatTerritoryName) {
                defeatedGenerals.append(general);
            }
        }

        // Process each defeated general
        for (GeneralPiece *general : defeatedGenerals) {
            qDebug() << "Processing defeated general" << general->getPlayer() << "#" << general->getNumber();

            QMessageBox msgBox(this);
            msgBox.setWindowTitle("Capture or Kill General?");
            msgBox.setText(QString("Attacker's General %1 #%2 has been defeated.\n\nDo you want to capture this general?")
                .arg(general->getPlayer())
                .arg(general->getNumber()));
            QPushButton *captureButton = msgBox.addButton("Capture", QMessageBox::YesRole);
            QPushButton *killButton = msgBox.addButton("Kill", QMessageBox::NoRole);
            msgBox.exec();

            QMessageBox::StandardButton choice = (msgBox.clickedButton() == captureButton)
                ? QMessageBox::Yes : QMessageBox::No;

            if (choice == QMessageBox::Yes) {
                qDebug() << "Capturing general";
                // Capture the general
                // Remove from attacker's generals list first
                m_attackingPlayer->removeGeneral(general);
                // Mark as captured
                general->setCapturedBy(m_defendingPlayer->getId());
                // Keep at current position (defender's territory)
                general->setPosition(combatPosition);
                // Add to defender's captured list
                m_defendingPlayer->addCapturedGeneral(general);
                qDebug() << "General captured successfully";
            } else {
                qDebug() << "Killing general";
                // Kill the general - remove and delete later
                m_attackingPlayer->removeGeneral(general);
                general->deleteLater();
                qDebug() << "General killed successfully";
            }
        }

        // Unclaim the combat territory from the attacker (they lost and have no troops here)
        if (m_attackingPlayer->ownsTerritory(m_combatTerritoryName)) {
            qDebug() << "Unclaiming territory" << m_combatTerritoryName << "from defeated attacker" << m_attackingPlayer->getId();
            m_attackingPlayer->unclaimTerritory(m_combatTerritoryName);
        }

        QMessageBox defenderWinsMsg(this);
        defenderWinsMsg.setWindowTitle("Combat Over");
        defenderWinsMsg.setText("Defender Wins! Territory successfully defended.");
        defenderWinsMsg.setIconPixmap(QPixmap(":/images/deadIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        defenderWinsMsg.setStandardButtons(QMessageBox::Ok);
        defenderWinsMsg.exec();

        m_combatResult = CombatResult::DefenderWins;
        accept();
        return true;
    }

    return false;
}

QColor CombatDialog::getTroopColor(GamePiece::Type type) const
{
    switch (type) {
    case GamePiece::Type::Infantry:
        return QColor(144, 238, 144);  // Light green
    case GamePiece::Type::Cavalry:
        return QColor(173, 216, 230);  // Light blue
    case GamePiece::Type::Catapult:
        return QColor(255, 182, 193);  // Light pink
    default:
        return QColor(Qt::white);
    }
}

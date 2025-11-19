#include "combatdialog.h"
#include <QDebug>
#include <QMessageBox>
#include <cstdlib>  // for rand()

CombatDialog::CombatDialog(Player *attackingPlayer,
                           Player *defendingPlayer,
                           const Position &combatPosition,
                           MapWidget *mapWidget,
                           QWidget *parent)
    : QDialog(parent)
    , m_attackingPlayer(attackingPlayer)
    , m_defendingPlayer(defendingPlayer)
    , m_combatPosition(combatPosition)
    , m_mapWidget(mapWidget)
    , m_selectedTarget(nullptr)
    , m_isAttackersTurn(true)
    , m_retreatButton(nullptr)
    , m_attackingHeader(nullptr)
    , m_defendingHeader(nullptr)
{
    setWindowTitle("Combat Resolution");

    // Get all pieces at combat position
    m_attackingPieces = m_attackingPlayer->getPiecesAtPosition(combatPosition);
    m_defendingPieces = m_defendingPlayer->getPiecesAtPosition(combatPosition);

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
        QLabel *messageLabel = new QLabel("Defender has no troops to defend with!\n\nAttacker wins by default!");
        messageLabel->setAlignment(Qt::AlignCenter);
        messageLabel->setStyleSheet("font-size: 14pt; padding: 20px;");
        tempLayout->addWidget(messageLabel);

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
        QLabel *messageLabel = new QLabel("Attacker has no troops!\n\nDefender wins by default!");
        messageLabel->setAlignment(Qt::AlignCenter);
        messageLabel->setStyleSheet("font-size: 14pt; padding: 20px;");
        tempLayout->addWidget(messageLabel);

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

    // Find all leaders (Caesar, General, Galley) with their legions
    for (GamePiece *piece : m_attackingPieces) {
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
        } else if (piece->getType() == GamePiece::Type::Galley) {
            GalleyPiece *galley = static_cast<GalleyPiece*>(piece);
            QGroupBox *legionGroupBox = createLegionGroupBox(galley, galley->getLegion(), true);
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

    // Find all leaders (Caesar, General, Galley) with their legions
    QList<GamePiece*> unledTroops;

    for (GamePiece *piece : m_defendingPieces) {
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
        } else if (piece->getType() == GamePiece::Type::Galley) {
            GalleyPiece *galley = static_cast<GalleyPiece*>(piece);
            QGroupBox *legionGroupBox = createLegionGroupBox(galley, galley->getLegion(), false);
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
            QString currentTerritoryName = m_mapWidget->getTerritoryNameAt(m_combatPosition.row, m_combatPosition.col);
            QLabel *retreatLabel = new QLabel(QString("%1 [%2,%3]")
                                                  .arg(currentTerritoryName)
                                                  .arg(m_combatPosition.row)
                                                  .arg(m_combatPosition.col));
            retreatLabel->setStyleSheet("font-size: 9pt; color: #666; font-style: italic; padding: 2px;");
            retreatLabel->setAlignment(Qt::AlignCenter);
            layout->addWidget(retreatLabel);
        }
    }

    // Add troops in legion
    Player *owningPlayer = (leader->getPlayer() == m_attackingPlayer->getId()) ? m_attackingPlayer : m_defendingPlayer;
    QList<GamePiece*> allPieces = owningPlayer->getPiecesAtPosition(m_combatPosition);

    for (int pieceId : legionIds) {
        for (GamePiece *piece : allPieces) {
            if (piece->getUniqueId() == pieceId) {
                QString troopType;
                if (piece->getType() == GamePiece::Type::Infantry) {
                    troopType = "Infantry";
                } else if (piece->getType() == GamePiece::Type::Cavalry) {
                    troopType = "Cavalry";
                } else if (piece->getType() == GamePiece::Type::Catapult) {
                    troopType = "Catapult";
                }

                QPushButton *troopButton = new QPushButton(QString("%1\nID: %2").arg(troopType).arg(piece->getSerialNumber()));
                troopButton->setMinimumHeight(40);
                troopButton->setMaximumWidth(140);  // Fixed width for buttons

                QColor color = getTroopColor(piece->getType());
                troopButton->setStyleSheet(QString("background-color: %1;").arg(color.name()));

                // Store button-to-piece mapping
                if (isAttacker) {
                    m_attackingTroopButtons[troopButton] = piece;
                    connect(troopButton, &QPushButton::clicked, this, &CombatDialog::onAttackingTroopClicked);
                } else {
                    m_defendingTroopButtons[troopButton] = piece;
                    connect(troopButton, &QPushButton::clicked, this, &CombatDialog::onDefendingTroopClicked);
                }

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
        QString currentTerritoryName = m_mapWidget->getTerritoryNameAt(m_combatPosition.row, m_combatPosition.col);
        QLabel *territoryLabel = new QLabel(QString("%1 [%2,%3]")
                                                .arg(currentTerritoryName)
                                                .arg(m_combatPosition.row)
                                                .arg(m_combatPosition.col));
        territoryLabel->setStyleSheet("font-size: 9pt; color: #666; font-style: italic; padding: 2px;");
        territoryLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(territoryLabel);
    }

    // Add each troop
    for (GamePiece *piece : troops) {
        QString troopType;
        if (piece->getType() == GamePiece::Type::Infantry) {
            troopType = "Infantry";
        } else if (piece->getType() == GamePiece::Type::Cavalry) {
            troopType = "Cavalry";
        } else if (piece->getType() == GamePiece::Type::Catapult) {
            troopType = "Catapult";
        }

        QPushButton *troopButton = new QPushButton(QString("%1\nID: %2").arg(troopType).arg(piece->getSerialNumber()));
        troopButton->setMinimumHeight(40);
        troopButton->setMaximumWidth(140);  // Fixed width for buttons

        QColor color = getTroopColor(piece->getType());
        troopButton->setStyleSheet(QString("background-color: %1;").arg(color.name()));

        // Store button-to-piece mapping
        if (isAttacker) {
            m_attackingTroopButtons[troopButton] = piece;
            connect(troopButton, &QPushButton::clicked, this, &CombatDialog::onAttackingTroopClicked);
        } else {
            m_defendingTroopButtons[troopButton] = piece;
            connect(troopButton, &QPushButton::clicked, this, &CombatDialog::onDefendingTroopClicked);
        }

        layout->addWidget(troopButton);
    }

    layout->addStretch();

    return groupBox;
}

void CombatDialog::setAttackingButtonsEnabled(bool enabled)
{
    for (QGroupBox *groupBox : m_attackingGroupBoxes) {
        groupBox->setEnabled(enabled);
    }
}

void CombatDialog::setDefendingButtonsEnabled(bool enabled)
{
    for (QGroupBox *groupBox : m_defendingGroupBoxes) {
        groupBox->setEnabled(enabled);
    }
}

void CombatDialog::onAttackingTroopClicked()
{
    QPushButton *clickedButton = qobject_cast<QPushButton*>(sender());
    if (!clickedButton) return;

    // Defender selected which attacking troop to attack
    GamePiece *attackingPiece = m_attackingTroopButtons[clickedButton];
    if (!attackingPiece) return;

    qDebug() << "Defender attacking attacking troop ID" << attackingPiece->getSerialNumber();

    // Disable all buttons during combat resolution
    setDefendingButtonsEnabled(false);
    setAttackingButtonsEnabled(false);

    // Calculate defender's advantage
    int advantage = getNetAdvantage(false);

    // Resolve attack
    bool isHit = resolveAttack(attackingPiece->getType(), advantage);

    // Show result dialog
    QString resultMessage;
    if (isHit) {
        resultMessage = QString("HIT! Attacking troop (ID: %1) has been destroyed.")
                            .arg(attackingPiece->getSerialNumber());
    } else {
        resultMessage = QString("MISS! Attacking troop (ID: %1) survived.")
                            .arg(attackingPiece->getSerialNumber());
    }

    QMessageBox::information(this, "Combat Result", resultMessage);

    // After OK is clicked, remove the troop if it was hit
    if (isHit) {
        qDebug() << "Removing attacking troop ID" << attackingPiece->getSerialNumber();
        // Remove the attacking troop button
        removeTroopButton(clickedButton);
        // Remove the piece from the player based on type
        if (attackingPiece->getType() == GamePiece::Type::Infantry) {
            m_attackingPlayer->removeInfantry(static_cast<InfantryPiece*>(attackingPiece));
        } else if (attackingPiece->getType() == GamePiece::Type::Cavalry) {
            m_attackingPlayer->removeCavalry(static_cast<CavalryPiece*>(attackingPiece));
        } else if (attackingPiece->getType() == GamePiece::Type::Catapult) {
            m_attackingPlayer->removeCatapult(static_cast<CatapultPiece*>(attackingPiece));
        }
        attackingPiece->deleteLater();  // Use deleteLater() instead of delete

        // Update advantages (catapult count may have changed)
        updateAdvantageDisplay();

        // Check if combat is over
        if (checkCombatEnd()) {
            return;  // Dialog closed, don't re-enable buttons
        }
    }

    // Switch back to attacker's turn
    setDefendingButtonsEnabled(true);
    setAttackingButtonsEnabled(false);
}

void CombatDialog::onDefendingTroopClicked()
{
    QPushButton *clickedButton = qobject_cast<QPushButton*>(sender());
    if (!clickedButton) return;

    // Attacker selected which defending troop to attack
    GamePiece *defendingPiece = m_defendingTroopButtons[clickedButton];
    if (!defendingPiece) return;

    qDebug() << "Attacker attacking defending troop ID" << defendingPiece->getSerialNumber();

    // Disable all buttons during combat resolution
    setDefendingButtonsEnabled(false);
    setAttackingButtonsEnabled(false);

    // Calculate attacker's advantage
    int advantage = getNetAdvantage(true);

    // Resolve attack
    bool isHit = resolveAttack(defendingPiece->getType(), advantage);

    // Show result dialog
    QString resultMessage;
    if (isHit) {
        resultMessage = QString("HIT! Defending troop (ID: %1) has been destroyed.")
                            .arg(defendingPiece->getSerialNumber());
    } else {
        resultMessage = QString("MISS! Defending troop (ID: %1) survived.")
                            .arg(defendingPiece->getSerialNumber());
    }

    QMessageBox::information(this, "Combat Result", resultMessage);

    // After OK is clicked, remove the troop if it was hit
    if (isHit) {
        qDebug() << "Removing defending troop ID" << defendingPiece->getSerialNumber();
        // Remove the defending troop button
        removeTroopButton(clickedButton);
        // Remove the piece from the player based on type
        if (defendingPiece->getType() == GamePiece::Type::Infantry) {
            m_defendingPlayer->removeInfantry(static_cast<InfantryPiece*>(defendingPiece));
        } else if (defendingPiece->getType() == GamePiece::Type::Cavalry) {
            m_defendingPlayer->removeCavalry(static_cast<CavalryPiece*>(defendingPiece));
        } else if (defendingPiece->getType() == GamePiece::Type::Catapult) {
            m_defendingPlayer->removeCatapult(static_cast<CatapultPiece*>(defendingPiece));
        }
        defendingPiece->deleteLater();  // Use deleteLater() instead of delete

        // Update advantages (catapult count may have changed)
        updateAdvantageDisplay();

        // Check if combat is over
        if (checkCombatEnd()) {
            return;  // Dialog closed, don't re-enable buttons
        }
    }

    // Switch to defender's turn
    setAttackingButtonsEnabled(true);
    setDefendingButtonsEnabled(false);
}

void CombatDialog::onRetreatClicked()
{
    // Move all attacking leaders (and their troops) back to their last territory
    // Use fresh lists from player to avoid dangling pointers

    // Process generals
    for (GeneralPiece *general : m_attackingPlayer->getGenerals()) {
        if (general && general->getPosition() == m_combatPosition && general->hasLastTerritory()) {
            Position retreatPosition = general->getLastTerritory();
            general->setPosition(retreatPosition);

            // Move all troops in this general's legion
            QList<int> legion = general->getLegion();
            for (GamePiece *troop : m_attackingTroopButtons.values()) {
                if (troop && legion.contains(troop->getUniqueId())) {
                    troop->setPosition(retreatPosition);
                }
            }
        }
    }

    // Process caesars
    for (CaesarPiece *caesar : m_attackingPlayer->getCaesars()) {
        if (caesar && caesar->getPosition() == m_combatPosition && caesar->hasLastTerritory()) {
            Position retreatPosition = caesar->getLastTerritory();
            caesar->setPosition(retreatPosition);

            // Move all troops in this caesar's legion
            QList<int> legion = caesar->getLegion();
            for (GamePiece *troop : m_attackingTroopButtons.values()) {
                if (troop && legion.contains(troop->getUniqueId())) {
                    troop->setPosition(retreatPosition);
                }
            }
        }
    }

    // Process galleys
    for (GalleyPiece *galley : m_attackingPlayer->getGalleys()) {
        if (galley && galley->getPosition() == m_combatPosition && galley->hasLastTerritory()) {
            Position retreatPosition = galley->getLastTerritory();
            galley->setPosition(retreatPosition);

            // Move all troops in this galley's legion
            QList<int> legion = galley->getLegion();
            for (GamePiece *troop : m_attackingTroopButtons.values()) {
                if (troop && legion.contains(troop->getUniqueId())) {
                    troop->setPosition(retreatPosition);
                }
            }
        }
    }

    QMessageBox::information(this, "Retreat", "Attacker has retreated! Surviving troops have returned to their previous territory.");
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
        City *city = m_defendingPlayer->getCityAtPosition(m_combatPosition);
        qDebug() << "Checking for city at position" << m_combatPosition.row << m_combatPosition.col;
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

bool CombatDialog::resolveAttack(GamePiece::Type targetType, int attackerAdvantage)
{
    // Roll dice (1-6)
    int roll = (qrand() % 6) + 1;
    int modifiedRoll = roll + attackerAdvantage;

    qDebug() << "Roll:" << roll << "+ Advantage:" << attackerAdvantage << "= Total:" << modifiedRoll;

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
    qDebug() << "checkCombatEnd called";
    bool attackerHasTroops = !m_attackingTroopButtons.isEmpty();
    bool defenderHasTroops = !m_defendingTroopButtons.isEmpty();

    qDebug() << "Attacker has troops:" << attackerHasTroops << "Defender has troops:" << defenderHasTroops;

    if (!defenderHasTroops) {
        qDebug() << "Defender defeated - processing victory";

        // Remove all defeated defending troops first
        qDebug() << "Attacker wins - removing all defeated defending troops";

        // Remove all defeated troops (they were already eliminated during combat)
        QList<GamePiece*> defeatedTroops;

        // Get infantry at combat position
        for (InfantryPiece *infantry : m_defendingPlayer->getInfantry()) {
            if (infantry && infantry->getPosition() == m_combatPosition) {
                defeatedTroops.append(infantry);
            }
        }

        // Get cavalry at combat position
        for (CavalryPiece *cavalry : m_defendingPlayer->getCavalry()) {
            if (cavalry && cavalry->getPosition() == m_combatPosition) {
                defeatedTroops.append(cavalry);
            }
        }

        // Get catapults at combat position
        for (CatapultPiece *catapult : m_defendingPlayer->getCatapults()) {
            if (catapult && catapult->getPosition() == m_combatPosition) {
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
            if (caesar && caesar->getPosition() == m_combatPosition) {
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

            QMessageBox::information(this, "Complete Takeover",
                QString("Player %1 has been eliminated!\n\n"
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

            accept();
            return true;
        }

        // Handle defeated generals (only if no Caesar was captured)
        QList<GeneralPiece*> defeatedGenerals;

        // Get fresh list of generals from the defending player (still at this position)
        qDebug() << "Collecting defeated generals";
        const QList<GeneralPiece*> &defendingGenerals = m_defendingPlayer->getGenerals();
        for (GeneralPiece *general : defendingGenerals) {
            if (general && general->getPosition() == m_combatPosition) {
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
                // Mark as captured (keeps general in original player's list)
                general->setCapturedBy(m_attackingPlayer->getId());
                // Move to attacker's position
                general->setPosition(m_combatPosition);
                // Add to attacker's captured list (for easy reference)
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

        // Transfer territory ownership
        QString territoryName = m_mapWidget->getTerritoryNameAt(m_combatPosition.row, m_combatPosition.col);

        // Remove territory from defender
        m_defendingPlayer->unclaimTerritory(territoryName);

        // Add territory to attacker
        m_attackingPlayer->claimTerritory(territoryName);

        // Transfer any cities at this position from defender to attacker
        City *city = m_defendingPlayer->getCityAtPosition(m_combatPosition);
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
                piece->setPosition(m_combatPosition);
            }
        }

        // Also move any attacking leaders (generals, caesars, galleys) to the conquered position
        // Use fresh lists from the player to avoid dangling pointers
        for (GeneralPiece *general : m_attackingPlayer->getGenerals()) {
            if (general && general->getPosition() == m_combatPosition) {
                general->setPosition(m_combatPosition);  // Already there, but ensure it's set
            }
        }
        for (CaesarPiece *caesar : m_attackingPlayer->getCaesars()) {
            if (caesar && caesar->getPosition() == m_combatPosition) {
                caesar->setPosition(m_combatPosition);
            }
        }
        for (GalleyPiece *galley : m_attackingPlayer->getGalleys()) {
            if (galley && galley->getPosition() == m_combatPosition) {
                galley->setPosition(m_combatPosition);
            }
        }

        QString conquestMessage = QString("Attacker Wins!\n\nTerritory %1 has been conquered by Player %2!")
                .arg(territoryName)
                .arg(m_attackingPlayer->getId());

        if (city) {
            conquestMessage += QString("\n\n%1 has been captured!")
                .arg(city->isFortified() ? "Walled City" : "City");
        }

        QMessageBox::information(this, "Combat Over", conquestMessage);
        accept();
        return true;
    }

    if (!attackerHasTroops) {
        // Defender wins - remove all defeated attacking troops first
        qDebug() << "Defender wins - removing all defeated attacking troops";

        // Remove all defeated troops (they were already eliminated during combat)
        // We need to check fresh lists from the player since m_attackingTroopButtons may be stale
        QList<GamePiece*> defeatedTroops;

        // Get infantry at combat position
        for (InfantryPiece *infantry : m_attackingPlayer->getInfantry()) {
            if (infantry && infantry->getPosition() == m_combatPosition) {
                defeatedTroops.append(infantry);
            }
        }

        // Get cavalry at combat position
        for (CavalryPiece *cavalry : m_attackingPlayer->getCavalry()) {
            if (cavalry && cavalry->getPosition() == m_combatPosition) {
                defeatedTroops.append(cavalry);
            }
        }

        // Get catapults at combat position
        for (CatapultPiece *catapult : m_attackingPlayer->getCatapults()) {
            if (catapult && catapult->getPosition() == m_combatPosition) {
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
            if (caesar && caesar->getPosition() == m_combatPosition) {
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
            if (general && general->getPosition() == m_combatPosition) {
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
                // Mark as captured (keeps general in original player's list)
                general->setCapturedBy(m_defendingPlayer->getId());
                // Keep at current position (defender's territory)
                general->setPosition(m_combatPosition);
                // Add to defender's captured list (for easy reference)
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

        QMessageBox::information(this, "Combat Over", "Defender Wins! Territory successfully defended.");
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

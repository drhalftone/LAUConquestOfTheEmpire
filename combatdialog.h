#ifndef COMBATDIALOG_H
#define COMBATDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include "player.h"
#include "gamepiece.h"
#include "mapwidget.h"

class CombatDialog : public QDialog
{
    Q_OBJECT

public:
    enum class CombatResult {
        AttackerWins = 0,
        DefenderWins = 1,
        AttackerRetreats = 2
    };

    explicit CombatDialog(Player *attackingPlayer,
                         Player *defendingPlayer,
                         const QString &combatTerritoryName,
                         MapWidget *mapWidget,
                         QWidget *parent = nullptr);

    // Get the result of the combat
    CombatResult getCombatResult() const { return m_combatResult; }

private slots:
    void onAttackingTroopClicked();
    void onDefendingTroopClicked();
    void onRetreatClicked();

private:
    // Create the attacking side (left)
    QWidget* createAttackingSide();

    // Create the defending side (right)
    QWidget* createDefendingSide();

    // Create a legion group box with header and troop buttons
    QGroupBox* createLegionGroupBox(GamePiece *leader, const QList<int> &legionIds, bool isAttacker);

    // Create a group box for unled troops (no general/caesar)
    QGroupBox* createUnledTroopsGroupBox(const QList<GamePiece*> &troops, bool isAttacker);

    // Get color for troop type
    QColor getTroopColor(GamePiece::Type type) const;

    // Enable/disable buttons based on turn
    void setAttackingButtonsEnabled(bool enabled);
    void setDefendingButtonsEnabled(bool enabled);

    // Calculate advantages
    int calculateAttackerAdvantage() const;
    int calculateDefenderAdvantage() const;
    int getNetAdvantage(bool forAttacker) const;

    // Update group box titles with advantages
    void updateAdvantageDisplay();

    // Combat resolution
    bool resolveAttack(GamePiece::Type targetType, int attackerAdvantage);
    void removeTroopButton(QPushButton *button);

    // Check if combat is over
    bool checkCombatEnd();

    Player *m_attackingPlayer;
    Player *m_defendingPlayer;
    QString m_combatTerritoryName;
    MapWidget *m_mapWidget;

    QList<GamePiece*> m_attackingPieces;
    QList<GamePiece*> m_defendingPieces;

    // Track all group boxes for enabling/disabling
    QList<QGroupBox*> m_attackingGroupBoxes;
    QList<QGroupBox*> m_defendingGroupBoxes;

    // Track all troop buttons with their piece info
    QMap<QPushButton*, GamePiece*> m_attackingTroopButtons;
    QMap<QPushButton*, GamePiece*> m_defendingTroopButtons;

    // Currently selected target
    QPushButton *m_selectedTarget;
    bool m_isAttackersTurn;

    // Retreat button
    QPushButton *m_retreatButton;

    // Player headers for showing advantages
    QLabel *m_attackingHeader;
    QLabel *m_defendingHeader;

    // Combat result
    CombatResult m_combatResult;
};

#endif // COMBATDIALOG_H

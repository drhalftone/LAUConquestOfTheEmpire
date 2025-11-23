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
#include "laurollingdiewidget.h"

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

    // Set up AI control for one or both sides
    void setupAIMode(bool attackerIsAI, bool defenderIsAI, int delayMs = 1000);

protected:
    void done(int result) override;

private slots:
    void makeAIMove();
    void onAttackingGalleyClicked();
    void onDefendingGalleyClicked();
    void onRetreatClicked();
    void onRollComplete(int dieValue, QObject *sender);

private:
    // Show combat result message with icon
    void showCombatResult(const QString &title, const QString &message);

    // Create the attacking side (left)
    QWidget* createAttackingSide();

    // Create the defending side (right)
    QWidget* createDefendingSide();

    // Create a legion group box with header and troop buttons
    QGroupBox* createLegionGroupBox(GamePiece *leader, const QList<int> &legionIds, bool isAttacker);

    // Create a group box for unled troops (no general/caesar)
    QGroupBox* createUnledTroopsGroupBox(const QList<GamePiece*> &troops, bool isAttacker);

    // Create a galley widget with generals underneath
    QWidget* createGalleyWidget(GalleyPiece *galley, bool isAttacker);

    // Create an empty general placeholder for galleys without passengers
    QGroupBox* createEmptyGeneralGroupBox();

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
    bool resolveAttack(GamePiece::Type targetType, int attackerAdvantage, int dieRoll);
    void removeTroopButton(QPushButton *button);

    // Check if combat is over
    bool checkCombatEnd();

    // Update galley passenger status after a piece is removed
    void updateGalleyPassengerStatus(const QString &galleySerialNumber, bool isAttacker);

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

    // Track galley buttons separately (galleys need special handling - only targetable when empty)
    QMap<QPushButton*, GalleyPiece*> m_attackingGalleyButtons;
    QMap<QPushButton*, GalleyPiece*> m_defendingGalleyButtons;

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

    // Rolling die widget for combat resolution
    LAURollingDieWidget *m_rollingDie;

    // Pending galley button for die roll (galleys need validation before rolling)
    QPushButton *m_pendingGalleyButton;

    // AI control
    bool m_attackerIsAI = false;
    bool m_defenderIsAI = false;
    int m_aiDelayMs = 1000;

    // Track the button being attacked (for visual feedback)
    QPushButton *m_targetedButton = nullptr;
    QString m_targetedButtonOriginalStyle;
};

#endif // COMBATDIALOG_H

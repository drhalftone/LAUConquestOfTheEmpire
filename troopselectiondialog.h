#ifndef TROOPSELECTIONDIALOG_H
#define TROOPSELECTIONDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QLabel>
#include <QList>
#include <QMap>
#include "gamepiece.h"

class TroopSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    static constexpr int MAX_LEGION_SIZE = 6;  // Maximum troops a general can command

    explicit TroopSelectionDialog(const QString &leaderName,
                                  const QList<GamePiece*> &availableTroops,
                                  const QList<int> &currentLegion,
                                  QWidget *parent = nullptr);

    // Get the list of piece IDs that were checked
    QList<int> getSelectedTroopIds() const;

private slots:
    void onCheckboxToggled();

private:
    void updateSelectionCount();

    QMap<int, QCheckBox*> m_checkboxes;  // Maps piece ID to checkbox
    QList<GamePiece*> m_troops;          // Available troops
    QLabel *m_countLabel;                // Shows selected count
};

#endif // TROOPSELECTIONDIALOG_H

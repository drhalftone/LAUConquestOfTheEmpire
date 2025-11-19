#ifndef TROOPSELECTIONDIALOG_H
#define TROOPSELECTIONDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QList>
#include <QMap>
#include "gamepiece.h"

class TroopSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TroopSelectionDialog(const QString &leaderName,
                                  const QList<GamePiece*> &availableTroops,
                                  const QList<int> &currentLegion,
                                  QWidget *parent = nullptr);

    // Get the list of piece IDs that were checked
    QList<int> getSelectedTroopIds() const;

private:
    QMap<int, QCheckBox*> m_checkboxes;  // Maps piece ID to checkbox
    QList<GamePiece*> m_troops;          // Available troops
};

#endif // TROOPSELECTIONDIALOG_H

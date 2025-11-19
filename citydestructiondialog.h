#ifndef CITYDESTRUCTIONDIALOG_H
#define CITYDESTRUCTIONDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QList>
#include <QMap>
#include "building.h"

class CityDestructionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CityDestructionDialog(QChar player, const QList<City*> &cities, QWidget *parent = nullptr);

    // Get the list of cities selected for destruction
    QList<City*> getCitiesToDestroy() const;

private:
    void setupUI();

    QChar m_player;
    QList<City*> m_cities;
    QMap<City*, QCheckBox*> m_cityCheckboxes;  // Map city to its checkbox
};

#endif // CITYDESTRUCTIONDIALOG_H

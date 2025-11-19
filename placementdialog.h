#ifndef PLACEMENTDIALOG_H
#define PLACEMENTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QGridLayout>
#include <QMap>
#include <QMouseEvent>

// Draggable icon label
class DraggableIconLabel : public QLabel
{
    Q_OBJECT

public:
    DraggableIconLabel(const QString &itemType, const QString &text, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QString m_itemType;
    QPoint m_dragStartPosition;
};

class PlacementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PlacementDialog(QChar player,
                           int infantryCount,
                           int cavalryCount,
                           int catapultCount,
                           int galleyCount,
                           int cityCount,
                           int fortificationCount,
                           int roadCount,
                           QWidget *parent = nullptr);

    // Check if all items have been placed
    bool allItemsPlaced() const;

    // Get remaining counts
    int getRemainingInfantry() const { return m_infantryRemaining; }
    int getRemainingCavalry() const { return m_cavalryRemaining; }
    int getRemainingCatapult() const { return m_catapultRemaining; }
    int getRemainingGalley() const { return m_galleyRemaining; }
    int getRemainingCity() const { return m_cityRemaining; }
    int getRemainingFortification() const { return m_fortificationRemaining; }
    int getRemainingRoad() const { return m_roadRemaining; }

public slots:
    void decrementItemCount(QString itemType);

private:
    void setupUI();
    QWidget* createItemBox(const QString &itemName, const QString &iconText,
                          const QColor &color, int count, int &remainingRef);

    QChar m_player;

    // Remaining counts (decremented as items are placed)
    int m_infantryRemaining;
    int m_cavalryRemaining;
    int m_catapultRemaining;
    int m_galleyRemaining;
    int m_cityRemaining;
    int m_fortificationRemaining;
    int m_roadRemaining;

    // Labels to update counts
    QLabel *m_infantryLabel;
    QLabel *m_cavalryLabel;
    QLabel *m_catapultLabel;
    QLabel *m_galleyLabel;
    QLabel *m_cityLabel;
    QLabel *m_fortificationLabel;
    QLabel *m_roadLabel;

    QGridLayout *m_gridLayout;
};

#endif // PLACEMENTDIALOG_H

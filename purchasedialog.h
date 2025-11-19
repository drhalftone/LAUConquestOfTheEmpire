#ifndef PURCHASEDIALOG_H
#define PURCHASEDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QLabel>
#include <QMap>

class PurchaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PurchaseDialog(QChar player, int availableMoney, int inflationMultiplier,
                          int maxCities, int maxFortifications, QWidget *parent = nullptr);

    // Get the quantities of items purchased
    int getInfantryCount() const { return m_infantrySpinBox->value(); }
    int getCavalryCount() const { return m_cavalrySpinBox->value(); }
    int getCatapultCount() const { return m_catapultSpinBox->value(); }
    int getGalleyCount() const { return m_galleySpinBox->value(); }
    int getCityCount() const { return m_citySpinBox->value(); }
    int getFortificationCount() const { return m_fortificationSpinBox->value(); }
    int getRoadCount() const { return 0; }  // Roads are generated automatically, not purchased

    // Get total spent
    int getTotalSpent() const { return m_totalSpent; }

private slots:
    void updateTotals();

private:
    QChar m_player;
    int m_availableMoney;
    int m_inflationMultiplier;
    int m_totalSpent;

    // Base prices (before inflation)
    static const int INFANTRY_BASE_COST = 10;
    static const int CAVALRY_BASE_COST = 20;
    static const int CATAPULT_BASE_COST = 30;
    static const int GALLEY_BASE_COST = 20;
    static const int CITY_BASE_COST = 30;
    static const int FORTIFICATION_BASE_COST = 20;
    static const int ROAD_BASE_COST = 10;

    // Spinboxes for quantities
    QSpinBox *m_infantrySpinBox;
    QSpinBox *m_cavalrySpinBox;
    QSpinBox *m_catapultSpinBox;
    QSpinBox *m_galleySpinBox;
    QSpinBox *m_citySpinBox;
    QSpinBox *m_fortificationSpinBox;
    QSpinBox *m_roadSpinBox;

    // Labels for costs per row
    QLabel *m_infantryCostLabel;
    QLabel *m_cavalryCostLabel;
    QLabel *m_catapultCostLabel;
    QLabel *m_galleyCostLabel;
    QLabel *m_cityCostLabel;
    QLabel *m_fortificationCostLabel;
    QLabel *m_roadCostLabel;

    // Bottom summary labels
    QLabel *m_availableLabel;
    QLabel *m_spendingLabel;
    QLabel *m_remainingLabel;

    int getCurrentPrice(int basePrice) const;
    void setupUI();
};

#endif // PURCHASEDIALOG_H

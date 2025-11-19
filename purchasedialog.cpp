#include "purchasedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QFrame>
#include <QFont>

PurchaseDialog::PurchaseDialog(QChar player, int availableMoney, int inflationMultiplier,
                             int maxCities, int maxFortifications, bool canPurchaseGalleys, QWidget *parent)
    : QDialog(parent)
    , m_player(player)
    , m_availableMoney(availableMoney)
    , m_inflationMultiplier(inflationMultiplier)
    , m_totalSpent(0)
{
    setWindowTitle(QString("Purchase Phase - Player %1").arg(player));
    setModal(true);

    // Disable the close (X) button - force user to click "Done" button
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

    resize(600, 500);

    setupUI();

    // Set maximum values for cities and fortifications
    // Ensure they are never negative to prevent spinbox issues
    m_citySpinBox->setMaximum(qMax(0, maxCities));
    m_fortificationSpinBox->setMaximum(qMax(0, maxFortifications));

    // Disable galley purchases if not allowed (home territory not adjacent to sea)
    if (!canPurchaseGalleys) {
        m_galleySpinBox->setEnabled(false);
        m_galleySpinBox->setToolTip("Galleys can only be purchased if your home territory is adjacent to a sea territory");
    }
}

int PurchaseDialog::getCurrentPrice(int basePrice) const
{
    return basePrice * m_inflationMultiplier;
}

void PurchaseDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Title
    QLabel *titleLabel = new QLabel(QString("Player %1 - Purchase Units & Buildings").arg(m_player));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Inflation notice
    if (m_inflationMultiplier > 1) {
        QLabel *inflationLabel = new QLabel(QString("Inflation Active: Prices ×%1").arg(m_inflationMultiplier));
        QFont inflationFont = inflationLabel->font();
        inflationFont.setPointSize(12);
        inflationFont.setBold(true);
        inflationLabel->setFont(inflationFont);
        inflationLabel->setAlignment(Qt::AlignCenter);
        inflationLabel->setStyleSheet("color: red;");
        mainLayout->addWidget(inflationLabel);
    }

    mainLayout->addSpacing(10);

    // Create table grid
    QGridLayout *tableLayout = new QGridLayout();
    tableLayout->setSpacing(10);

    // Header row
    QLabel *itemHeader = new QLabel("Item");
    QLabel *priceHeader = new QLabel("Unit Price");
    QLabel *quantityHeader = new QLabel("Quantity");
    QLabel *totalHeader = new QLabel("Total Cost");

    QFont headerFont;
    headerFont.setBold(true);
    headerFont.setPointSize(11);
    itemHeader->setFont(headerFont);
    priceHeader->setFont(headerFont);
    quantityHeader->setFont(headerFont);
    totalHeader->setFont(headerFont);

    tableLayout->addWidget(itemHeader, 0, 0);
    tableLayout->addWidget(priceHeader, 0, 1, Qt::AlignCenter);
    tableLayout->addWidget(quantityHeader, 0, 2, Qt::AlignCenter);
    tableLayout->addWidget(totalHeader, 0, 3, Qt::AlignCenter);

    int row = 1;

    // Helper lambda to create row
    auto createRow = [&](const QString &itemName, int basePrice, QSpinBox *&spinBox, QLabel *&costLabel) {
        int currentPrice = getCurrentPrice(basePrice);

        QLabel *nameLabel = new QLabel(itemName);
        QLabel *priceLabel = new QLabel(QString("%1 talents").arg(currentPrice));
        spinBox = new QSpinBox();
        spinBox->setMinimum(0);
        spinBox->setMaximum(999);
        spinBox->setValue(0);
        spinBox->setAlignment(Qt::AlignCenter);
        costLabel = new QLabel("0 talents");
        costLabel->setAlignment(Qt::AlignCenter);

        tableLayout->addWidget(nameLabel, row, 0);
        tableLayout->addWidget(priceLabel, row, 1, Qt::AlignCenter);
        tableLayout->addWidget(spinBox, row, 2);
        tableLayout->addWidget(costLabel, row, 3, Qt::AlignCenter);

        connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PurchaseDialog::updateTotals);

        row++;
    };

    // Create all rows
    createRow("Infantry", INFANTRY_BASE_COST, m_infantrySpinBox, m_infantryCostLabel);
    createRow("Cavalry", CAVALRY_BASE_COST, m_cavalrySpinBox, m_cavalryCostLabel);
    createRow("Catapult", CATAPULT_BASE_COST, m_catapultSpinBox, m_catapultCostLabel);
    createRow("Galley (Ship)", GALLEY_BASE_COST, m_galleySpinBox, m_galleyCostLabel);
    createRow("City", CITY_BASE_COST, m_citySpinBox, m_cityCostLabel);
    createRow("Fortification (Wall)", FORTIFICATION_BASE_COST, m_fortificationSpinBox, m_fortificationCostLabel);
    // Roads are generated automatically and for free - no purchase needed
    // Initialize road spinbox to nullptr to indicate it's not used
    m_roadSpinBox = nullptr;
    m_roadCostLabel = nullptr;

    mainLayout->addLayout(tableLayout);

    // Separator
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // Bottom summary section
    QGridLayout *summaryLayout = new QGridLayout();
    summaryLayout->setSpacing(10);

    QFont summaryFont;
    summaryFont.setPointSize(12);
    summaryFont.setBold(true);

    QLabel *availableTextLabel = new QLabel("Available Money:");
    availableTextLabel->setFont(summaryFont);
    m_availableLabel = new QLabel(QString("%1 talents").arg(m_availableMoney));
    m_availableLabel->setFont(summaryFont);
    m_availableLabel->setStyleSheet("color: green;");

    QLabel *spendingTextLabel = new QLabel("Total Spending:");
    spendingTextLabel->setFont(summaryFont);
    m_spendingLabel = new QLabel("0 talents");
    m_spendingLabel->setFont(summaryFont);
    m_spendingLabel->setStyleSheet("color: blue;");

    QLabel *remainingTextLabel = new QLabel("Remaining:");
    remainingTextLabel->setFont(summaryFont);
    m_remainingLabel = new QLabel(QString("%1 talents").arg(m_availableMoney));
    m_remainingLabel->setFont(summaryFont);
    m_remainingLabel->setStyleSheet("color: green;");

    summaryLayout->addWidget(availableTextLabel, 0, 0, Qt::AlignRight);
    summaryLayout->addWidget(m_availableLabel, 0, 1, Qt::AlignLeft);
    summaryLayout->addWidget(spendingTextLabel, 1, 0, Qt::AlignRight);
    summaryLayout->addWidget(m_spendingLabel, 1, 1, Qt::AlignLeft);
    summaryLayout->addWidget(remainingTextLabel, 2, 0, Qt::AlignRight);
    summaryLayout->addWidget(m_remainingLabel, 2, 1, Qt::AlignLeft);

    mainLayout->addLayout(summaryLayout);

    mainLayout->addSpacing(10);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_doneButton = new QPushButton("Done - End Purchase Phase");
    m_doneButton->setMinimumHeight(40);
    QFont buttonFont = m_doneButton->font();
    buttonFont.setPointSize(11);
    buttonFont.setBold(true);
    m_doneButton->setFont(buttonFont);
    connect(m_doneButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(m_doneButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);
}

void PurchaseDialog::updateTotals()
{
    // Calculate individual costs
    int infantryCost = m_infantrySpinBox->value() * getCurrentPrice(INFANTRY_BASE_COST);
    int cavalryCost = m_cavalrySpinBox->value() * getCurrentPrice(CAVALRY_BASE_COST);
    int catapultCost = m_catapultSpinBox->value() * getCurrentPrice(CATAPULT_BASE_COST);
    int galleyCost = m_galleySpinBox->value() * getCurrentPrice(GALLEY_BASE_COST);
    int cityCost = m_citySpinBox->value() * getCurrentPrice(CITY_BASE_COST);
    int fortificationCost = m_fortificationSpinBox->value() * getCurrentPrice(FORTIFICATION_BASE_COST);

    // Update row cost labels
    m_infantryCostLabel->setText(QString("%1 talents").arg(infantryCost));
    m_cavalryCostLabel->setText(QString("%1 talents").arg(cavalryCost));
    m_catapultCostLabel->setText(QString("%1 talents").arg(catapultCost));
    m_galleyCostLabel->setText(QString("%1 talents").arg(galleyCost));
    m_cityCostLabel->setText(QString("%1 talents").arg(cityCost));
    m_fortificationCostLabel->setText(QString("%1 talents").arg(fortificationCost));

    // Calculate total (roads are free, so not included)
    m_totalSpent = infantryCost + cavalryCost + catapultCost + galleyCost +
                   cityCost + fortificationCost;

    int remaining = m_availableMoney - m_totalSpent;

    // Update summary labels
    m_spendingLabel->setText(QString("%1 talents").arg(m_totalSpent));
    m_remainingLabel->setText(QString("%1 talents").arg(remaining));

    // Update colors and disable Done button if over budget
    if (remaining < 0) {
        m_remainingLabel->setStyleSheet("color: red; font-weight: bold;");
        m_spendingLabel->setStyleSheet("color: red; font-weight: bold;");
        m_doneButton->setEnabled(false);
        m_doneButton->setToolTip("Cannot complete purchase - spending exceeds available money!");
    } else {
        m_remainingLabel->setStyleSheet("color: green; font-weight: bold;");
        m_spendingLabel->setStyleSheet("color: blue; font-weight: bold;");
        m_doneButton->setEnabled(true);
        m_doneButton->setToolTip("");
    }

    // Update maximum values for all spinboxes based on remaining budget
    // This prevents user from going over budget in the first place
    auto updateSpinBoxMax = [this](QSpinBox *spinBox, int basePrice, int currentCostExcludingThis) {
        int price = getCurrentPrice(basePrice);
        if (price > 0) {
            int availableForThis = m_availableMoney - currentCostExcludingThis;
            int maxAffordable = availableForThis / price;
            int currentValue = spinBox->value();

            spinBox->setMinimum(0);  // Ensure minimum stays at 0
            spinBox->setMaximum(qMax(0, maxAffordable));

            // If current value exceeds new max, adjust it down
            if (currentValue > maxAffordable) {
                spinBox->setValue(qMax(0, maxAffordable));
            }
        }
    };

    // Temporarily block signals to prevent recursive updates
    m_infantrySpinBox->blockSignals(true);
    m_cavalrySpinBox->blockSignals(true);
    m_catapultSpinBox->blockSignals(true);
    m_galleySpinBox->blockSignals(true);
    m_citySpinBox->blockSignals(true);
    m_fortificationSpinBox->blockSignals(true);

    // Update each spinbox max based on cost of everything else
    updateSpinBoxMax(m_infantrySpinBox, INFANTRY_BASE_COST, m_totalSpent - infantryCost);
    updateSpinBoxMax(m_cavalrySpinBox, CAVALRY_BASE_COST, m_totalSpent - cavalryCost);
    updateSpinBoxMax(m_catapultSpinBox, CATAPULT_BASE_COST, m_totalSpent - catapultCost);
    updateSpinBoxMax(m_galleySpinBox, GALLEY_BASE_COST, m_totalSpent - galleyCost);
    updateSpinBoxMax(m_citySpinBox, CITY_BASE_COST, m_totalSpent - cityCost);
    updateSpinBoxMax(m_fortificationSpinBox, FORTIFICATION_BASE_COST, m_totalSpent - fortificationCost);

    // Unblock signals
    m_infantrySpinBox->blockSignals(false);
    m_cavalrySpinBox->blockSignals(false);
    m_catapultSpinBox->blockSignals(false);
    m_galleySpinBox->blockSignals(false);
    m_citySpinBox->blockSignals(false);
    m_fortificationSpinBox->blockSignals(false);
}

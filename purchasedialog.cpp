#include "purchasedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QMessageBox>

PurchaseDialog::PurchaseDialog(QChar player,
                               int availableMoney,
                               int inflationMultiplier,
                               const QList<CityPlacementOption> &cityOptions,
                               const QList<FortificationOption> &fortificationOptions,
                               const QList<GalleyPlacementOption> &galleyOptions,
                               int currentGalleyCount,
                               QWidget *parent)
    : QDialog(parent)
    , m_player(player)
    , m_availableMoney(availableMoney)
    , m_inflationMultiplier(inflationMultiplier)
    , m_totalSpent(0)
    , m_currentGalleyCount(currentGalleyCount)
    , m_cityOptions(cityOptions)
    , m_fortificationOptions(fortificationOptions)
    , m_galleyOptions(galleyOptions)
{
    setWindowTitle(QString("Purchase Phase - Player %1").arg(player));
    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    resize(700, 600);

    setupUI();
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

    mainLayout->addSpacing(10);

    // Scroll area for all options
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::StyledPanel);

    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

    // ===== TROOPS SECTION =====
    QGroupBox *troopsGroup = new QGroupBox("Military Units (Placed at Home Province)");
    QGridLayout *troopsLayout = new QGridLayout();

    int row = 0;

    // Infantry
    troopsLayout->addWidget(new QLabel("Infantry:"), row, 0);
    m_infantrySpinBox = new QSpinBox();
    m_infantrySpinBox->setMinimum(0);
    m_infantrySpinBox->setMaximum(999);
    m_infantrySpinBox->setValue(0);
    troopsLayout->addWidget(m_infantrySpinBox, row, 1);
    troopsLayout->addWidget(new QLabel(QString("%1 talents each").arg(getCurrentPrice(INFANTRY_BASE_COST))), row, 2);
    connect(m_infantrySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PurchaseDialog::updateTotals);
    row++;

    // Cavalry
    troopsLayout->addWidget(new QLabel("Cavalry:"), row, 0);
    m_cavalrySpinBox = new QSpinBox();
    m_cavalrySpinBox->setMinimum(0);
    m_cavalrySpinBox->setMaximum(999);
    m_cavalrySpinBox->setValue(0);
    troopsLayout->addWidget(m_cavalrySpinBox, row, 1);
    troopsLayout->addWidget(new QLabel(QString("%1 talents each").arg(getCurrentPrice(CAVALRY_BASE_COST))), row, 2);
    connect(m_cavalrySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PurchaseDialog::updateTotals);
    row++;

    // Catapults
    troopsLayout->addWidget(new QLabel("Catapults:"), row, 0);
    m_catapultSpinBox = new QSpinBox();
    m_catapultSpinBox->setMinimum(0);
    m_catapultSpinBox->setMaximum(999);
    m_catapultSpinBox->setValue(0);
    troopsLayout->addWidget(m_catapultSpinBox, row, 1);
    troopsLayout->addWidget(new QLabel(QString("%1 talents each").arg(getCurrentPrice(CATAPULT_BASE_COST))), row, 2);
    connect(m_catapultSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PurchaseDialog::updateTotals);
    row++;

    troopsGroup->setLayout(troopsLayout);
    scrollLayout->addWidget(troopsGroup);

    // ===== CITIES SECTION =====
    if (!m_cityOptions.isEmpty()) {
        QGroupBox *citiesGroup = new QGroupBox("New Cities");
        QVBoxLayout *citiesLayout = new QVBoxLayout();

        for (const CityPlacementOption &option : m_cityOptions) {
            QHBoxLayout *cityRow = new QHBoxLayout();

            // Regular city checkbox
            QCheckBox *cityCheckbox = new QCheckBox(QString("City at %1").arg(option.territoryName));
            connect(cityCheckbox, &QCheckBox::toggled, this, &PurchaseDialog::updateTotals);
            m_cityCheckboxes[cityCheckbox] = option;
            cityRow->addWidget(cityCheckbox);
            cityRow->addWidget(new QLabel(QString("(%1 talents)").arg(getCurrentPrice(CITY_BASE_COST))));

            // Fortified city checkbox
            QCheckBox *fortifiedCheckbox = new QCheckBox(QString("Fortified City at %1").arg(option.territoryName));
            connect(fortifiedCheckbox, &QCheckBox::toggled, this, [this, cityCheckbox, fortifiedCheckbox]() {
                // If fortified is checked, uncheck regular
                if (fortifiedCheckbox->isChecked()) {
                    cityCheckbox->setChecked(false);
                }
                updateTotals();
            });
            // If regular city is checked, uncheck fortified
            connect(cityCheckbox, &QCheckBox::toggled, this, [fortifiedCheckbox](bool checked) {
                if (checked) {
                    fortifiedCheckbox->setChecked(false);
                }
            });
            m_fortifiedCityCheckboxes[fortifiedCheckbox] = option;
            cityRow->addWidget(fortifiedCheckbox);
            cityRow->addWidget(new QLabel(QString("(%1 talents)").arg(getCurrentPrice(CITY_BASE_COST + FORTIFICATION_BASE_COST))));

            cityRow->addStretch();
            citiesLayout->addLayout(cityRow);
        }

        citiesGroup->setLayout(citiesLayout);
        scrollLayout->addWidget(citiesGroup);
    }

    // ===== FORTIFICATIONS SECTION (for existing cities) =====
    if (!m_fortificationOptions.isEmpty()) {
        QGroupBox *fortificationsGroup = new QGroupBox("Fortify Existing Cities");
        QVBoxLayout *fortificationsLayout = new QVBoxLayout();

        for (const FortificationOption &option : m_fortificationOptions) {
            QHBoxLayout *fortRow = new QHBoxLayout();

            QCheckBox *fortCheckbox = new QCheckBox(QString("Add walls to city at %1").arg(option.territoryName));
            connect(fortCheckbox, &QCheckBox::toggled, this, &PurchaseDialog::updateTotals);
            m_fortificationCheckboxes[fortCheckbox] = option;

            fortRow->addWidget(fortCheckbox);
            fortRow->addWidget(new QLabel(QString("(%1 talents)").arg(getCurrentPrice(FORTIFICATION_BASE_COST))));
            fortRow->addStretch();

            fortificationsLayout->addLayout(fortRow);
        }

        fortificationsGroup->setLayout(fortificationsLayout);
        scrollLayout->addWidget(fortificationsGroup);
    }

    // ===== GALLEYS SECTION =====
    if (!m_galleyOptions.isEmpty()) {
        QGroupBox *galleysGroup = new QGroupBox(
            QString("Galleys (Naval Units) - You own %1/%2")
            .arg(m_currentGalleyCount)
            .arg(MAX_GALLEYS)
        );
        QGridLayout *galleysLayout = new QGridLayout();

        // Calculate how many more galleys can be purchased
        int maxPurchasable = qMax(0, MAX_GALLEYS - m_currentGalleyCount);

        // Add info label if already at max
        if (maxPurchasable == 0) {
            QLabel *maxLabel = new QLabel("You already have the maximum number of galleys (6).");
            QFont font = maxLabel->font();
            font.setBold(true);
            maxLabel->setFont(font);
            maxLabel->setStyleSheet("color: red;");
            galleysLayout->addWidget(maxLabel, 0, 0, 1, 3);
        }

        int galleyRow = (maxPurchasable == 0) ? 1 : 0;
        for (const GalleyPlacementOption &option : m_galleyOptions) {
            QString label = QString("Galleys at %1 border (%2)")
                .arg(option.direction)
                .arg(option.seaTerritoryName);

            galleysLayout->addWidget(new QLabel(label + ":"), galleyRow, 0);

            QSpinBox *galleySpinBox = new QSpinBox();
            galleySpinBox->setMinimum(0);
            galleySpinBox->setMaximum(maxPurchasable);
            galleySpinBox->setValue(0);
            if (maxPurchasable == 0) {
                galleySpinBox->setEnabled(false);
                galleySpinBox->setToolTip("Maximum galley limit (6) already reached");
            }
            connect(galleySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PurchaseDialog::updateTotals);
            m_galleySpinboxes[galleySpinBox] = option;

            galleysLayout->addWidget(galleySpinBox, galleyRow, 1);
            galleysLayout->addWidget(new QLabel(QString("%1 talents each").arg(getCurrentPrice(GALLEY_BASE_COST))), galleyRow, 2);

            galleyRow++;
        }

        galleysGroup->setLayout(galleysLayout);
        scrollLayout->addWidget(galleysGroup);
    }

    scrollLayout->addStretch();
    scrollWidget->setLayout(scrollLayout);
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // ===== SUMMARY SECTION =====
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    QHBoxLayout *summaryLayout = new QHBoxLayout();
    m_availableLabel = new QLabel(QString("Available: %1 talents").arg(m_availableMoney));
    m_spendingLabel = new QLabel("Spending: 0 talents");
    m_remainingLabel = new QLabel(QString("Remaining: %1 talents").arg(m_availableMoney));

    summaryLayout->addWidget(m_availableLabel);
    summaryLayout->addStretch();
    summaryLayout->addWidget(m_spendingLabel);
    summaryLayout->addStretch();
    summaryLayout->addWidget(m_remainingLabel);

    mainLayout->addLayout(summaryLayout);

    // ===== BUTTONS =====
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_purchaseButton = new QPushButton("Complete Purchase");
    m_purchaseButton->setMinimumHeight(40);
    QFont buttonFont = m_purchaseButton->font();
    buttonFont.setPointSize(11);
    buttonFont.setBold(true);
    m_purchaseButton->setFont(buttonFont);
    connect(m_purchaseButton, &QPushButton::clicked, this, &PurchaseDialog::onPurchaseClicked);

    buttonLayout->addWidget(m_purchaseButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    // Initial update
    updateTotals();
}

void PurchaseDialog::updateTotals()
{
    int totalCost = 0;

    // Calculate troop costs
    totalCost += m_infantrySpinBox->value() * getCurrentPrice(INFANTRY_BASE_COST);
    totalCost += m_cavalrySpinBox->value() * getCurrentPrice(CAVALRY_BASE_COST);
    totalCost += m_catapultSpinBox->value() * getCurrentPrice(CATAPULT_BASE_COST);

    // Calculate city costs
    for (auto it = m_cityCheckboxes.begin(); it != m_cityCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            totalCost += getCurrentPrice(CITY_BASE_COST);
        }
    }

    // Calculate fortified city costs
    for (auto it = m_fortifiedCityCheckboxes.begin(); it != m_fortifiedCityCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            totalCost += getCurrentPrice(CITY_BASE_COST + FORTIFICATION_BASE_COST);
        }
    }

    // Calculate fortification costs
    for (auto it = m_fortificationCheckboxes.begin(); it != m_fortificationCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            totalCost += getCurrentPrice(FORTIFICATION_BASE_COST);
        }
    }

    // Calculate galley costs and check total galley count
    int totalGalleysPurchasing = 0;
    for (auto it = m_galleySpinboxes.begin(); it != m_galleySpinboxes.end(); ++it) {
        int count = it.key()->value();
        totalCost += count * getCurrentPrice(GALLEY_BASE_COST);
        totalGalleysPurchasing += count;
    }

    m_totalSpent = totalCost;
    int remaining = m_availableMoney - m_totalSpent;
    int totalGalleysAfterPurchase = m_currentGalleyCount + totalGalleysPurchasing;

    // Update labels
    m_spendingLabel->setText(QString("Spending: %1 talents").arg(m_totalSpent));
    m_remainingLabel->setText(QString("Remaining: %1 talents").arg(remaining));

    // Update colors and button state
    bool overBudget = remaining < 0;
    bool tooManyGalleys = totalGalleysAfterPurchase > MAX_GALLEYS;

    if (overBudget) {
        m_remainingLabel->setStyleSheet("color: red; font-weight: bold;");
        m_spendingLabel->setStyleSheet("color: red; font-weight: bold;");
        m_purchaseButton->setEnabled(false);
        m_purchaseButton->setToolTip("Cannot complete purchase - spending exceeds available money!");
    } else if (tooManyGalleys) {
        m_remainingLabel->setStyleSheet("color: green; font-weight: bold;");
        m_spendingLabel->setStyleSheet("color: blue; font-weight: bold;");
        m_purchaseButton->setEnabled(false);
        m_purchaseButton->setToolTip(QString("Cannot complete purchase - too many galleys! (%1/%2)").arg(totalGalleysAfterPurchase).arg(MAX_GALLEYS));
    } else {
        m_remainingLabel->setStyleSheet("color: green; font-weight: bold;");
        m_spendingLabel->setStyleSheet("color: blue; font-weight: bold;");
        m_purchaseButton->setEnabled(true);
        m_purchaseButton->setToolTip("");
    }
}

PurchaseResult PurchaseDialog::getPurchaseResult() const
{
    PurchaseResult result;
    result.totalCost = m_totalSpent;

    // Get troops
    result.infantry = m_infantrySpinBox->value();
    result.cavalry = m_cavalrySpinBox->value();
    result.catapults = m_catapultSpinBox->value();

    // Get cities
    for (auto it = m_cityCheckboxes.begin(); it != m_cityCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            PurchaseResult::CityPurchase city;
            city.territoryName = it.value().territoryName;
            city.position = it.value().position;
            city.fortified = false;
            result.cities.append(city);
        }
    }

    // Get fortified cities
    for (auto it = m_fortifiedCityCheckboxes.begin(); it != m_fortifiedCityCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            PurchaseResult::CityPurchase city;
            city.territoryName = it.value().territoryName;
            city.position = it.value().position;
            city.fortified = true;
            result.cities.append(city);
        }
    }

    // Get fortifications
    for (auto it = m_fortificationCheckboxes.begin(); it != m_fortificationCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            result.fortifications.append(it.value().territoryName);
        }
    }

    // Get galleys
    for (auto it = m_galleySpinboxes.begin(); it != m_galleySpinboxes.end(); ++it) {
        int count = it.key()->value();
        if (count > 0) {
            PurchaseResult::GalleyPurchase galley;
            galley.seaBorder = it.value().seaPosition;
            galley.count = count;
            result.galleys.append(galley);
        }
    }

    return result;
}

void PurchaseDialog::onPurchaseClicked()
{
    // If nothing was purchased, just accept
    if (m_totalSpent == 0) {
        accept();
        return;
    }

    // Build summary of purchases
    QStringList items;

    // Add troops
    if (m_infantrySpinBox->value() > 0) {
        items.append(QString("%1 Infantry (%2 talents)")
            .arg(m_infantrySpinBox->value())
            .arg(m_infantrySpinBox->value() * getCurrentPrice(INFANTRY_BASE_COST)));
    }
    if (m_cavalrySpinBox->value() > 0) {
        items.append(QString("%1 Cavalry (%2 talents)")
            .arg(m_cavalrySpinBox->value())
            .arg(m_cavalrySpinBox->value() * getCurrentPrice(CAVALRY_BASE_COST)));
    }
    if (m_catapultSpinBox->value() > 0) {
        items.append(QString("%1 Catapults (%2 talents)")
            .arg(m_catapultSpinBox->value())
            .arg(m_catapultSpinBox->value() * getCurrentPrice(CATAPULT_BASE_COST)));
    }

    // Add regular cities
    for (auto it = m_cityCheckboxes.begin(); it != m_cityCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            items.append(QString("City at %1 (%2 talents)")
                .arg(it.value().territoryName)
                .arg(getCurrentPrice(CITY_BASE_COST)));
        }
    }

    // Add fortified cities
    for (auto it = m_fortifiedCityCheckboxes.begin(); it != m_fortifiedCityCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            items.append(QString("Fortified City at %1 (%2 talents)")
                .arg(it.value().territoryName)
                .arg(getCurrentPrice(CITY_BASE_COST + FORTIFICATION_BASE_COST)));
        }
    }

    // Add fortifications
    for (auto it = m_fortificationCheckboxes.begin(); it != m_fortificationCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            items.append(QString("Fortify city at %1 (%2 talents)")
                .arg(it.value().territoryName)
                .arg(getCurrentPrice(FORTIFICATION_BASE_COST)));
        }
    }

    // Add galleys
    for (auto it = m_galleySpinboxes.begin(); it != m_galleySpinboxes.end(); ++it) {
        int count = it.key()->value();
        if (count > 0) {
            items.append(QString("%1 Galleys at %2 border (%3) - %4 talents")
                .arg(count)
                .arg(it.value().direction)
                .arg(it.value().seaTerritoryName)
                .arg(count * getCurrentPrice(GALLEY_BASE_COST)));
        }
    }

    // Build confirmation message
    QString message = QString("Player %1 - Purchase Summary:\n\n").arg(m_player);

    if (items.isEmpty()) {
        message += "No items selected.\n";
    } else {
        for (const QString &item : items) {
            message += "• " + item + "\n";
        }
    }

    message += QString("\nTotal Cost: %1 talents\n").arg(m_totalSpent);
    message += QString("Remaining: %1 talents\n\n").arg(m_availableMoney - m_totalSpent);
    message += "Are you sure you want to complete this purchase?";

    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Purchase",
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        accept();
    }
}

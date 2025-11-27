#include "purchasedialog.h"
#include "building.h"
#include "gamemapwidget.h"
#include "mapgraph.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QMessageBox>
#include <QPixmap>
#include <QPainter>
#include <QApplication>
#include <QDialog>

PurchaseDialog::PurchaseDialog(QChar player,
                               int availableMoney,
                               int inflationMultiplier,
                               const QList<CityPlacementOption> &cityOptions,
                               const QList<FortificationOption> &fortificationOptions,
                               const QList<GalleyPlacementOption> &galleyOptions,
                               int currentGalleyCount,
                               int availableInfantry,
                               int availableCavalry,
                               int availableCatapults,
                               int availableGalleys,
                               const QList<City*> &citiesToDestroy,
                               GameMapWidget *mapWidget,
                               const QString &homeProvinceName,
                               QWidget *parent,
                               bool combatUnitsOnly)
    : QDialog(parent)
    , m_player(player)
    , m_availableMoney(availableMoney)
    , m_inflationMultiplier(inflationMultiplier)
    , m_totalSpent(0)
    , m_currentGalleyCount(currentGalleyCount)
    , m_availableInfantry(availableInfantry)
    , m_availableCavalry(availableCavalry)
    , m_availableCatapults(availableCatapults)
    , m_availableGalleys(availableGalleys)
    , m_cityOptions(cityOptions)
    , m_fortificationOptions(fortificationOptions)
    , m_galleyOptions(galleyOptions)
    , m_availableCitiesToDestroy(citiesToDestroy)
    , m_mapWidget(mapWidget)
    , m_homeProvinceName(homeProvinceName)
    , m_troopsGroupBox(nullptr)
    , m_galleysGroupBox(nullptr)
    , m_combatUnitsOnly(combatUnitsOnly)
{
    if (combatUnitsOnly) {
        setWindowTitle(QString("Build Your Army - Player %1").arg(player));
    } else {
        setWindowTitle(QString("Purchase Phase - Player %1").arg(player));
    }
    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    resize(700, 600);

    setupUI();
}

int PurchaseDialog::getCurrentPrice(int basePrice) const
{
    return basePrice * m_inflationMultiplier;
}

QPixmap PurchaseDialog::createIconCollage(const QString &iconPath, int count) const
{
    // Load the base icon
    QPixmap baseIcon(iconPath);
    if (baseIcon.isNull()) {
        return QPixmap();
    }

    // Scale icon to reasonable size
    QPixmap icon = baseIcon.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Limit display count to avoid huge images
    int displayCount = qMin(count, 10);

    // Calculate collage size based on overlap
    int iconWidth = icon.width();
    int iconHeight = icon.height();
    int overlap = iconWidth / 2;  // 50% overlap
    int collageWidth = iconWidth + (displayCount - 1) * overlap;
    int collageHeight = iconHeight;

    // Create collage pixmap
    QPixmap collage(collageWidth, collageHeight);
    collage.fill(Qt::transparent);

    QPainter painter(&collage);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Draw overlapping icons
    for (int i = 0; i < displayCount; ++i) {
        int x = i * overlap;
        painter.drawPixmap(x, 0, icon);
    }

    // If there are more icons than we can display, add a label
    if (count > displayCount) {
        painter.setPen(Qt::black);
        QFont font = painter.font();
        font.setBold(true);
        font.setPointSize(10);
        painter.setFont(font);
        QString moreText = QString("+%1").arg(count - displayCount);
        painter.drawText(collage.rect(), Qt::AlignRight | Qt::AlignVCenter, moreText);
    }

    painter.end();
    return collage;
}

void PurchaseDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Title
    QString titleText = m_combatUnitsOnly
        ? QString("Player %1 - Build Your Army").arg(m_player)
        : QString("Player %1 - Purchase Units & Buildings").arg(m_player);
    QLabel *titleLabel = new QLabel(titleText);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    mainLayout->addSpacing(10);

    // === City Destruction Section (Optional) ===
    if (!m_availableCitiesToDestroy.isEmpty()) {
        QGroupBox *destructionGroupBox = new QGroupBox("Destroy Cities (Optional)");
        QFont groupFont = destructionGroupBox->font();
        groupFont.setBold(true);
        groupFont.setPointSize(11);
        destructionGroupBox->setFont(groupFont);
        destructionGroupBox->setStyleSheet("QGroupBox { border: 2px solid #d9534f; border-radius: 5px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px 0 5px; }");

        QVBoxLayout *destructionLayout = new QVBoxLayout();

        QLabel *destructionInfo = new QLabel("You may destroy your own cities to prevent them from being captured.\nCities marked during your turn are pre-selected.");
        destructionInfo->setWordWrap(true);
        destructionInfo->setStyleSheet("font-weight: normal; font-style: italic;");
        destructionLayout->addWidget(destructionInfo);

        destructionLayout->addSpacing(5);

        // Create checkboxes for each city
        for (City *city : m_availableCitiesToDestroy) {
            QString cityType = city->isFortified() ? "Walled City" : "City";
            QString cityLabel = QString("%1 at %2").arg(cityType).arg(city->getTerritoryName());

            QCheckBox *checkbox = new QCheckBox(cityLabel);
            checkbox->setChecked(city->isMarkedForDestruction());  // Pre-check if marked
            checkbox->setStyleSheet("font-weight: normal;");

            // Connect to highlight territory on map (like hover effect)
            // Only highlight if checkbox is checked (last checked city is highlighted)
            connect(checkbox, &QCheckBox::toggled, this, [this, city](bool checked) {
                if (m_mapWidget && m_mapWidget->getGraph()) {
                    if (checked) {
                        // Get territory ID and highlight it with hover effect
                        Territory territory = m_mapWidget->getGraph()->getTerritory(city->getTerritoryName());
                        if (territory.id > 0) {
                            m_mapWidget->setHoveredTerritoryById(territory.id);
                        }
                    } else {
                        // Unchecked - clear highlight
                        m_mapWidget->setHoveredTerritoryById(0);
                    }
                }
                // Check if this affects home city and update troop group boxes
                onCityDestructionToggled();
            });

            m_cityDestructionCheckboxes[checkbox] = city;
            destructionLayout->addWidget(checkbox);
        }

        destructionGroupBox->setLayout(destructionLayout);
        mainLayout->addWidget(destructionGroupBox);
        mainLayout->addSpacing(10);
    }

    // Scroll area for all options
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::StyledPanel);

    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

    // ===== TROOPS SECTION =====
    m_troopsGroupBox = new QGroupBox("Military Units (Placed at Home Province)");
    QGridLayout *troopsLayout = new QGridLayout();

    int row = 0;

    // Infantry
    QLabel *infantryIcon = new QLabel();
    infantryIcon->setPixmap(QPixmap(":/images/infantryIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    troopsLayout->addWidget(infantryIcon, row, 0);
    troopsLayout->addWidget(new QLabel("Infantry:"), row, 1);
    m_infantrySpinBox = new QSpinBox();
    m_infantrySpinBox->setMinimum(0);
    m_infantrySpinBox->setMaximum(m_availableInfantry);
    m_infantrySpinBox->setValue(0);
    if (m_availableInfantry == 0) {
        m_infantrySpinBox->setEnabled(false);
        m_infantrySpinBox->setToolTip("No infantry pieces available in the game box");
    }
    troopsLayout->addWidget(m_infantrySpinBox, row, 2);
    troopsLayout->addWidget(new QLabel(QString("%1 talents each").arg(getCurrentPrice(INFANTRY_BASE_COST))), row, 3);
    troopsLayout->addWidget(new QLabel(QString("(%1 available)").arg(m_availableInfantry)), row, 4);
    connect(m_infantrySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PurchaseDialog::updateTotals);
    row++;

    // Cavalry
    QLabel *cavalryIcon = new QLabel();
    cavalryIcon->setPixmap(QPixmap(":/images/cavalryIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    troopsLayout->addWidget(cavalryIcon, row, 0);
    troopsLayout->addWidget(new QLabel("Cavalry:"), row, 1);
    m_cavalrySpinBox = new QSpinBox();
    m_cavalrySpinBox->setMinimum(0);
    m_cavalrySpinBox->setMaximum(m_availableCavalry);
    m_cavalrySpinBox->setValue(0);
    if (m_availableCavalry == 0) {
        m_cavalrySpinBox->setEnabled(false);
        m_cavalrySpinBox->setToolTip("No cavalry pieces available in the game box");
    }
    troopsLayout->addWidget(m_cavalrySpinBox, row, 2);
    troopsLayout->addWidget(new QLabel(QString("%1 talents each").arg(getCurrentPrice(CAVALRY_BASE_COST))), row, 3);
    troopsLayout->addWidget(new QLabel(QString("(%1 available)").arg(m_availableCavalry)), row, 4);
    connect(m_cavalrySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PurchaseDialog::updateTotals);
    row++;

    // Catapults
    QLabel *catapultIcon = new QLabel();
    catapultIcon->setPixmap(QPixmap(":/images/catapultIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    troopsLayout->addWidget(catapultIcon, row, 0);
    troopsLayout->addWidget(new QLabel("Catapults:"), row, 1);
    m_catapultSpinBox = new QSpinBox();
    m_catapultSpinBox->setMinimum(0);
    m_catapultSpinBox->setMaximum(m_availableCatapults);
    m_catapultSpinBox->setValue(0);
    if (m_availableCatapults == 0) {
        m_catapultSpinBox->setEnabled(false);
        m_catapultSpinBox->setToolTip("No catapult pieces available in the game box");
    }
    troopsLayout->addWidget(m_catapultSpinBox, row, 2);
    troopsLayout->addWidget(new QLabel(QString("%1 talents each").arg(getCurrentPrice(CATAPULT_BASE_COST))), row, 3);
    troopsLayout->addWidget(new QLabel(QString("(%1 available)").arg(m_availableCatapults)), row, 4);
    connect(m_catapultSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PurchaseDialog::updateTotals);
    row++;

    m_troopsGroupBox->setLayout(troopsLayout);
    scrollLayout->addWidget(m_troopsGroupBox);

    // ===== CITIES SECTION =====
    // Combine new cities and fortifications into one group with two columns
    // Skip if in combat units only mode
    if (!m_combatUnitsOnly && (!m_cityOptions.isEmpty() || !m_fortificationOptions.isEmpty())) {
        QGroupBox *citiesGroup = new QGroupBox("Cities");
        QGridLayout *citiesLayout = new QGridLayout();

        // Column headers
        QLabel *newCitiesHeader = new QLabel("New Cities");
        QFont headerFont = newCitiesHeader->font();
        headerFont.setBold(true);
        newCitiesHeader->setFont(headerFont);
        citiesLayout->addWidget(newCitiesHeader, 0, 0, 1, 3);

        QLabel *fortifiedHeader = new QLabel("Fortified Cities");
        fortifiedHeader->setFont(headerFont);
        citiesLayout->addWidget(fortifiedHeader, 0, 4, 1, 3);

        int gridRow = 1;

        // Add new city options - each territory can have city (left) OR fortified city (right)
        for (const CityPlacementOption &option : m_cityOptions) {
            // Left column: City icon + checkbox + price
            QLabel *cityIcon = new QLabel();
            cityIcon->setPixmap(QPixmap(":/images/newCityIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            citiesLayout->addWidget(cityIcon, gridRow, 0);

            QCheckBox *cityCheckbox = new QCheckBox(QString("City at %1").arg(option.territoryName));
            connect(cityCheckbox, &QCheckBox::toggled, this, &PurchaseDialog::updateTotals);
            m_cityCheckboxes[cityCheckbox] = option;
            citiesLayout->addWidget(cityCheckbox, gridRow, 1);
            citiesLayout->addWidget(new QLabel(QString("(%1 talents)").arg(getCurrentPrice(CITY_BASE_COST))), gridRow, 2);

            // Spacer column
            citiesLayout->setColumnMinimumWidth(3, 30);

            // Right column: Wall icon + fortified checkbox + price
            QLabel *wallIcon = new QLabel();
            wallIcon->setPixmap(QPixmap(":/images/wallIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            citiesLayout->addWidget(wallIcon, gridRow, 4);

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
            citiesLayout->addWidget(fortifiedCheckbox, gridRow, 5);
            citiesLayout->addWidget(new QLabel(QString("(%1 talents)").arg(getCurrentPrice(CITY_BASE_COST + FORTIFICATION_BASE_COST))), gridRow, 6);

            gridRow++;
        }

        // Add fortification options for existing cities (walls only, no new city)
        // These go in the right column with blank left column
        for (const FortificationOption &option : m_fortificationOptions) {
            // Left column: blank (columns 0, 1, 2)

            // Spacer column
            citiesLayout->setColumnMinimumWidth(3, 30);

            // Right column: Wall icon + fortification checkbox + price
            QLabel *wallIcon = new QLabel();
            wallIcon->setPixmap(QPixmap(":/images/wallIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            citiesLayout->addWidget(wallIcon, gridRow, 4);

            QCheckBox *fortCheckbox = new QCheckBox(QString("Add walls at %1").arg(option.territoryName));
            connect(fortCheckbox, &QCheckBox::toggled, this, &PurchaseDialog::updateTotals);
            m_fortificationCheckboxes[fortCheckbox] = option;
            citiesLayout->addWidget(fortCheckbox, gridRow, 5);
            citiesLayout->addWidget(new QLabel(QString("(%1 talents)").arg(getCurrentPrice(FORTIFICATION_BASE_COST))), gridRow, 6);

            gridRow++;
        }

        citiesGroup->setLayout(citiesLayout);
        scrollLayout->addWidget(citiesGroup);
    }

    // ===== GALLEYS SECTION =====
    // Skip if in combat units only mode
    if (!m_combatUnitsOnly && !m_galleyOptions.isEmpty()) {
        m_galleysGroupBox = new QGroupBox(
            QString("Galleys (Naval Units) - You own %1/%2, %3 available in box")
            .arg(m_currentGalleyCount)
            .arg(MAX_GALLEYS)
            .arg(m_availableGalleys)
        );
        QGridLayout *galleysLayout = new QGridLayout();

        // Calculate how many more galleys can be purchased
        // Limited by both: player's max (6) and pieces available in the game box
        int maxByPlayerLimit = qMax(0, MAX_GALLEYS - m_currentGalleyCount);
        int maxPurchasable = qMin(maxByPlayerLimit, m_availableGalleys);

        // Add info label if can't buy any
        if (maxPurchasable == 0) {
            QString reason;
            if (maxByPlayerLimit == 0) {
                reason = "You already have the maximum number of galleys (6).";
            } else if (m_availableGalleys == 0) {
                reason = "No galley pieces available in the game box.";
            }
            QLabel *maxLabel = new QLabel(reason);
            QFont font = maxLabel->font();
            font.setBold(true);
            maxLabel->setFont(font);
            maxLabel->setStyleSheet("color: red;");
            galleysLayout->addWidget(maxLabel, 0, 0, 1, 4);
        }

        int galleyRow = (maxPurchasable == 0) ? 1 : 0;
        for (const GalleyPlacementOption &option : m_galleyOptions) {
            QString label = QString("Galleys at %1 border (%2)")
                .arg(option.direction)
                .arg(option.seaTerritoryName);

            // Galley icon
            QLabel *galleyIcon = new QLabel();
            galleyIcon->setPixmap(QPixmap(":/images/galleyIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            galleysLayout->addWidget(galleyIcon, galleyRow, 0);

            galleysLayout->addWidget(new QLabel(label + ":"), galleyRow, 1);

            QSpinBox *galleySpinBox = new QSpinBox();
            galleySpinBox->setMinimum(0);
            galleySpinBox->setMaximum(maxPurchasable);
            galleySpinBox->setValue(0);
            if (maxPurchasable == 0) {
                galleySpinBox->setEnabled(false);
                if (maxByPlayerLimit == 0) {
                    galleySpinBox->setToolTip("Maximum galley limit (6) already reached");
                } else {
                    galleySpinBox->setToolTip("No galley pieces available in the game box");
                }
            }
            connect(galleySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PurchaseDialog::updateTotals);
            m_galleySpinboxes[galleySpinBox] = option;

            galleysLayout->addWidget(galleySpinBox, galleyRow, 2);
            galleysLayout->addWidget(new QLabel(QString("%1 talents each").arg(getCurrentPrice(GALLEY_BASE_COST))), galleyRow, 3);

            galleyRow++;
        }

        m_galleysGroupBox->setLayout(galleysLayout);
        scrollLayout->addWidget(m_galleysGroupBox);
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

    m_purchaseButton = new QPushButton("Complete Action");
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

    // Check if home city exists and disable troop/galley groups if it doesn't
    onCityDestructionToggled();
}

void PurchaseDialog::updateTotals()
{
    int totalCost = 0;

    // Calculate troop costs (only if troops group box is enabled)
    if (!m_troopsGroupBox || m_troopsGroupBox->isEnabled()) {
        totalCost += m_infantrySpinBox->value() * getCurrentPrice(INFANTRY_BASE_COST);
        totalCost += m_cavalrySpinBox->value() * getCurrentPrice(CAVALRY_BASE_COST);
        totalCost += m_catapultSpinBox->value() * getCurrentPrice(CATAPULT_BASE_COST);
    }

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

    // Calculate galley costs and check total galley count (only if galleys group box is enabled)
    int totalGalleysPurchasing = 0;
    if (!m_galleysGroupBox || m_galleysGroupBox->isEnabled()) {
        for (auto it = m_galleySpinboxes.begin(); it != m_galleySpinboxes.end(); ++it) {
            int count = it.key()->value();
            totalCost += count * getCurrentPrice(GALLEY_BASE_COST);
            totalGalleysPurchasing += count;
        }
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

    // Get troops (only if troops group box is enabled)
    if (!m_troopsGroupBox || m_troopsGroupBox->isEnabled()) {
        result.infantry = m_infantrySpinBox->value();
        result.cavalry = m_cavalrySpinBox->value();
        result.catapults = m_catapultSpinBox->value();
    } else {
        result.infantry = 0;
        result.cavalry = 0;
        result.catapults = 0;
    }

    // Get cities
    for (auto it = m_cityCheckboxes.begin(); it != m_cityCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            PurchaseResult::CityPurchase city;
            city.territoryName = it.value().territoryName;
            city.fortified = false;
            result.cities.append(city);
        }
    }

    // Get fortified cities
    for (auto it = m_fortifiedCityCheckboxes.begin(); it != m_fortifiedCityCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            PurchaseResult::CityPurchase city;
            city.territoryName = it.value().territoryName;
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

    // Get galleys (only if galleys group box is enabled)
    if (!m_galleysGroupBox || m_galleysGroupBox->isEnabled()) {
        for (auto it = m_galleySpinboxes.begin(); it != m_galleySpinboxes.end(); ++it) {
            int count = it.key()->value();
            if (count > 0) {
                PurchaseResult::GalleyPurchase galley;
                galley.seaTerritoryName = it.value().seaTerritoryName;
                galley.count = count;
                result.galleys.append(galley);
            }
        }
    }

    // Get cities selected for destruction
    for (auto it = m_cityDestructionCheckboxes.begin(); it != m_cityDestructionCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            result.citiesToDestroy.append(it.value());
        }
    }

    return result;
}

void PurchaseDialog::onPurchaseClicked()
{
    // Build confirmation dialog showing both purchases and destructions
    PurchaseResult result = getPurchaseResult();

    bool hasPurchases = (result.infantry > 0 || result.cavalry > 0 || result.catapults > 0 ||
                        !result.cities.isEmpty() || !result.fortifications.isEmpty() || !result.galleys.isEmpty());
    bool hasDestructions = !result.citiesToDestroy.isEmpty();

    if (hasPurchases || hasDestructions) {
        // Create custom confirmation dialog
        QDialog confirmDialog(this);
        confirmDialog.setWindowTitle("Confirm Turn End");
        confirmDialog.setModal(true);

        QHBoxLayout *topLayout = new QHBoxLayout(&confirmDialog);

        // Add icon on the left (burning city if destroying, otherwise normal)
        QLabel *iconLabel = new QLabel();
        QPixmap iconPixmap;
        if (hasDestructions) {
            iconPixmap = QPixmap(":/images/fireCityIcon.png");
        } else {
            iconPixmap = QPixmap(":/images/cityIcon.png");
        }
        iconLabel->setPixmap(iconPixmap.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setAlignment(Qt::AlignTop);
        topLayout->addWidget(iconLabel);

        topLayout->addSpacing(20);

        // Main content on the right
        QVBoxLayout *mainLayout = new QVBoxLayout();

        QLabel *headerLabel = new QLabel(QString("Player %1 - Turn Summary").arg(m_player));
        headerLabel->setStyleSheet("font-weight: bold; font-size: 14pt;");
        mainLayout->addWidget(headerLabel);

        mainLayout->addSpacing(10);

        // Build purchase summary with icons
        if (hasPurchases) {
            QLabel *purchaseHeader = new QLabel("<b>Purchases:</b>");
            mainLayout->addWidget(purchaseHeader);
            mainLayout->addSpacing(5);

            // Create a grid layout for purchases with icons
            QGridLayout *purchaseGrid = new QGridLayout();
            int row = 0;

            // Infantry
            if (result.infantry > 0) {
                QLabel *infantryIcon = new QLabel();
                infantryIcon->setPixmap(QPixmap(":/images/infantryIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                purchaseGrid->addWidget(infantryIcon, row, 0);
                QLabel *infantryLabel = new QLabel(QString("%1 Infantry").arg(result.infantry));
                purchaseGrid->addWidget(infantryLabel, row, 1);
                row++;
            }

            // Cavalry
            if (result.cavalry > 0) {
                QLabel *cavalryIcon = new QLabel();
                cavalryIcon->setPixmap(QPixmap(":/images/cavalryIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                purchaseGrid->addWidget(cavalryIcon, row, 0);
                QLabel *cavalryLabel = new QLabel(QString("%1 Cavalry").arg(result.cavalry));
                purchaseGrid->addWidget(cavalryLabel, row, 1);
                row++;
            }

            // Catapults
            if (result.catapults > 0) {
                QLabel *catapultIcon = new QLabel();
                catapultIcon->setPixmap(QPixmap(":/images/catapultIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                purchaseGrid->addWidget(catapultIcon, row, 0);
                QLabel *catapultLabel = new QLabel(QString("%1 Catapult%2").arg(result.catapults).arg(result.catapults > 1 ? "s" : ""));
                purchaseGrid->addWidget(catapultLabel, row, 1);
                row++;
            }

            // Cities
            for (const auto &city : result.cities) {
                QLabel *cityIcon = new QLabel();
                if (city.fortified) {
                    // Show both city and wall icons for fortified cities
                    QPixmap cityPixmap = QPixmap(":/images/newCityIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    QPixmap wallPixmap = QPixmap(":/images/wallIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    QPixmap combined(48, 24);
                    combined.fill(Qt::transparent);
                    QPainter painter(&combined);
                    painter.drawPixmap(0, 0, cityPixmap);
                    painter.drawPixmap(24, 0, wallPixmap);
                    cityIcon->setPixmap(combined);
                } else {
                    cityIcon->setPixmap(QPixmap(":/images/newCityIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                purchaseGrid->addWidget(cityIcon, row, 0);
                QString cityType = city.fortified ? "Fortified City" : "City";
                QLabel *cityLabel = new QLabel(QString("%1 at %2").arg(cityType).arg(city.territoryName));
                purchaseGrid->addWidget(cityLabel, row, 1);
                row++;
            }

            // Fortifications
            for (const QString &fort : result.fortifications) {
                QLabel *wallIcon = new QLabel();
                wallIcon->setPixmap(QPixmap(":/images/wallIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                purchaseGrid->addWidget(wallIcon, row, 0);
                QLabel *fortLabel = new QLabel(QString("Fortification at %1").arg(fort));
                purchaseGrid->addWidget(fortLabel, row, 1);
                row++;
            }

            // Galleys
            for (const auto &galley : result.galleys) {
                QLabel *galleyIcon = new QLabel();
                galleyIcon->setPixmap(QPixmap(":/images/galleyIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                purchaseGrid->addWidget(galleyIcon, row, 0);
                QLabel *galleyLabel = new QLabel(QString("%1 Galley%2 at %3").arg(galley.count).arg(galley.count > 1 ? "s" : "").arg(galley.seaTerritoryName));
                purchaseGrid->addWidget(galleyLabel, row, 1);
                row++;
            }

            mainLayout->addLayout(purchaseGrid);
            mainLayout->addSpacing(10);

            QLabel *costLabel = new QLabel(QString("<b>Total Cost: %1 talents</b>").arg(result.totalCost));
            mainLayout->addWidget(costLabel);
        }

        if (hasDestructions) {
            if (hasPurchases) {
                mainLayout->addSpacing(15);  // Extra spacing between sections
            }

            QLabel *destroyHeader = new QLabel("<b><span style='color: #d9534f;'>Cities to Destroy:</span></b>");
            mainLayout->addWidget(destroyHeader);
            mainLayout->addSpacing(5);

            // Create a grid layout for cities to destroy with burning city icons
            QGridLayout *destroyGrid = new QGridLayout();
            int row = 0;

            for (City *city : result.citiesToDestroy) {
                QLabel *burnIcon = new QLabel();
                burnIcon->setPixmap(QPixmap(":/images/fireCityIcon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                destroyGrid->addWidget(burnIcon, row, 0);

                QString cityType = city->isFortified() ? "Walled City" : "City";
                QLabel *cityLabel = new QLabel(QString("<span style='color: #d9534f;'>%1 at %2</span>")
                    .arg(cityType)
                    .arg(city->getTerritoryName()));
                destroyGrid->addWidget(cityLabel, row, 1);
                row++;
            }

            mainLayout->addLayout(destroyGrid);
        }

        mainLayout->addSpacing(20);

        // Buttons
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();

        QPushButton *noButton = new QPushButton("No - Go Back");
        noButton->setMinimumHeight(35);
        connect(noButton, &QPushButton::clicked, &confirmDialog, &QDialog::reject);
        buttonLayout->addWidget(noButton);

        buttonLayout->addSpacing(10);

        QPushButton *yesButton = new QPushButton("Yes - End Turn");
        yesButton->setMinimumHeight(35);
        yesButton->setDefault(true);
        QFont yesFont = yesButton->font();
        yesFont.setBold(true);
        yesButton->setFont(yesFont);
        yesButton->setStyleSheet("background-color: #5cb85c; color: white;");
        connect(yesButton, &QPushButton::clicked, &confirmDialog, &QDialog::accept);
        buttonLayout->addWidget(yesButton);

        buttonLayout->addStretch();
        mainLayout->addLayout(buttonLayout);

        topLayout->addLayout(mainLayout);

        // Show confirmation and only accept purchase dialog if user confirms
        if (confirmDialog.exec() != QDialog::Accepted) {
            // User declined - go back to purchase dialog
            return;  // Don't call accept()
        }
    }

    // User confirmed or nothing to confirm - accept the purchase dialog
    accept();
}

void PurchaseDialog::onCityDestructionToggled()
{
    // Check if home city exists or is being destroyed
    bool hasHomeCity = false;
    bool destroyingHomeCity = false;

    // Check if any of the available cities to destroy is the home city
    for (City *city : m_availableCitiesToDestroy) {
        if (city->getTerritoryName() == m_homeProvinceName) {
            hasHomeCity = true;
            // Check if this home city is checked for destruction
            for (auto it = m_cityDestructionCheckboxes.begin(); it != m_cityDestructionCheckboxes.end(); ++it) {
                if (it.value() == city && it.key()->isChecked()) {
                    destroyingHomeCity = true;
                    break;
                }
            }
            break;
        }
    }

    // Determine if troops/galleys should be disabled
    bool shouldDisable = !hasHomeCity || destroyingHomeCity;

    // Disable or enable the troops group box (preserve spinbox values)
    if (m_troopsGroupBox) {
        m_troopsGroupBox->setEnabled(!shouldDisable);
    }

    // Disable or enable the galleys group box (preserve spinbox values)
    if (m_galleysGroupBox) {
        m_galleysGroupBox->setEnabled(!shouldDisable);
    }

    // Update totals after disabling/enabling
    updateTotals();
}

// =============================================================================
// AI Integration
// =============================================================================

QList<PurchaseDialog::PurchaseMenuItem> PurchaseDialog::getAvailableItems() const
{
    QList<PurchaseMenuItem> items;

    // Infantry
    if (m_availableInfantry > 0) {
        PurchaseMenuItem item;
        item.itemType = "Infantry";
        item.currentPrice = getCurrentPrice(INFANTRY_BASE_COST);
        item.maxQuantity = qMin(m_availableInfantry, m_availableMoney / item.currentPrice);
        item.location = "Home Province";
        items.append(item);
    }

    // Cavalry
    if (m_availableCavalry > 0) {
        PurchaseMenuItem item;
        item.itemType = "Cavalry";
        item.currentPrice = getCurrentPrice(CAVALRY_BASE_COST);
        item.maxQuantity = qMin(m_availableCavalry, m_availableMoney / item.currentPrice);
        item.location = "Home Province";
        items.append(item);
    }

    // Catapults
    if (m_availableCatapults > 0) {
        PurchaseMenuItem item;
        item.itemType = "Catapult";
        item.currentPrice = getCurrentPrice(CATAPULT_BASE_COST);
        item.maxQuantity = qMin(m_availableCatapults, m_availableMoney / item.currentPrice);
        item.location = "Home Province";
        items.append(item);
    }

    // Cities (one per territory option)
    int cityPrice = getCurrentPrice(CITY_BASE_COST);
    for (const CityPlacementOption &option : m_cityOptions) {
        PurchaseMenuItem item;
        item.itemType = "City";
        item.currentPrice = cityPrice;
        item.maxQuantity = (m_availableMoney >= cityPrice) ? 1 : 0;
        item.location = option.territoryName;
        items.append(item);
    }

    // Fortified Cities (one per territory option)
    int fortifiedCityPrice = getCurrentPrice(CITY_BASE_COST + FORTIFICATION_BASE_COST);
    for (const CityPlacementOption &option : m_cityOptions) {
        PurchaseMenuItem item;
        item.itemType = "FortifiedCity";
        item.currentPrice = fortifiedCityPrice;
        item.maxQuantity = (m_availableMoney >= fortifiedCityPrice) ? 1 : 0;
        item.location = option.territoryName;
        items.append(item);
    }

    // Fortifications for existing cities
    int fortificationPrice = getCurrentPrice(FORTIFICATION_BASE_COST);
    for (const FortificationOption &option : m_fortificationOptions) {
        PurchaseMenuItem item;
        item.itemType = "Fortification";
        item.currentPrice = fortificationPrice;
        item.maxQuantity = (m_availableMoney >= fortificationPrice) ? 1 : 0;
        item.location = option.territoryName;
        items.append(item);
    }

    // Galleys (per sea border)
    int galleyPrice = getCurrentPrice(GALLEY_BASE_COST);
    int galleysAvailableToBuy = qMin(m_availableGalleys, MAX_GALLEYS - m_currentGalleyCount);
    for (const GalleyPlacementOption &option : m_galleyOptions) {
        PurchaseMenuItem item;
        item.itemType = "Galley";
        item.currentPrice = galleyPrice;
        item.maxQuantity = qMin(galleysAvailableToBuy, m_availableMoney / galleyPrice);
        item.location = QString("%1 (%2)").arg(option.seaTerritoryName).arg(option.direction);
        items.append(item);
    }

    return items;
}

void PurchaseDialog::setupAIAutoMode(int delayMs, const QMap<QString, int> &purchases)
{
    // Set AI mode flag to skip confirmation dialog
    m_aiAutoMode = true;

    // First timer: set the values so user can see them
    QTimer::singleShot(delayMs, this, [this, purchases, delayMs]() {
        qDebug() << "AI Auto-Mode: Interacting with purchase dialog";

        // Set troop quantities
        if (purchases.contains("Infantry") && m_infantrySpinBox) {
            int qty = purchases["Infantry"];
            m_infantrySpinBox->setValue(qty);
            qDebug() << "AI Auto-Mode: Setting infantry to" << qty;
        }
        if (purchases.contains("Cavalry") && m_cavalrySpinBox) {
            int qty = purchases["Cavalry"];
            m_cavalrySpinBox->setValue(qty);
            qDebug() << "AI Auto-Mode: Setting cavalry to" << qty;
        }
        if (purchases.contains("Catapults") && m_catapultSpinBox) {
            int qty = purchases["Catapults"];
            m_catapultSpinBox->setValue(qty);
            qDebug() << "AI Auto-Mode: Setting catapults to" << qty;
        }

        // Check city checkboxes
        for (auto it = m_cityCheckboxes.begin(); it != m_cityCheckboxes.end(); ++it) {
            QString key = QString("City:%1").arg(it.value().territoryName);
            if (purchases.contains(key) && purchases[key] > 0) {
                it.key()->setChecked(true);
                qDebug() << "AI Auto-Mode: Checking city at" << it.value().territoryName;
            }
        }

        // Check fortified city checkboxes
        for (auto it = m_fortifiedCityCheckboxes.begin(); it != m_fortifiedCityCheckboxes.end(); ++it) {
            QString key = QString("FortifiedCity:%1").arg(it.value().territoryName);
            if (purchases.contains(key) && purchases[key] > 0) {
                it.key()->setChecked(true);
                qDebug() << "AI Auto-Mode: Checking fortified city at" << it.value().territoryName;
            }
        }

        // Check fortification checkboxes
        for (auto it = m_fortificationCheckboxes.begin(); it != m_fortificationCheckboxes.end(); ++it) {
            QString key = QString("Fortification:%1").arg(it.value().territoryName);
            if (purchases.contains(key) && purchases[key] > 0) {
                it.key()->setChecked(true);
                qDebug() << "AI Auto-Mode: Checking fortification at" << it.value().territoryName;
            }
        }

        // Set galley quantities
        for (auto it = m_galleySpinboxes.begin(); it != m_galleySpinboxes.end(); ++it) {
            QString key = QString("Galley:%1").arg(it.value().seaTerritoryName);
            if (purchases.contains(key)) {
                it.key()->setValue(purchases[key]);
                qDebug() << "AI Auto-Mode: Setting galleys at" << it.value().seaTerritoryName << "to" << purchases[key];
            }
        }

        // Update totals display
        updateTotals();

        // Second timer: click the purchase button
        // Since m_aiAutoMode is set, onPurchaseClicked() will skip confirmation and accept directly
        QTimer::singleShot(delayMs, this, [this]() {
            qDebug() << "AI Auto-Mode: Clicking purchase button";
            if (m_purchaseButton) {
                m_purchaseButton->click();
            } else {
                accept();
            }
        });
    });
}

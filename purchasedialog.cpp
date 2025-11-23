#include "purchasedialog.h"
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

    troopsGroup->setLayout(troopsLayout);
    scrollLayout->addWidget(troopsGroup);

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
        QGroupBox *galleysGroup = new QGroupBox(
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

    // Create custom confirmation dialog with icon collages
    QDialog confirmDialog(this);
    confirmDialog.setWindowTitle(m_aiAutoMode ? "AI Purchase Summary" : "Confirm Purchase");
    confirmDialog.setModal(true);

    // In AI mode, auto-close after delay so observers can see
    if (m_aiAutoMode) {
        QTimer::singleShot(1500, &confirmDialog, &QDialog::accept);
    }

    QVBoxLayout *mainLayout = new QVBoxLayout(&confirmDialog);

    // Title
    QLabel *titleLabel = new QLabel(QString("Player %1 - Purchase Summary").arg(m_player));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    mainLayout->addSpacing(10);

    // Add troop collages
    if (m_infantrySpinBox->value() > 0) {
        QHBoxLayout *infantryRow = new QHBoxLayout();
        QLabel *infantryCollage = new QLabel();
        infantryCollage->setPixmap(createIconCollage(":/images/infantryIcon.png", m_infantrySpinBox->value()));
        infantryRow->addWidget(infantryCollage);
        infantryRow->addWidget(new QLabel(QString("%1 Infantry - %2 talents")
            .arg(m_infantrySpinBox->value())
            .arg(m_infantrySpinBox->value() * getCurrentPrice(INFANTRY_BASE_COST))));
        infantryRow->addStretch();
        mainLayout->addLayout(infantryRow);
    }

    if (m_cavalrySpinBox->value() > 0) {
        QHBoxLayout *cavalryRow = new QHBoxLayout();
        QLabel *cavalryCollage = new QLabel();
        cavalryCollage->setPixmap(createIconCollage(":/images/cavalryIcon.png", m_cavalrySpinBox->value()));
        cavalryRow->addWidget(cavalryCollage);
        cavalryRow->addWidget(new QLabel(QString("%1 Cavalry - %2 talents")
            .arg(m_cavalrySpinBox->value())
            .arg(m_cavalrySpinBox->value() * getCurrentPrice(CAVALRY_BASE_COST))));
        cavalryRow->addStretch();
        mainLayout->addLayout(cavalryRow);
    }

    if (m_catapultSpinBox->value() > 0) {
        QHBoxLayout *catapultRow = new QHBoxLayout();
        QLabel *catapultCollage = new QLabel();
        catapultCollage->setPixmap(createIconCollage(":/images/catapultIcon.png", m_catapultSpinBox->value()));
        catapultRow->addWidget(catapultCollage);
        catapultRow->addWidget(new QLabel(QString("%1 Catapults - %2 talents")
            .arg(m_catapultSpinBox->value())
            .arg(m_catapultSpinBox->value() * getCurrentPrice(CATAPULT_BASE_COST))));
        catapultRow->addStretch();
        mainLayout->addLayout(catapultRow);
    }

    // Count cities and show collage
    int cityCount = 0;
    int fortifiedCityCount = 0;
    QStringList cityDetails;
    QStringList fortifiedCityDetails;

    for (auto it = m_cityCheckboxes.begin(); it != m_cityCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            cityCount++;
            cityDetails.append(it.value().territoryName);
        }
    }

    for (auto it = m_fortifiedCityCheckboxes.begin(); it != m_fortifiedCityCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            fortifiedCityCount++;
            fortifiedCityDetails.append(it.value().territoryName);
        }
    }

    if (cityCount > 0) {
        QHBoxLayout *cityRow = new QHBoxLayout();
        QLabel *cityCollage = new QLabel();
        cityCollage->setPixmap(createIconCollage(":/images/newCityIcon.png", cityCount));
        cityRow->addWidget(cityCollage);
        cityRow->addWidget(new QLabel(QString("%1 City(s) at %2 - %3 talents")
            .arg(cityCount)
            .arg(cityDetails.join(", "))
            .arg(cityCount * getCurrentPrice(CITY_BASE_COST))));
        cityRow->addStretch();
        mainLayout->addLayout(cityRow);
    }

    if (fortifiedCityCount > 0) {
        QHBoxLayout *fortCityRow = new QHBoxLayout();

        // City icon
        QLabel *cityCollage = new QLabel();
        cityCollage->setPixmap(createIconCollage(":/images/newCityIcon.png", fortifiedCityCount));
        fortCityRow->addWidget(cityCollage);

        // Wall icon
        QLabel *wallCollage = new QLabel();
        wallCollage->setPixmap(createIconCollage(":/images/wallIcon.png", fortifiedCityCount));
        fortCityRow->addWidget(wallCollage);

        fortCityRow->addWidget(new QLabel(QString("%1 Fortified City(s) at %2 - %3 talents")
            .arg(fortifiedCityCount)
            .arg(fortifiedCityDetails.join(", "))
            .arg(fortifiedCityCount * getCurrentPrice(CITY_BASE_COST + FORTIFICATION_BASE_COST))));
        fortCityRow->addStretch();
        mainLayout->addLayout(fortCityRow);
    }

    // Count fortifications (walls added to existing cities)
    int fortificationCount = 0;
    QStringList fortificationDetails;
    for (auto it = m_fortificationCheckboxes.begin(); it != m_fortificationCheckboxes.end(); ++it) {
        if (it.key()->isChecked()) {
            fortificationCount++;
            fortificationDetails.append(it.value().territoryName);
        }
    }

    if (fortificationCount > 0) {
        QHBoxLayout *fortRow = new QHBoxLayout();
        QLabel *wallCollage = new QLabel();
        wallCollage->setPixmap(createIconCollage(":/images/wallIcon.png", fortificationCount));
        fortRow->addWidget(wallCollage);
        fortRow->addWidget(new QLabel(QString("%1 Fortification(s) at %2 - %3 talents")
            .arg(fortificationCount)
            .arg(fortificationDetails.join(", "))
            .arg(fortificationCount * getCurrentPrice(FORTIFICATION_BASE_COST))));
        fortRow->addStretch();
        mainLayout->addLayout(fortRow);
    }

    // Count galleys and show collage
    int totalGalleys = 0;
    QStringList galleyDetails;
    for (auto it = m_galleySpinboxes.begin(); it != m_galleySpinboxes.end(); ++it) {
        int count = it.key()->value();
        if (count > 0) {
            totalGalleys += count;
            galleyDetails.append(QString("%1 at %2 %3")
                .arg(count)
                .arg(it.value().direction)
                .arg(it.value().seaTerritoryName));
        }
    }

    if (totalGalleys > 0) {
        QHBoxLayout *galleyRow = new QHBoxLayout();
        QLabel *galleyCollage = new QLabel();
        galleyCollage->setPixmap(createIconCollage(":/images/galleyIcon.png", totalGalleys));
        galleyRow->addWidget(galleyCollage);
        galleyRow->addWidget(new QLabel(QString("%1 Galley(s): %2 - %3 talents")
            .arg(totalGalleys)
            .arg(galleyDetails.join(", "))
            .arg(totalGalleys * getCurrentPrice(GALLEY_BASE_COST))));
        galleyRow->addStretch();
        mainLayout->addLayout(galleyRow);
    }

    mainLayout->addSpacing(15);

    // Cost summary
    QLabel *costLabel = new QLabel(QString("Total Cost: %1 talents\nRemaining: %2 talents")
        .arg(m_totalSpent)
        .arg(m_availableMoney - m_totalSpent));
    QFont costFont = costLabel->font();
    costFont.setBold(true);
    costLabel->setFont(costFont);
    mainLayout->addWidget(costLabel);

    mainLayout->addSpacing(10);

    QLabel *questionLabel = new QLabel(m_aiAutoMode ?
        "AI has completed purchasing." :
        "Are you sure you want to complete this purchase?");
    mainLayout->addWidget(questionLabel);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *yesButton = new QPushButton(m_aiAutoMode ? "OK" : "Yes");
    connect(yesButton, &QPushButton::clicked, &confirmDialog, &QDialog::accept);
    buttonLayout->addStretch();
    buttonLayout->addWidget(yesButton);

    if (!m_aiAutoMode) {
        QPushButton *noButton = new QPushButton("No");
        connect(noButton, &QPushButton::clicked, &confirmDialog, &QDialog::reject);
        buttonLayout->addWidget(noButton);
    }

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    if (confirmDialog.exec() == QDialog::Accepted) {
        accept();
    }
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
        item.position = option.position;
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
        item.position = option.position;
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
        item.position = option.position;
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
        item.position = option.seaPosition;
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

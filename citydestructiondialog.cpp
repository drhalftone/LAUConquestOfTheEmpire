#include "citydestructiondialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QFrame>
#include <QScrollArea>
#include <QGroupBox>
#include <QMessageBox>

CityDestructionDialog::CityDestructionDialog(QChar player, const QList<City*> &cities, QWidget *parent)
    : QDialog(parent)
    , m_player(player)
    , m_cities(cities)
{
    setWindowTitle(QString("Destroy Cities - Player %1").arg(player));
    setModal(true);

    // Disable the close (X) button - force user to use the dialog buttons
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

    resize(500, 400);

    setupUI();
}

QList<City*> CityDestructionDialog::getCitiesToDestroy() const
{
    QList<City*> citiesToDestroy;

    for (auto it = m_cityCheckboxes.constBegin(); it != m_cityCheckboxes.constEnd(); ++it) {
        if (it.value()->isChecked()) {
            citiesToDestroy.append(it.key());
        }
    }

    return citiesToDestroy;
}

void CityDestructionDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Title
    QLabel *titleLabel = new QLabel(QString("Player %1 - Destroy Cities").arg(m_player));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    mainLayout->addSpacing(10);

    // Instruction text
    QLabel *instructionLabel = new QLabel(
        "You may destroy your own cities to prevent them from being captured by enemies.\n"
        "Select the cities you wish to destroy and click \"Confirm Destruction\".\n"
        "Warning: This action cannot be undone!"
    );
    instructionLabel->setWordWrap(true);
    instructionLabel->setAlignment(Qt::AlignCenter);
    QFont instructionFont = instructionLabel->font();
    instructionFont.setPointSize(10);
    instructionLabel->setFont(instructionFont);
    mainLayout->addWidget(instructionLabel);

    mainLayout->addSpacing(10);

    // If no cities, show message
    if (m_cities.isEmpty()) {
        QLabel *noCitiesLabel = new QLabel("You have no cities to destroy.");
        noCitiesLabel->setAlignment(Qt::AlignCenter);
        QFont noCitiesFont = noCitiesLabel->font();
        noCitiesFont.setPointSize(12);
        noCitiesFont.setItalic(true);
        noCitiesLabel->setFont(noCitiesFont);
        mainLayout->addWidget(noCitiesLabel);
        mainLayout->addStretch();
    } else {
        // Create scroll area for cities list
        QScrollArea *scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::StyledPanel);

        QWidget *scrollWidget = new QWidget();
        QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

        // Add checkbox for each city
        for (City *city : m_cities) {
            QCheckBox *checkbox = new QCheckBox();

            // Build city description
            QString cityDesc = QString("%1 at (%2, %3)")
                .arg(city->getTerritoryName())
                .arg(city->getPosition().row)
                .arg(city->getPosition().col);

            // Add fortification status
            if (city->isFortified()) {
                cityDesc += " [Fortified]";
            } else {
                cityDesc += " [Not Fortified]";
            }

            checkbox->setText(cityDesc);
            checkbox->setFont(QFont("Arial", 10));

            m_cityCheckboxes[city] = checkbox;
            scrollLayout->addWidget(checkbox);
        }

        scrollLayout->addStretch();
        scrollWidget->setLayout(scrollLayout);
        scrollArea->setWidget(scrollWidget);
        mainLayout->addWidget(scrollArea);
    }

    mainLayout->addSpacing(10);

    // Separator
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton *skipButton = new QPushButton("Skip - Don't Destroy Any Cities");
    skipButton->setMinimumHeight(40);
    QFont buttonFont = skipButton->font();
    buttonFont.setPointSize(10);
    skipButton->setFont(buttonFont);
    connect(skipButton, &QPushButton::clicked, this, [this]() {
        // Uncheck all boxes before rejecting
        for (QCheckBox *checkbox : m_cityCheckboxes.values()) {
            checkbox->setChecked(false);
        }
        accept();  // Still accept, but with no cities selected
    });

    QPushButton *confirmButton = new QPushButton("Confirm Destruction");
    confirmButton->setMinimumHeight(40);
    QFont confirmFont = confirmButton->font();
    confirmFont.setPointSize(10);
    confirmFont.setBold(true);
    confirmButton->setFont(confirmFont);
    confirmButton->setStyleSheet("background-color: #d9534f; color: white;");  // Red button for destructive action
    connect(confirmButton, &QPushButton::clicked, this, [this]() {
        // Count how many cities are selected for destruction
        int selectedCount = 0;
        for (QCheckBox *checkbox : m_cityCheckboxes.values()) {
            if (checkbox->isChecked()) {
                selectedCount++;
            }
        }

        // If no cities selected, just accept without confirmation
        if (selectedCount == 0) {
            accept();
            return;
        }

        // Show confirmation dialog
        QString message;
        if (selectedCount == 1) {
            message = "Are you sure you want to destroy 1 city?\n\nThis action cannot be undone!";
        } else {
            message = QString("Are you sure you want to destroy %1 cities?\n\nThis action cannot be undone!").arg(selectedCount);
        }

        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Confirm City Destruction",
            message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No  // Default to No for safety
        );

        if (reply == QMessageBox::Yes) {
            accept();
        }
        // If No, do nothing - dialog stays open
    });

    // Only enable confirm button if player has cities
    confirmButton->setEnabled(!m_cities.isEmpty());

    buttonLayout->addWidget(skipButton);
    buttonLayout->addSpacing(10);
    buttonLayout->addWidget(confirmButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);
}

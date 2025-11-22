#include "troopselectiondialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QPixmap>

TroopSelectionDialog::TroopSelectionDialog(const QString &leaderName,
                                           const QList<GamePiece*> &availableTroops,
                                           const QList<int> &currentLegion,
                                           QWidget *parent)
    : QDialog(parent)
    , m_troops(availableTroops)
    , m_countLabel(nullptr)
{
    setWindowTitle(QString("Select Troops for %1").arg(leaderName));
    setModal(true);
    resize(400, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Title
    QLabel *titleLabel = new QLabel(QString("%1 - Select troops to move together:").arg(leaderName));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // Instruction with limit info
    QLabel *instructionLabel = new QLabel(QString("Check the troops you want to move with this leader.\nMaximum %1 troops per legion. Troops with 0 moves cannot move.").arg(MAX_LEGION_SIZE));
    instructionLabel->setWordWrap(true);
    mainLayout->addWidget(instructionLabel);

    // Selection count label
    m_countLabel = new QLabel();
    QFont countFont = m_countLabel->font();
    countFont.setBold(true);
    m_countLabel->setFont(countFont);
    mainLayout->addWidget(m_countLabel);

    mainLayout->addSpacing(10);

    // Scroll area for checkboxes
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumHeight(300);

    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setSpacing(5);

    // Create checkbox for each troop with icon
    for (GamePiece *piece : availableTroops) {
        QString typeName;
        QString iconPath;
        GamePiece::Type pieceType = piece->getType();

        if (pieceType == GamePiece::Type::Infantry) {
            typeName = "Infantry";
            iconPath = ":/images/infantryIcon.png";
        } else if (pieceType == GamePiece::Type::Cavalry) {
            typeName = "Cavalry";
            iconPath = ":/images/cavalryIcon.png";
        } else if (pieceType == GamePiece::Type::Catapult) {
            typeName = "Catapult";
            iconPath = ":/images/catapultIcon.png";
        } else if (pieceType == GamePiece::Type::Galley) {
            typeName = "Galley";
            iconPath = ":/images/galleyIcon.png";
        } else if (pieceType == GamePiece::Type::General) {
            typeName = QString("General #%1").arg(static_cast<GeneralPiece*>(piece)->getNumber());
            iconPath = ":/images/generalIcon.png";
        } else if (pieceType == GamePiece::Type::Caesar) {
            typeName = "Caesar";
            iconPath = ":/images/ceasarIcon.png";
        } else {
            typeName = "Unknown";
            iconPath = ":/images/infantryIcon.png";  // fallback
        }

        int moves = piece->getMovesRemaining();
        QString label = QString("%1 - ID:%2 (%3 move%4 left)")
                            .arg(typeName)
                            .arg(piece->getUniqueId())
                            .arg(moves)
                            .arg(moves == 1 ? "" : "s");

        // Create horizontal layout for icon + checkbox
        QHBoxLayout *rowLayout = new QHBoxLayout();
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(5);

        // Add icon - load fresh pixmap for each row
        QLabel *iconLabel = new QLabel();
        QPixmap iconPixmap(iconPath);
        if (!iconPixmap.isNull()) {
            iconLabel->setPixmap(iconPixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        iconLabel->setFixedSize(24, 24);
        rowLayout->addWidget(iconLabel);

        // Add checkbox
        QCheckBox *checkbox = new QCheckBox(label);

        // Pre-check if this piece is in the current legion
        if (currentLegion.contains(piece->getUniqueId())) {
            checkbox->setChecked(true);
        }

        // Disable checkbox if no moves remaining (but still show it)
        if (moves == 0) {
            checkbox->setStyleSheet("color: gray;");
        }

        m_checkboxes[piece->getUniqueId()] = checkbox;
        rowLayout->addWidget(checkbox);
        rowLayout->addStretch();

        // Connect checkbox to update selection count and enforce limit
        connect(checkbox, &QCheckBox::toggled, this, &TroopSelectionDialog::onCheckboxToggled);

        scrollLayout->addLayout(rowLayout);
    }

    // Initialize the selection count display
    updateSelectionCount();

    scrollLayout->addStretch();
    scrollWidget->setLayout(scrollLayout);
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // Separator
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton *okButton = new QPushButton("OK");
    QPushButton *cancelButton = new QPushButton("Cancel");

    okButton->setDefault(true);
    okButton->setMinimumWidth(80);
    cancelButton->setMinimumWidth(80);

    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);
}

QList<int> TroopSelectionDialog::getSelectedTroopIds() const
{
    QList<int> selectedIds;

    for (auto it = m_checkboxes.begin(); it != m_checkboxes.end(); ++it) {
        if (it.value()->isChecked()) {
            selectedIds.append(it.key());
        }
    }

    return selectedIds;
}

void TroopSelectionDialog::onCheckboxToggled()
{
    updateSelectionCount();
}

void TroopSelectionDialog::updateSelectionCount()
{
    int selectedCount = 0;
    for (auto it = m_checkboxes.begin(); it != m_checkboxes.end(); ++it) {
        if (it.value()->isChecked()) {
            selectedCount++;
        }
    }

    // Update the count label
    QString countText = QString("Selected: %1 / %2").arg(selectedCount).arg(MAX_LEGION_SIZE);
    if (selectedCount > MAX_LEGION_SIZE) {
        m_countLabel->setText(countText + " (TOO MANY!)");
        m_countLabel->setStyleSheet("color: red;");
    } else if (selectedCount == MAX_LEGION_SIZE) {
        m_countLabel->setText(countText);
        m_countLabel->setStyleSheet("color: orange;");
    } else {
        m_countLabel->setText(countText);
        m_countLabel->setStyleSheet("color: green;");
    }

    // Enable/disable unchecked checkboxes based on whether limit is reached
    bool atLimit = (selectedCount >= MAX_LEGION_SIZE);
    for (auto it = m_checkboxes.begin(); it != m_checkboxes.end(); ++it) {
        QCheckBox *checkbox = it.value();
        if (!checkbox->isChecked()) {
            // Disable unchecked boxes if at limit
            checkbox->setEnabled(!atLimit);
        } else {
            // Always keep checked boxes enabled so user can uncheck them
            checkbox->setEnabled(true);
        }
    }
}

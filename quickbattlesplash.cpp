#include "quickbattlesplash.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFont>
#include <QSettings>

QuickBattleSplash::QuickBattleSplash(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Quick Battle");
    setModal(true);
    setFixedSize(350, 400);

    setupUI();
    loadSettings();
}

void QuickBattleSplash::loadSettings()
{
    QSettings settings("LAU", "QuickBattle");
    m_budgetSpinBox->setValue(settings.value("budget", 200).toInt());
    m_attackerAICheckBox->setChecked(settings.value("attackerAI", false).toBool());
    m_defenderAICheckBox->setChecked(settings.value("defenderAI", false).toBool());
    m_defenderCityCheckBox->setChecked(settings.value("defenderCity", false).toBool());
    m_defenderFortificationCheckBox->setChecked(settings.value("defenderFortification", false).toBool());
}

void QuickBattleSplash::saveSettings()
{
    QSettings settings("LAU", "QuickBattle");
    settings.setValue("budget", m_budgetSpinBox->value());
    settings.setValue("attackerAI", m_attackerAICheckBox->isChecked());
    settings.setValue("defenderAI", m_defenderAICheckBox->isChecked());
    settings.setValue("defenderCity", m_defenderCityCheckBox->isChecked());
    settings.setValue("defenderFortification", m_defenderFortificationCheckBox->isChecked());
}

void QuickBattleSplash::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Title with icon
    QHBoxLayout *titleLayout = new QHBoxLayout();
    QLabel *iconLabel = new QLabel();
    iconLabel->setPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    titleLayout->addWidget(iconLabel);

    QLabel *titleLabel = new QLabel("Quick Battle");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    mainLayout->addLayout(titleLayout);

    // Budget section
    QGroupBox *budgetGroup = new QGroupBox("Budget");
    QHBoxLayout *budgetLayout = new QHBoxLayout();
    QLabel *budgetLabel = new QLabel("Talents per side:");
    m_budgetSpinBox = new QSpinBox();
    m_budgetSpinBox->setRange(50, 500);
    m_budgetSpinBox->setSingleStep(10);
    m_budgetSpinBox->setValue(200);
    m_budgetSpinBox->setSuffix(" talents");
    budgetLayout->addWidget(budgetLabel);
    budgetLayout->addWidget(m_budgetSpinBox);
    budgetGroup->setLayout(budgetLayout);
    mainLayout->addWidget(budgetGroup);

    // Attacker section
    QGroupBox *attackerGroup = new QGroupBox("Attacker");
    QVBoxLayout *attackerLayout = new QVBoxLayout();
    m_attackerAICheckBox = new QCheckBox("AI Controlled");
    attackerLayout->addWidget(m_attackerAICheckBox);
    attackerGroup->setLayout(attackerLayout);
    mainLayout->addWidget(attackerGroup);

    // Defender section
    QGroupBox *defenderGroup = new QGroupBox("Defender");
    QVBoxLayout *defenderLayout = new QVBoxLayout();
    m_defenderAICheckBox = new QCheckBox("AI Controlled");
    m_defenderCityCheckBox = new QCheckBox("Has City (+1 defense)");
    m_defenderFortificationCheckBox = new QCheckBox("Has Fortification (+1 defense)");
    defenderLayout->addWidget(m_defenderAICheckBox);
    defenderLayout->addWidget(m_defenderCityCheckBox);
    defenderLayout->addWidget(m_defenderFortificationCheckBox);
    defenderGroup->setLayout(defenderLayout);
    mainLayout->addWidget(defenderGroup);

    mainLayout->addStretch();

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start Battle");
    m_startButton->setDefault(true);
    m_quitButton = new QPushButton("Quit");

    // Style the start button
    m_startButton->setMinimumHeight(40);
    QFont buttonFont = m_startButton->font();
    buttonFont.setBold(true);
    m_startButton->setFont(buttonFont);

    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_quitButton);
    mainLayout->addLayout(buttonLayout);

    // Connect buttons
    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        saveSettings();
        accept();
    });
    connect(m_quitButton, &QPushButton::clicked, this, &QDialog::reject);
}

int QuickBattleSplash::getBudget() const
{
    return m_budgetSpinBox->value();
}

bool QuickBattleSplash::isAttackerAI() const
{
    return m_attackerAICheckBox->isChecked();
}

bool QuickBattleSplash::isDefenderAI() const
{
    return m_defenderAICheckBox->isChecked();
}

bool QuickBattleSplash::defenderHasCity() const
{
    return m_defenderCityCheckBox->isChecked();
}

bool QuickBattleSplash::defenderHasFortification() const
{
    return m_defenderFortificationCheckBox->isChecked();
}

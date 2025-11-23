#ifndef QUICKBATTLESPLASH_H
#define QUICKBATTLESPLASH_H

#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>

class QuickBattleSplash : public QDialog
{
    Q_OBJECT

public:
    explicit QuickBattleSplash(QWidget *parent = nullptr);

    // Get configuration values
    int getBudget() const;
    bool isAttackerAI() const;
    bool isDefenderAI() const;
    bool defenderHasCity() const;
    bool defenderHasFortification() const;

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    QSpinBox *m_budgetSpinBox;
    QCheckBox *m_attackerAICheckBox;
    QCheckBox *m_defenderAICheckBox;
    QCheckBox *m_defenderCityCheckBox;
    QCheckBox *m_defenderFortificationCheckBox;
    QPushButton *m_startButton;
    QPushButton *m_quitButton;
};

#endif // QUICKBATTLESPLASH_H

/*********************************************************************************
 *                                                                               *
 * Copyright (c) 2025, Dr. Daniel L. Lau                                         *
 * All rights reserved.                                                          *
 *                                                                               *
 * Redistribution and use in source and binary forms, with or without            *
 * modification, are permitted provided that the following conditions are met:   *
 * 1. Redistributions of source code must retain the above copyright             *
 *    notice, this list of conditions and the following disclaimer.              *
 * 2. Redistributions in binary form must reproduce the above copyright          *
 *    notice, this list of conditions and the following disclaimer in the        *
 *    documentation and/or other materials provided with the distribution.       *
 * 3. All advertising materials mentioning features or use of this software      *
 *    must display the following acknowledgement:                                *
 *    This product includes software developed by the <organization>.            *
 * 4. Neither the name of the <organization> nor the                             *
 *    names of its contributors may be used to endorse or promote products       *
 *    derived from this software without specific prior written permission.      *
 *                                                                               *
 * THIS SOFTWARE IS PROVIDED BY Dr. Daniel L. Lau ''AS IS'' AND ANY              *
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED     *
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE        *
 * DISCLAIMED. IN NO EVENT SHALL Dr. Daniel L. Lau BE LIABLE FOR ANY             *
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES    *
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;  *
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND   *
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT    *
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS *
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                  *
 *                                                                               *
 *********************************************************************************/

#include "lauscoresheetwidget.h"
#include <QFont>
#include <QSettings>
#include <algorithm>

LAUScoreSheetWidget::LAUScoreSheetWidget(QWidget *parent)
    : QWidget(parent)
{
    layout = new QGridLayout(this);

    // Initialize scores and used arrays
    scores.resize(NUM_CATEGORIES);
    used.resize(NUM_CATEGORIES);
    categoryLabels.resize(NUM_CATEGORIES);
    accumulatedScoreLabels.resize(NUM_CATEGORIES);
    potentialScoreButtons.resize(NUM_CATEGORIES);

    for (int i = 0; i < NUM_CATEGORIES; ++i) {
        scores[i] = -1;
        used[i] = false;
    }

    // Category names
    QStringList upperNames = {"Aces", "Twos", "Threes", "Fours", "Fives", "Sixes"};
    QStringList lowerNames = {"3 of a Kind", "4 of a Kind", "Full House",
                              "Sm. Straight", "Lg. Straight", "YAHTZEE", "Chance"};

    int row = 0;

    // Title
    QLabel *titleLabel = new QLabel("YAHTZEE SCORE SHEET", this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(12);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel, row++, 0, 1, 4);

    // Column headers
    layout->addWidget(new QLabel("Category", this), row, 0);
    QLabel *scoredHeader = new QLabel("Scored", this);
    QFont headerFont = scoredHeader->font();
    headerFont.setBold(true);
    scoredHeader->setFont(headerFont);
    scoredHeader->setAlignment(Qt::AlignCenter);
    layout->addWidget(scoredHeader, row, 1);

    QLabel *potentialHeader = new QLabel("Potential", this);
    potentialHeader->setFont(headerFont);
    potentialHeader->setAlignment(Qt::AlignCenter);
    layout->addWidget(potentialHeader, row++, 2);

    // Upper Section Header
    QLabel *upperHeader = new QLabel("UPPER SECTION", this);
    QFont sectionHeaderFont = upperHeader->font();
    sectionHeaderFont.setBold(true);
    upperHeader->setFont(sectionHeaderFont);
    layout->addWidget(upperHeader, row++, 0, 1, 4);

    // Upper section categories
    for (int i = 0; i < 6; ++i) {
        categoryLabels[i] = new QLabel(upperNames[i], this);
        layout->addWidget(categoryLabels[i], row, 0);

        // Accumulated score column
        accumulatedScoreLabels[i] = new QLabel("--", this);
        accumulatedScoreLabels[i]->setAlignment(Qt::AlignCenter);
        accumulatedScoreLabels[i]->setMinimumWidth(60);
        layout->addWidget(accumulatedScoreLabels[i], row, 1);

        // Potential score column (clickable button)
        potentialScoreButtons[i] = new QPushButton("", this);
        potentialScoreButtons[i]->setMinimumWidth(60);
        potentialScoreButtons[i]->setEnabled(false);
        layout->addWidget(potentialScoreButtons[i], row, 2);

        connect(potentialScoreButtons[i], &QPushButton::clicked, [this, i]() {
            scoreCategory(static_cast<ScoreCategory>(i));
        });

        row++;
    }

    // Upper section totals
    QFont boldFont;
    boldFont.setBold(true);

    layout->addWidget(new QLabel("SUBTOTAL", this), row, 0);
    upperSubtotalLabel = new QLabel("0", this);
    upperSubtotalLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(upperSubtotalLabel, row, 1);
    upperSubtotalPotentialLabel = new QLabel("", this);
    upperSubtotalPotentialLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(upperSubtotalPotentialLabel, row++, 2);

    layout->addWidget(new QLabel("BONUS (35 if >= 63)", this), row, 0);
    bonusLabel = new QLabel("0", this);
    bonusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(bonusLabel, row, 1);
    bonusPotentialLabel = new QLabel("", this);
    bonusPotentialLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(bonusPotentialLabel, row++, 2);

    layout->addWidget(new QLabel("UPPER TOTAL", this), row, 0);
    upperTotalLabel = new QLabel("0", this);
    upperTotalLabel->setFont(boldFont);
    upperTotalLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(upperTotalLabel, row, 1);
    upperTotalPotentialLabel = new QLabel("", this);
    upperTotalPotentialLabel->setFont(boldFont);
    upperTotalPotentialLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(upperTotalPotentialLabel, row++, 2);

    // Lower Section Header
    QLabel *lowerHeader = new QLabel("LOWER SECTION", this);
    lowerHeader->setFont(sectionHeaderFont);
    layout->addWidget(lowerHeader, row++, 0, 1, 4);

    // Lower section categories
    for (int i = 0; i < 7; ++i) {
        int catIndex = i + 6;  // Offset by upper section
        categoryLabels[catIndex] = new QLabel(lowerNames[i], this);
        layout->addWidget(categoryLabels[catIndex], row, 0);

        // Accumulated score column
        accumulatedScoreLabels[catIndex] = new QLabel("--", this);
        accumulatedScoreLabels[catIndex]->setAlignment(Qt::AlignCenter);
        accumulatedScoreLabels[catIndex]->setMinimumWidth(60);
        layout->addWidget(accumulatedScoreLabels[catIndex], row, 1);

        // Potential score column (clickable button)
        potentialScoreButtons[catIndex] = new QPushButton("", this);
        potentialScoreButtons[catIndex]->setMinimumWidth(60);
        potentialScoreButtons[catIndex]->setEnabled(false);
        layout->addWidget(potentialScoreButtons[catIndex], row, 2);

        connect(potentialScoreButtons[catIndex], &QPushButton::clicked, [this, catIndex]() {
            scoreCategory(static_cast<ScoreCategory>(catIndex));
        });

        row++;
    }

    // Lower total
    layout->addWidget(new QLabel("LOWER TOTAL", this), row, 0);
    lowerTotalLabel = new QLabel("0", this);
    lowerTotalLabel->setFont(boldFont);
    lowerTotalLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(lowerTotalLabel, row, 1);
    lowerTotalPotentialLabel = new QLabel("", this);
    lowerTotalPotentialLabel->setFont(boldFont);
    lowerTotalPotentialLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(lowerTotalPotentialLabel, row++, 2);

    // Grand total
    layout->addWidget(new QLabel("GRAND TOTAL", this), row, 0);
    QFont grandFont;
    grandFont.setBold(true);
    grandFont.setPointSize(14);
    grandTotalLabel = new QLabel("0", this);
    grandTotalLabel->setFont(grandFont);
    grandTotalLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(grandTotalLabel, row, 1);
    grandTotalPotentialLabel = new QLabel("", this);
    grandTotalPotentialLabel->setFont(grandFont);
    grandTotalPotentialLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(grandTotalPotentialLabel, row++, 2);

    // Set column stretch to prevent excessive white space
    layout->setColumnStretch(0, 1);  // Category column can grow
    layout->setColumnStretch(1, 0);  // Scored column fixed
    layout->setColumnStretch(2, 0);  // Potential column fixed

    setMaximumWidth(400);
    adjustSize();
}

LAUScoreSheetWidget::~LAUScoreSheetWidget()
{
}

void LAUScoreSheetWidget::updatePotentialScores(QVector<int> diceValues)
{
    currentDiceValues = diceValues;

    // Calculate potential scores for all categories
    int upperSubtotalPotential = 0;
    int lowerTotalPotential = 0;

    // Update button text to show potential scores
    for (int i = 0; i < NUM_CATEGORIES; ++i) {
        if (!used[i]) {
            int potentialScore = calculateScore(static_cast<ScoreCategory>(i), diceValues);
            potentialScoreButtons[i]->setText(QString::number(potentialScore));
            potentialScoreButtons[i]->setEnabled(true);
            potentialScoreButtons[i]->setVisible(true);

            // Add to potential totals
            if (i <= SIXES) {
                upperSubtotalPotential += potentialScore;
            } else {
                lowerTotalPotential += potentialScore;
            }
        } else {
            // Hide potential for already used categories
            potentialScoreButtons[i]->setText("");
            potentialScoreButtons[i]->setEnabled(false);
            potentialScoreButtons[i]->setVisible(false);
        }
    }

    // Don't show totals in potential column - only show individual category scores
}

void LAUScoreSheetWidget::scoreCategory(ScoreCategory category)
{
    if (used[category]) return;

    int score = calculateScore(category, currentDiceValues);
    scores[category] = score;
    used[category] = true;

    // Move score from potential column to accumulated column
    accumulatedScoreLabels[category]->setText(QString::number(score));
    potentialScoreButtons[category]->setText("");
    potentialScoreButtons[category]->setEnabled(false);

    // Update accumulated totals
    int upperSubtotal = 0;
    for (int i = ACES; i <= SIXES; ++i) {
        if (used[i]) {
            upperSubtotal += scores[i];
        }
    }
    upperSubtotalLabel->setText(QString::number(upperSubtotal));

    int bonus = (upperSubtotal >= 63) ? 35 : 0;
    bonusLabel->setText(QString::number(bonus));

    int upperTotal = upperSubtotal + bonus;
    upperTotalLabel->setText(QString::number(upperTotal));

    int lowerTotal = 0;
    for (int i = THREE_OF_KIND; i <= CHANCE; ++i) {
        if (used[i]) {
            lowerTotal += scores[i];
        }
    }
    lowerTotalLabel->setText(QString::number(lowerTotal));

    int grandTotal = upperTotal + lowerTotal;
    grandTotalLabel->setText(QString::number(grandTotal));

    // Only hide the button that was just used
    potentialScoreButtons[category]->setText("");
    potentialScoreButtons[category]->setEnabled(false);
    potentialScoreButtons[category]->setVisible(false);

    emit categoryScored(category, score);
}

int LAUScoreSheetWidget::getTotalScore() const
{
    return grandTotalLabel->text().toInt();
}

void LAUScoreSheetWidget::reset()
{
    for (int i = 0; i < NUM_CATEGORIES; ++i) {
        scores[i] = -1;
        used[i] = false;
        accumulatedScoreLabels[i]->setText("--");
        potentialScoreButtons[i]->setText("");
        potentialScoreButtons[i]->setEnabled(false);
        potentialScoreButtons[i]->setVisible(false);
    }

    // Reset accumulated totals
    upperSubtotalLabel->setText("0");
    bonusLabel->setText("0");
    upperTotalLabel->setText("0");
    lowerTotalLabel->setText("0");
    grandTotalLabel->setText("0");

    // Clear potential totals
    upperSubtotalPotentialLabel->setText("");
    bonusPotentialLabel->setText("");
    upperTotalPotentialLabel->setText("");
    lowerTotalPotentialLabel->setText("");
    grandTotalPotentialLabel->setText("");

    currentDiceValues.clear();
}

int LAUScoreSheetWidget::calculateScore(ScoreCategory category, QVector<int> diceValues)
{
    switch (category) {
        case ACES:   return countOccurrences(diceValues, 1) * 1;
        case TWOS:   return countOccurrences(diceValues, 2) * 2;
        case THREES: return countOccurrences(diceValues, 3) * 3;
        case FOURS:  return countOccurrences(diceValues, 4) * 4;
        case FIVES:  return countOccurrences(diceValues, 5) * 5;
        case SIXES:  return countOccurrences(diceValues, 6) * 6;

        case THREE_OF_KIND:
            return hasNOfAKind(diceValues, 3) ? sumDice(diceValues) : 0;

        case FOUR_OF_KIND:
            return hasNOfAKind(diceValues, 4) ? sumDice(diceValues) : 0;

        case FULL_HOUSE:
            return isFullHouse(diceValues) ? 25 : 0;

        case SMALL_STRAIGHT:
            return isSmallStraight(diceValues) ? 30 : 0;

        case LARGE_STRAIGHT:
            return isLargeStraight(diceValues) ? 40 : 0;

        case YAHTZEE:
            return isYahtzee(diceValues) ? 50 : 0;

        case CHANCE:
            return sumDice(diceValues);

        default:
            return 0;
    }
}

int LAUScoreSheetWidget::countOccurrences(QVector<int> diceValues, int value)
{
    int count = 0;
    for (int v : diceValues) {
        if (v == value) count++;
    }
    return count;
}

bool LAUScoreSheetWidget::hasNOfAKind(QVector<int> diceValues, int n)
{
    for (int i = 1; i <= 6; ++i) {
        if (countOccurrences(diceValues, i) >= n) {
            return true;
        }
    }
    return false;
}

bool LAUScoreSheetWidget::isFullHouse(QVector<int> diceValues)
{
    QVector<int> counts(7, 0);  // Index 0 unused, 1-6 for die values
    for (int v : diceValues) {
        counts[v]++;
    }

    bool hasThree = false;
    bool hasTwo = false;
    for (int count : counts) {
        if (count == 3) hasThree = true;
        if (count == 2) hasTwo = true;
    }

    return hasThree && hasTwo;
}

bool LAUScoreSheetWidget::isSmallStraight(QVector<int> diceValues)
{
    QVector<bool> present(7, false);
    for (int v : diceValues) {
        present[v] = true;
    }

    // Check for 1-2-3-4, 2-3-4-5, or 3-4-5-6
    return (present[1] && present[2] && present[3] && present[4]) ||
           (present[2] && present[3] && present[4] && present[5]) ||
           (present[3] && present[4] && present[5] && present[6]);
}

bool LAUScoreSheetWidget::isLargeStraight(QVector<int> diceValues)
{
    QVector<bool> present(7, false);
    for (int v : diceValues) {
        present[v] = true;
    }

    // Check for 1-2-3-4-5 or 2-3-4-5-6
    return (present[1] && present[2] && present[3] && present[4] && present[5]) ||
           (present[2] && present[3] && present[4] && present[5] && present[6]);
}

bool LAUScoreSheetWidget::isYahtzee(QVector<int> diceValues)
{
    if (diceValues.isEmpty()) return false;
    int first = diceValues[0];
    for (int v : diceValues) {
        if (v != first) return false;
    }
    return true;
}

int LAUScoreSheetWidget::sumDice(QVector<int> diceValues)
{
    int sum = 0;
    for (int v : diceValues) {
        sum += v;
    }
    return sum;
}

void LAUScoreSheetWidget::saveState()
{
    QSettings settings("LAU", "Yahtzee");

    // Save scores and used flags
    settings.beginWriteArray("categories", NUM_CATEGORIES);
    for (int i = 0; i < NUM_CATEGORIES; ++i) {
        settings.setArrayIndex(i);
        settings.setValue("score", scores[i]);
        settings.setValue("used", used[i]);
    }
    settings.endArray();
}

void LAUScoreSheetWidget::loadState()
{
    QSettings settings("LAU", "Yahtzee");

    // Load scores and used flags
    int size = settings.beginReadArray("categories");
    for (int i = 0; i < qMin(size, static_cast<int>(NUM_CATEGORIES)); ++i) {
        settings.setArrayIndex(i);
        scores[i] = settings.value("score", -1).toInt();
        used[i] = settings.value("used", false).toBool();

        // Update UI
        if (used[i]) {
            accumulatedScoreLabels[i]->setText(QString::number(scores[i]));
            // Hide the potential button for used categories
            potentialScoreButtons[i]->setText("");
            potentialScoreButtons[i]->setEnabled(false);
            potentialScoreButtons[i]->setVisible(false);
        } else {
            accumulatedScoreLabels[i]->setText("--");
        }
    }
    settings.endArray();

    // Recalculate and update all totals
    int upperSubtotal = 0;
    for (int i = ACES; i <= SIXES; ++i) {
        if (used[i]) {
            upperSubtotal += scores[i];
        }
    }
    upperSubtotalLabel->setText(QString::number(upperSubtotal));

    int bonus = (upperSubtotal >= 63) ? 35 : 0;
    bonusLabel->setText(QString::number(bonus));

    int upperTotal = upperSubtotal + bonus;
    upperTotalLabel->setText(QString::number(upperTotal));

    int lowerTotal = 0;
    for (int i = THREE_OF_KIND; i <= CHANCE; ++i) {
        if (used[i]) {
            lowerTotal += scores[i];
        }
    }
    lowerTotalLabel->setText(QString::number(lowerTotal));

    int grandTotal = upperTotal + lowerTotal;
    grandTotalLabel->setText(QString::number(grandTotal));
}

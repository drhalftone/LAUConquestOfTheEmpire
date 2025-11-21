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

#ifndef LAUSCORESHEETWIDGET_H
#define LAUSCORESHEETWIDGET_H

#include <QWidget>
#include <QVector>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>

enum ScoreCategory {
    ACES = 0,
    TWOS,
    THREES,
    FOURS,
    FIVES,
    SIXES,
    THREE_OF_KIND,
    FOUR_OF_KIND,
    FULL_HOUSE,
    SMALL_STRAIGHT,
    LARGE_STRAIGHT,
    YAHTZEE,
    CHANCE,
    NUM_CATEGORIES
};

class LAUScoreSheetWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LAUScoreSheetWidget(QWidget *parent = nullptr);
    ~LAUScoreSheetWidget();

    // Update potential scores for current dice
    void updatePotentialScores(QVector<int> diceValues);

    // Score a category
    void scoreCategory(ScoreCategory category);

    // Get current total score
    int getTotalScore() const;

    // Reset score sheet
    void reset();

    // Save/load state
    void saveState();
    void loadState();

signals:
    void categoryScored(ScoreCategory category, int score);

private:
    // Calculate score for a category
    int calculateScore(ScoreCategory category, QVector<int> diceValues);

    // Helper functions for scoring
    int countOccurrences(QVector<int> diceValues, int value);
    bool hasNOfAKind(QVector<int> diceValues, int n);
    bool isFullHouse(QVector<int> diceValues);
    bool isSmallStraight(QVector<int> diceValues);
    bool isLargeStraight(QVector<int> diceValues);
    bool isYahtzee(QVector<int> diceValues);
    int sumDice(QVector<int> diceValues);

    QGridLayout *layout;
    QVector<QLabel*> categoryLabels;
    QVector<QLabel*> accumulatedScoreLabels;  // Column 1: Accumulated scores
    QVector<QPushButton*> potentialScoreButtons;  // Column 2: Potential scores (clickable)
    QVector<int> scores;  // -1 means not scored yet
    QVector<bool> used;   // Track which categories have been used

    QLabel *upperSubtotalLabel;
    QLabel *upperSubtotalPotentialLabel;
    QLabel *bonusLabel;
    QLabel *bonusPotentialLabel;
    QLabel *upperTotalLabel;
    QLabel *upperTotalPotentialLabel;
    QLabel *lowerTotalLabel;
    QLabel *lowerTotalPotentialLabel;
    QLabel *grandTotalLabel;
    QLabel *grandTotalPotentialLabel;

    QVector<int> currentDiceValues;
};

#endif // LAUSCORESHEETWIDGET_H

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

#ifndef LAUROLLINGDIEWIDGET_H
#define LAUROLLINGDIEWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QRandomGenerator>
#include <QVector>

class LAURollingDieWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LAURollingDieWidget(int numDice = 1, QWidget *parent = nullptr);
    ~LAURollingDieWidget();

    // Get the current dice values
    QVector<int> getDiceValues() const { return diceValues; }

    // Get value of specific die (0-indexed), defaults to first die
    int getValue(int index = 0) const;

    // Get number of dice
    int getNumDice() const { return numDice; }

    // Check if currently rolling
    bool rolling() const { return isRolling; }

    // Roll dice - if no indices provided, rolls all dice. Otherwise rolls only selected indices.
    void roll(QVector<int> indicesToRoll = QVector<int>());

    // Select/deselect all dice
    void selectAll();
    void deselectAll();

    // Check if die is selected
    bool isSelected(int index) const;

    // Enable/disable selection (for first roll where selection shouldn't be allowed)
    void setSelectionEnabled(bool enabled) { selectionEnabled = enabled; }

    // Save/load state
    void saveState();
    void loadState();

signals:
    void rollComplete(QVector<int> values);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void onRollTimer();

private:
    void drawDieFace(QPainter &painter, int value, int orientation, const QRect &rect, bool selected);
    void drawPip(QPainter &painter, int x, int y, int size);
    void startRolling(QVector<int> indicesToRoll);
    int getDieIndexAtPosition(const QPoint &pos);

    int numDice;
    QVector<int> diceValues;
    QVector<int> diceOrientations;
    QVector<bool> diceSelected;
    QVector<int> rollingIndices;
    QTimer *rollTimer;
    bool isRolling;
    bool selectionEnabled;
    int rollCount;
    int maxRolls;
};

#endif // LAUROLLINGDIEWIDGET_H

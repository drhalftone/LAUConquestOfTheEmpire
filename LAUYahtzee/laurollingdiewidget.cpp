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

#include "laurollingdiewidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QSettings>

LAURollingDieWidget::LAURollingDieWidget(int numDice, QWidget *parent)
    : QWidget(parent), numDice(numDice), isRolling(false), selectionEnabled(false), rollCount(0), maxRolls(30)
{
    setWindowTitle(QString("Rolling Dice"));

    // Initialize all dice with random values
    diceValues.resize(numDice);
    diceOrientations.resize(numDice);
    diceSelected.resize(numDice);
    for (int i = 0; i < numDice; ++i) {
        diceValues[i] = QRandomGenerator::global()->bounded(1, 7);
        diceOrientations[i] = QRandomGenerator::global()->bounded(0, 4);
        diceSelected[i] = false;  // Start with no dice selected
    }

    // Resize window based on number of dice
    int dieSize = 200;
    int spacing = 20;
    int totalWidth = numDice * dieSize + (numDice + 1) * spacing;
    int totalHeight = dieSize + 2 * spacing;
    resize(totalWidth, totalHeight);

    // Create and setup timer
    rollTimer = new QTimer(this);
    connect(rollTimer, &QTimer::timeout, this, &LAURollingDieWidget::onRollTimer);
}

LAURollingDieWidget::~LAURollingDieWidget()
{
}

int LAURollingDieWidget::getValue(int index) const
{
    if (index >= 0 && index < diceValues.size()) {
        return diceValues[index];
    }
    return 0;  // Return 0 if index is out of bounds
}

void LAURollingDieWidget::roll(QVector<int> indicesToRoll)
{
    if (isRolling) return;  // Don't start new roll if already rolling

    if (indicesToRoll.isEmpty()) {
        // Roll all dice
        for (int i = 0; i < numDice; ++i) {
            indicesToRoll.append(i);
        }
    }

    startRolling(indicesToRoll);
}

void LAURollingDieWidget::selectAll()
{
    for (int i = 0; i < numDice; ++i) {
        diceSelected[i] = true;
    }
    update();
}

void LAURollingDieWidget::deselectAll()
{
    for (int i = 0; i < numDice; ++i) {
        diceSelected[i] = false;
    }
    update();
}

bool LAURollingDieWidget::isSelected(int index) const
{
    if (index >= 0 && index < diceSelected.size()) {
        return diceSelected[index];
    }
    return false;
}

void LAURollingDieWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background (black)
    painter.fillRect(rect(), Qt::black);

    // Calculate die size and spacing
    int spacing = 20;
    int availableWidth = width() - (numDice + 1) * spacing;
    int availableHeight = height() - 2 * spacing;
    int dieSize = qMin(availableWidth / numDice, availableHeight);

    // Calculate total width of all dice including spacing between them
    int totalDiceWidth = numDice * dieSize + (numDice - 1) * spacing;

    // Center the entire group of dice horizontally
    int startX = (width() - totalDiceWidth) / 2;

    // Draw all dice side by side, centered both horizontally and vertically
    for (int i = 0; i < numDice; ++i) {
        int x = startX + i * (dieSize + spacing);
        int y = (height() - dieSize) / 2;
        QRect dieRect(x, y, dieSize, dieSize);
        drawDieFace(painter, diceValues[i], diceOrientations[i], dieRect, diceSelected[i]);
    }
}

void LAURollingDieWidget::drawDieFace(QPainter &painter, int value, int orientation, const QRect &rect, bool selected)
{
    // Save painter state
    painter.save();

    // Set up coordinate system: work in a fixed 100x100 space
    // This ensures proportions stay consistent regardless of actual size
    qreal scale = rect.width() / 100.0;
    painter.translate(rect.topLeft());
    painter.scale(scale, scale);

    // Draw die background (red or green if selected) - now in 100x100 coordinate space
    if (selected) {
        painter.setBrush(QColor(0, 180, 0));  // Green for selected
        painter.setPen(QPen(QColor(0, 255, 0), 3));  // Bright green border
    } else {
        painter.setBrush(Qt::red);
        painter.setPen(QPen(Qt::darkRed, 2));
    }
    painter.drawRoundedRect(QRect(0, 0, 100, 100), 5, 5);

    // Calculate pip size and positions in fixed 100x100 space
    int pipSize = 16;      // Fixed pip size
    int margin = 20;       // Fixed margin

    int left = margin;
    int centerX = 50;
    int right = 100 - margin;
    int top = margin;
    int centerY = 50;
    int bottom = 100 - margin;

    // Draw pips based on value (white)
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);

    switch(value) {
        case 1:
            drawPip(painter, centerX, centerY, pipSize);
            break;
        case 2:
            // Randomize orientation: 0=diagonal \, 1=diagonal /
            if (orientation % 2 == 0) {
                drawPip(painter, left, top, pipSize);
                drawPip(painter, right, bottom, pipSize);
            } else {
                drawPip(painter, right, top, pipSize);
                drawPip(painter, left, bottom, pipSize);
            }
            break;
        case 3:
            // Randomize orientation: 0=diagonal \, 1=diagonal /
            if (orientation % 2 == 0) {
                drawPip(painter, left, top, pipSize);
                drawPip(painter, centerX, centerY, pipSize);
                drawPip(painter, right, bottom, pipSize);
            } else {
                drawPip(painter, right, top, pipSize);
                drawPip(painter, centerX, centerY, pipSize);
                drawPip(painter, left, bottom, pipSize);
            }
            break;
        case 4:
            drawPip(painter, left, top, pipSize);
            drawPip(painter, right, top, pipSize);
            drawPip(painter, left, bottom, pipSize);
            drawPip(painter, right, bottom, pipSize);
            break;
        case 5:
            drawPip(painter, left, top, pipSize);
            drawPip(painter, right, top, pipSize);
            drawPip(painter, centerX, centerY, pipSize);
            drawPip(painter, left, bottom, pipSize);
            drawPip(painter, right, bottom, pipSize);
            break;
        case 6:
            drawPip(painter, left, top, pipSize);
            drawPip(painter, right, top, pipSize);
            drawPip(painter, left, centerY, pipSize);
            drawPip(painter, right, centerY, pipSize);
            drawPip(painter, left, bottom, pipSize);
            drawPip(painter, right, bottom, pipSize);
            break;
    }

    // Restore painter state
    painter.restore();
}

void LAURollingDieWidget::drawPip(QPainter &painter, int x, int y, int size)
{
    painter.drawEllipse(QPoint(x, y), size/2, size/2);
}

void LAURollingDieWidget::mousePressEvent(QMouseEvent *event)
{
    if (isRolling || !selectionEnabled) return;  // Ignore clicks while rolling or if selection disabled

    // Find which die was clicked
    int dieIndex = getDieIndexAtPosition(event->pos());
    if (dieIndex >= 0) {
        // Toggle selection
        diceSelected[dieIndex] = !diceSelected[dieIndex];
        update();
    }
}

int LAURollingDieWidget::getDieIndexAtPosition(const QPoint &pos)
{
    // Calculate die size and spacing (same as in paintEvent)
    int spacing = 20;
    int availableWidth = width() - (numDice + 1) * spacing;
    int availableHeight = height() - 2 * spacing;
    int dieSize = qMin(availableWidth / numDice, availableHeight);

    int totalDiceWidth = numDice * dieSize + (numDice - 1) * spacing;
    int startX = (width() - totalDiceWidth) / 2;
    int startY = (height() - dieSize) / 2;

    // Check which die was clicked
    for (int i = 0; i < numDice; ++i) {
        int x = startX + i * (dieSize + spacing);
        QRect dieRect(x, startY, dieSize, dieSize);
        if (dieRect.contains(pos)) {
            return i;
        }
    }

    return -1;  // No die was clicked
}

void LAURollingDieWidget::startRolling(QVector<int> indicesToRoll)
{
    isRolling = true;
    rollCount = 0;
    rollingIndices = indicesToRoll;
    rollTimer->start(20);  // Start fast (20ms)
}

void LAURollingDieWidget::onRollTimer()
{
    // Generate random values only for dice being rolled
    for (int index : rollingIndices) {
        if (index >= 0 && index < numDice) {
            diceValues[index] = QRandomGenerator::global()->bounded(1, 7);
            diceOrientations[index] = QRandomGenerator::global()->bounded(0, 4);
        }
    }

    // Play click sound (system beep)
    QApplication::beep();

    // Trigger repaint to show new values
    update();

    // Increment roll count
    rollCount++;

    // Calculate slowing down effect
    // Gradually increase timer interval to slow down
    if (rollCount > 10) {
        int newInterval = 20 + (rollCount - 10) * 15;
        rollTimer->setInterval(newInterval);
    }

    // Stop after reaching max rolls
    if (rollCount >= maxRolls) {
        rollTimer->stop();
        isRolling = false;
        emit rollComplete(diceValues);
    }
}

void LAURollingDieWidget::saveState()
{
    QSettings settings("LAU", "Yahtzee");

    // Save dice values
    settings.beginWriteArray("diceValues", numDice);
    for (int i = 0; i < numDice; ++i) {
        settings.setArrayIndex(i);
        settings.setValue("value", diceValues[i]);
        settings.setValue("selected", diceSelected[i]);
    }
    settings.endArray();
}

void LAURollingDieWidget::loadState()
{
    QSettings settings("LAU", "Yahtzee");

    // Load dice values
    int size = settings.beginReadArray("diceValues");
    for (int i = 0; i < qMin(size, numDice); ++i) {
        settings.setArrayIndex(i);
        diceValues[i] = settings.value("value", 1).toInt();
        diceSelected[i] = settings.value("selected", false).toBool();
    }
    settings.endArray();

    update();
}

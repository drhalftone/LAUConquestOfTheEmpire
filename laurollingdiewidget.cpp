/*********************************************************************************
 * Copyright (c) 2025, Dr. Daniel L. Lau - All rights reserved.                  *
 *********************************************************************************/

#include "laurollingdiewidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QApplication>

LAURollingDieWidget::LAURollingDieWidget(int numDice, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowStaysOnTopHint)
    , numDice(numDice), isRolling(false), rollCount(0), maxRolls(30), m_rollSender(nullptr)
{
    setWindowTitle(QString("Rolling Dice"));
    diceValues.resize(numDice);
    diceOrientations.resize(numDice);
    for (int i = 0; i < numDice; ++i) {
        diceValues[i] = QRandomGenerator::global()->bounded(1, 7);
        diceOrientations[i] = QRandomGenerator::global()->bounded(0, 4);
    }
    int dieSize = 200, spacing = 20;
    resize(numDice * dieSize + (numDice + 1) * spacing, dieSize + 2 * spacing);
    rollTimer = new QTimer(this);
    connect(rollTimer, &QTimer::timeout, this, &LAURollingDieWidget::onRollTimer);
}

LAURollingDieWidget::~LAURollingDieWidget() {}

int LAURollingDieWidget::getValue(int index) const {
    return (index >= 0 && index < diceValues.size()) ? diceValues[index] : 0;
}

void LAURollingDieWidget::startRoll() {
    m_rollSender = sender();  // Capture the object that emitted the signal
    if (!isVisible()) show();
    raise(); activateWindow();
    if (!isRolling) startRolling();
}

void LAURollingDieWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::black);
    int spacing = 20;
    int dieSize = qMin((width() - (numDice + 1) * spacing) / numDice, height() - 2 * spacing);
    int startX = (width() - (numDice * dieSize + (numDice - 1) * spacing)) / 2;
    for (int i = 0; i < numDice; ++i) {
        drawDieFace(painter, diceValues[i], diceOrientations[i], 
                    QRect(startX + i * (dieSize + spacing), (height() - dieSize) / 2, dieSize, dieSize));
    }
}

void LAURollingDieWidget::drawDieFace(QPainter &painter, int value, int orientation, const QRect &rect) {
    painter.save();
    qreal scale = rect.width() / 100.0;
    painter.translate(rect.topLeft());
    painter.scale(scale, scale);
    painter.setBrush(Qt::red);
    painter.setPen(QPen(Qt::darkRed, 2));
    painter.drawRoundedRect(QRect(0, 0, 100, 100), 5, 5);
    int pipSize = 16, margin = 20;
    int left = margin, centerX = 50, right = 80, top = margin, centerY = 50, bottom = 80;
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    switch(value) {
        case 1: drawPip(painter, centerX, centerY, pipSize); break;
        case 2: 
            if (orientation % 2 == 0) { drawPip(painter, left, top, pipSize); drawPip(painter, right, bottom, pipSize); }
            else { drawPip(painter, right, top, pipSize); drawPip(painter, left, bottom, pipSize); }
            break;
        case 3:
            if (orientation % 2 == 0) { drawPip(painter, left, top, pipSize); drawPip(painter, centerX, centerY, pipSize); drawPip(painter, right, bottom, pipSize); }
            else { drawPip(painter, right, top, pipSize); drawPip(painter, centerX, centerY, pipSize); drawPip(painter, left, bottom, pipSize); }
            break;
        case 4: drawPip(painter, left, top, pipSize); drawPip(painter, right, top, pipSize); drawPip(painter, left, bottom, pipSize); drawPip(painter, right, bottom, pipSize); break;
        case 5: drawPip(painter, left, top, pipSize); drawPip(painter, right, top, pipSize); drawPip(painter, centerX, centerY, pipSize); drawPip(painter, left, bottom, pipSize); drawPip(painter, right, bottom, pipSize); break;
        case 6: drawPip(painter, left, top, pipSize); drawPip(painter, right, top, pipSize); drawPip(painter, left, centerY, pipSize); drawPip(painter, right, centerY, pipSize); drawPip(painter, left, bottom, pipSize); drawPip(painter, right, bottom, pipSize); break;
    }
    painter.restore();
}

void LAURollingDieWidget::drawPip(QPainter &painter, int x, int y, int size) {
    painter.drawEllipse(QPoint(x, y), size/2, size/2);
}

void LAURollingDieWidget::mousePressEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    if (isRolling) rollCount = maxRolls - 1;
}

void LAURollingDieWidget::startRolling() {
    isRolling = true; rollCount = 0; rollTimer->start(20);
}

void LAURollingDieWidget::onRollTimer() {
    for (int i = 0; i < numDice; ++i) {
        diceValues[i] = QRandomGenerator::global()->bounded(1, 7);
        diceOrientations[i] = QRandomGenerator::global()->bounded(0, 4);
    }
    QApplication::beep(); update(); rollCount++;
    if (rollCount > 10) rollTimer->setInterval(20 + (rollCount - 10) * 15);
    if (rollCount >= maxRolls) {
        rollTimer->stop(); isRolling = false;
        emit rollComplete(diceValues[0], m_rollSender);
        m_rollSender = nullptr;
    }
}

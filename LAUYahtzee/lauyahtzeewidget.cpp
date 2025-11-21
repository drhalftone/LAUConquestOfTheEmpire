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

#include "lauyahtzeewidget.h"
#include <QSettings>

LAUYahtzeeWidget::LAUYahtzeeWidget(QWidget *parent)
    : QWidget(parent), rollsRemaining(3)
{
    setWindowTitle(QString("LAU Yahtzee - Dice"));

    // Create main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Create menu bar
    menuBar = new QMenuBar(this);

    // Window menu
    QMenu *windowMenu = menuBar->addMenu("Window");
    QAction *showScoreSheetAction = windowMenu->addAction(
        QIcon::fromTheme("window-new", QIcon(":/icons/window")),
        "Show Score Sheet");
    connect(showScoreSheetAction, &QAction::triggered, this, &LAUYahtzeeWidget::onShowScoreSheet);

    windowMenu->addSeparator();

    QAction *resetScoreSheetAction = windowMenu->addAction(
        QIcon::fromTheme("edit-clear", QIcon(":/icons/reset")),
        "Reset Score Sheet");
    connect(resetScoreSheetAction, &QAction::triggered, this, &LAUYahtzeeWidget::onResetScoreSheet);

    // Help menu
    QMenu *helpMenu = menuBar->addMenu("Help");
    QAction *aboutAction = helpMenu->addAction(
        QIcon::fromTheme("help-about", QIcon(":/icons/help")),
        "About LAU Yahtzee");
    connect(aboutAction, &QAction::triggered, this, &LAUYahtzeeWidget::onAbout);

    mainLayout->setMenuBar(menuBar);

    // Create layout for dice and roll button
    QVBoxLayout *contentLayout = new QVBoxLayout();

    // Create dice widget with 5 dice
    diceWidget = new LAURollingDieWidget(5, this);
    contentLayout->addWidget(diceWidget);

    // Create roll button
    rollButton = new QPushButton("Roll All Dice (3 rolls left)", this);
    rollButton->setMinimumHeight(50);
    contentLayout->addWidget(rollButton);

    mainLayout->addLayout(contentLayout);

    // Create score sheet as separate window
    scoreSheet = new LAUScoreSheetWidget(nullptr);  // No parent = separate window
    scoreSheet->setWindowTitle("LAU Yahtzee - Score Sheet");
    scoreSheet->show();

    // Connect signals
    connect(rollButton, &QPushButton::clicked, this, &LAUYahtzeeWidget::onRollButtonClicked);
    connect(diceWidget, &LAURollingDieWidget::rollComplete, this, &LAUYahtzeeWidget::onRollComplete);
    connect(scoreSheet, &LAUScoreSheetWidget::categoryScored, this, &LAUYahtzeeWidget::onCategoryScored);

    resize(720, 300);

    // Load saved state
    loadState();
}

LAUYahtzeeWidget::~LAUYahtzeeWidget()
{
    // Delete score sheet window when main window closes
    if (scoreSheet) {
        delete scoreSheet;
    }
}

void LAUYahtzeeWidget::onRollButtonClicked()
{
    if (diceWidget->rolling()) {
        return;  // Already rolling
    }

    if (rollsRemaining == 3) {
        // First roll - roll all dice, disable selection
        diceWidget->setSelectionEnabled(false);
        diceWidget->deselectAll();
        diceWidget->roll();  // Roll all
        rollsRemaining--;
        rollButton->setEnabled(false);  // Disable while rolling
    } else if (rollsRemaining > 0) {
        // Subsequent rolls - roll UNSELECTED dice (selected = kept/preserved)
        QVector<int> unselectedIndices;
        for (int i = 0; i < diceWidget->getNumDice(); ++i) {
            if (!diceWidget->isSelected(i)) {  // Roll the ones NOT selected
                unselectedIndices.append(i);
            }
        }

        if (unselectedIndices.isEmpty()) {
            // All dice are selected (kept) - player is done, go to end-of-turn state
            rollsRemaining = 0;
            diceWidget->setSelectionEnabled(false);
            rollButton->setEnabled(false);  // Disable until score is entered
            rollButton->setText("Enter a score to continue");
        } else {
            // Roll the unselected dice
            diceWidget->roll(unselectedIndices);
            rollsRemaining--;
            rollButton->setEnabled(false);  // Disable while rolling
        }
    }
    // If rollsRemaining == 0, button should be disabled anyway
}

void LAUYahtzeeWidget::onRollComplete(QVector<int> values)
{
    // Update score sheet with potential scores
    scoreSheet->updatePotentialScores(values);

    // Enable selection after first roll
    if (rollsRemaining < 3) {
        diceWidget->setSelectionEnabled(true);
    }

    // Re-enable button after roll completes
    if (rollsRemaining > 0) {
        rollButton->setEnabled(true);
        rollButton->setText(QString("Roll Unselected Dice (%1 roll%2 left)")
                               .arg(rollsRemaining)
                               .arg(rollsRemaining == 1 ? "" : "s"));
    } else {
        // Out of rolls - select all dice and disable button until score is entered
        diceWidget->selectAll();
        diceWidget->setSelectionEnabled(false);
        rollButton->setEnabled(false);
        rollButton->setText("Enter a score to continue");
    }
}

void LAUYahtzeeWidget::onCategoryScored(ScoreCategory category, int score)
{
    Q_UNUSED(category);
    Q_UNUSED(score);

    // After scoring, reset for new turn
    rollsRemaining = 3;
    diceWidget->setSelectionEnabled(false);
    diceWidget->deselectAll();
    rollButton->setEnabled(true);
    rollButton->setText("Roll All Dice (3 rolls left)");

    // Save state after scoring
    saveState();
}

void LAUYahtzeeWidget::closeEvent(QCloseEvent *event)
{
    // Save state before closing
    saveState();

    // Quit the entire application when the dice window is closed
    QApplication::quit();
    event->accept();
}

void LAUYahtzeeWidget::onShowScoreSheet()
{
    // Always show the score sheet, don't toggle
    scoreSheet->show();
    scoreSheet->raise();
    scoreSheet->activateWindow();
}

void LAUYahtzeeWidget::onResetScoreSheet()
{
    // Ask for confirmation
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Reset Score Sheet",
        "Are you sure you want to reset the score sheet? This will clear all scores and start a new game.",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        // Reset score sheet
        scoreSheet->reset();

        // Reset dice widget
        rollsRemaining = 3;
        diceWidget->setSelectionEnabled(false);
        diceWidget->deselectAll();
        rollButton->setEnabled(true);
        rollButton->setText("Roll All Dice (3 rolls left)");

        // Save the reset state
        saveState();
    }
}

void LAUYahtzeeWidget::onAbout()
{
    QMessageBox::about(this, "About LAU Yahtzee",
        "<h3>LAU Yahtzee</h3>"
        "<p>A dice rolling game based on the classic Yahtzee rules.</p>"
        "<p>Copyright (c) 2025, Dr. Daniel L. Lau<br>"
        "All rights reserved.</p>"
        "<p><b>How to Play:</b></p>"
        "<ul>"
        "<li>Roll the dice up to 3 times per turn</li>"
        "<li>Click dice to select/keep them (green = kept)</li>"
        "<li>Unselected (red) dice will be re-rolled</li>"
        "<li>After rolling, click a category on the score sheet to score</li>"
        "</ul>");
}

void LAUYahtzeeWidget::saveState()
{
    QSettings settings("LAU", "Yahtzee");

    // Save game state
    settings.setValue("rollsRemaining", rollsRemaining);

    // Save dice and score sheet state
    diceWidget->saveState();
    scoreSheet->saveState();
}

void LAUYahtzeeWidget::loadState()
{
    QSettings settings("LAU", "Yahtzee");

    // Load game state
    rollsRemaining = settings.value("rollsRemaining", 3).toInt();

    // Load dice and score sheet state
    diceWidget->loadState();
    scoreSheet->loadState();

    // Update UI based on loaded state
    if (rollsRemaining == 3) {
        rollButton->setText("Roll All Dice (3 rolls left)");
        rollButton->setEnabled(true);
        diceWidget->setSelectionEnabled(false);
    } else if (rollsRemaining > 0) {
        rollButton->setText(QString("Roll Unselected Dice (%1 roll%2 left)")
                               .arg(rollsRemaining)
                               .arg(rollsRemaining == 1 ? "" : "s"));
        rollButton->setEnabled(true);
        diceWidget->setSelectionEnabled(true);
    } else {
        rollButton->setText("Enter a score to continue");
        rollButton->setEnabled(false);
        diceWidget->setSelectionEnabled(false);
    }
}

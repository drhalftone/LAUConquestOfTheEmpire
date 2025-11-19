#include "mainwindow.h"
#include "mapwidget.h"
#include "scorewindow.h"
#include "walletwindow.h"
#include "purchasedialog.h"
#include "placementdialog.h"
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_mapWidget(nullptr)
    , m_scoreWindow(nullptr)
    , m_walletWindow(nullptr)
{
    setWindowTitle("Conquest of the Empire");
    resize(800, 600);

    m_mapWidget = new MapWidget(this);
    setCentralWidget(m_mapWidget);

    // Create score window
    m_scoreWindow = new ScoreWindow(nullptr);
    m_scoreWindow->show();

    // Create wallet window
    m_walletWindow = new WalletWindow(nullptr);
    m_walletWindow->move(m_scoreWindow->x(), m_scoreWindow->y() + m_scoreWindow->height() + 30);
    m_walletWindow->show();

    // Connect score updates
    connect(m_mapWidget, &MapWidget::scoresChanged, this, [this]() {
        m_scoreWindow->updateScores(m_mapWidget->calculateScores());
    });

    // Connect tax collection (when turn ends)
    connect(m_mapWidget, &MapWidget::taxesCollected, this, [this](QChar player, int amount) {
        m_walletWindow->addToWallet(player, amount);
    });

    // Connect purchase phase
    connect(m_mapWidget, &MapWidget::purchasePhaseNeeded, this, [this](QChar player, int availableMoney, int inflationMultiplier) {
        // Note: This old purchase flow is no longer used - PlayerInfoWidget handles purchases now
        // Setting default limits for backward compatibility
        PurchaseDialog *dialog = new PurchaseDialog(player, availableMoney, inflationMultiplier, 10, 10, this);

        if (dialog->exec() == QDialog::Accepted) {
            int amountSpent = dialog->getTotalSpent();

            // Deduct from wallet window
            m_walletWindow->addToWallet(player, -amountSpent);

            // Turn management now handled by PlayerInfoWidget, not MapWidget
            // m_mapWidget->handlePurchaseComplete(amountSpent);
        }

        delete dialog;
    });

    // Initialize scores with starting values
    m_scoreWindow->updateScores(m_mapWidget->calculateScores());

    // TEST MODE: Open purchase dialog immediately with 100 credits
    QTimer::singleShot(500, this, [this]() {
        PurchaseDialog *testDialog = new PurchaseDialog('A', 100, 1, 5, 5, this);

        if (testDialog->exec() == QDialog::Accepted) {
            // Show placement dialog with purchased items
            PlacementDialog *placementDialog = new PlacementDialog(
                'A',
                testDialog->getInfantryCount(),
                testDialog->getCavalryCount(),
                testDialog->getCatapultCount(),
                testDialog->getGalleyCount(),
                testDialog->getCityCount(),
                testDialog->getFortificationCount(),
                testDialog->getRoadCount(),
                this
            );

            // Connect map widget's itemPlaced signal to placement dialog's decrement slot
            connect(m_mapWidget, &MapWidget::itemPlaced, placementDialog, &PlacementDialog::decrementItemCount);

            placementDialog->show();
        }

        delete testDialog;
    });
}

MainWindow::~MainWindow()
{
    if (m_scoreWindow) {
        delete m_scoreWindow;
    }
    if (m_walletWindow) {
        delete m_walletWindow;
    }
}

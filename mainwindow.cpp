#include "mainwindow.h"
#include "mapwidget.h"
#include "scorewindow.h"
#include "walletwindow.h"
#include "purchasedialog.h"
#include "placementdialog.h"
#include <QTimer>
#include <QDebug>

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
    // NOTE: Purchase phase is now handled by PlayerInfoWidget, not here
    // This old handler is kept for backward compatibility but is not used
    connect(m_mapWidget, &MapWidget::purchasePhaseNeeded, this, [this](QChar player, int availableMoney, int inflationMultiplier) {
        // Old purchase flow - disabled, PlayerInfoWidget handles purchases now
        qDebug() << "purchasePhaseNeeded signal received, but PlayerInfoWidget handles purchases now";
    });

    // Initialize scores with starting values
    m_scoreWindow->updateScores(m_mapWidget->calculateScores());

    // TEST MODE: DISABLED - Purchase dialog is now handled by PlayerInfoWidget
    // The new purchase flow is integrated into the turn-based gameplay
    /*
    QTimer::singleShot(500, this, [this]() {
        // Old test code removed - use PlayerInfoWidget for purchases
    });
    */
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

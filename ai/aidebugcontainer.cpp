#include "aidebugcontainer.h"
#include "player.h"
#include <QSettings>
#include <QCloseEvent>

AIDebugContainer::AIDebugContainer(QWidget *parent)
    : QDialog(parent)
    , m_tabWidget(nullptr)
{
    setupUI();
    loadSettings();
}

AIDebugContainer::~AIDebugContainer()
{
    saveSettings();
}

void AIDebugContainer::setupUI()
{
    setWindowTitle("AI Debug - All Players");
    setMinimumSize(700, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabPosition(QTabWidget::North);
    m_tabWidget->setMovable(true);  // Allow reordering tabs

    mainLayout->addWidget(m_tabWidget);

    setLayout(mainLayout);
}

void AIDebugContainer::addAIPlayer(AIPlayer *ai, Player *player)
{
    if (!ai || !player) return;

    QChar playerId = player->getId();

    // Check if already added
    if (m_debugWidgets.contains(playerId)) {
        return;
    }

    // Create debug widget for this AI
    AIDebugWidget *debugWidget = new AIDebugWidget(this);
    debugWidget->setAIPlayer(ai);

    // Create tab label with player info
    QString tabLabel = QString("Player %1").arg(playerId);
    QString homeName = player->getHomeProvinceName();
    if (!homeName.isEmpty()) {
        tabLabel = QString("P%1 - %2").arg(playerId).arg(homeName);
    }

    // Add to tab widget
    int tabIndex = m_tabWidget->addTab(debugWidget, tabLabel);

    // Set tab tooltip with more details
    m_tabWidget->setTabToolTip(tabIndex, QString("Player %1 (%2)")
        .arg(playerId)
        .arg(homeName));

    // Store references
    m_debugWidgets[playerId] = debugWidget;
    m_playerTabIndex[playerId] = tabIndex;

    // Update window title
    setWindowTitle(QString("AI Debug - %1 Players").arg(m_tabWidget->count()));
}

void AIDebugContainer::removeAIPlayer(QChar playerId)
{
    if (!m_debugWidgets.contains(playerId)) {
        return;
    }

    AIDebugWidget *widget = m_debugWidgets[playerId];
    int tabIndex = m_tabWidget->indexOf(widget);

    if (tabIndex >= 0) {
        m_tabWidget->removeTab(tabIndex);
    }

    m_debugWidgets.remove(playerId);
    m_playerTabIndex.remove(playerId);

    // Update remaining tab indices
    m_playerTabIndex.clear();
    for (auto it = m_debugWidgets.begin(); it != m_debugWidgets.end(); ++it) {
        int idx = m_tabWidget->indexOf(it.value());
        if (idx >= 0) {
            m_playerTabIndex[it.key()] = idx;
        }
    }

    // Update window title
    if (m_tabWidget->count() > 0) {
        setWindowTitle(QString("AI Debug - %1 Players").arg(m_tabWidget->count()));
    } else {
        setWindowTitle("AI Debug - No Players");
    }

    delete widget;
}

AIDebugWidget* AIDebugContainer::getDebugWidget(QChar playerId) const
{
    return m_debugWidgets.value(playerId, nullptr);
}

void AIDebugContainer::clear()
{
    // Remove all tabs
    while (m_tabWidget->count() > 0) {
        QWidget *widget = m_tabWidget->widget(0);
        m_tabWidget->removeTab(0);
        delete widget;
    }

    m_debugWidgets.clear();
    m_playerTabIndex.clear();

    setWindowTitle("AI Debug - No Players");
}

void AIDebugContainer::showPlayer(QChar playerId)
{
    if (m_playerTabIndex.contains(playerId)) {
        m_tabWidget->setCurrentIndex(m_playerTabIndex[playerId]);
    }
}

void AIDebugContainer::closeEvent(QCloseEvent *event)
{
    saveSettings();
    // Hide instead of close, so we can show again later
    hide();
    event->ignore();
}

void AIDebugContainer::saveSettings()
{
    QSettings settings("LAU", "ConquestOfTheEmpire");
    settings.beginGroup("AIDebugContainer");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("currentTab", m_tabWidget->currentIndex());
    settings.endGroup();
}

void AIDebugContainer::loadSettings()
{
    QSettings settings("LAU", "ConquestOfTheEmpire");
    settings.beginGroup("AIDebugContainer");

    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    } else {
        // Default position and size
        resize(750, 700);
        move(100, 100);
    }

    // Note: currentTab will be restored after tabs are added
    settings.endGroup();
}

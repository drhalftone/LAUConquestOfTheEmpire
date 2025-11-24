// Stub file for QuickBattle mini-game
// Provides minimal implementations for symbols referenced but not used

#include "playerinfowidget.h"
#include <QCloseEvent>

// Constructor/Destructor stubs
PlayerInfoWidget::PlayerInfoWidget(QWidget *parent) : QWidget(parent) {}
PlayerInfoWidget::~PlayerInfoWidget() {}

// Event handler stubs
void PlayerInfoWidget::closeEvent(QCloseEvent *event) { event->accept(); }
void PlayerInfoWidget::handleTerritoryRightClick(const QString &, const QPoint &, QChar) {}
void PlayerInfoWidget::onEndTurnClicked() {}

// AI mode stubs - not used for combat-only testing
void PlayerInfoWidget::setAIAutoMode(bool, int) {}
bool PlayerInfoWidget::aiMoveLeaderToTerritory(GamePiece *, const QString &) { return false; }
void PlayerInfoWidget::endTurn() {}

// State query stubs - return empty/zero values
int PlayerInfoWidget::getDisplayedWallet(QChar) const { return 0; }
int PlayerInfoWidget::getDisplayedTerritoryCount(QChar) const { return 0; }
QStringList PlayerInfoWidget::getDisplayedTerritories(QChar) const { return QStringList(); }
int PlayerInfoWidget::getDisplayedPieceCount(QChar) const { return 0; }
QList<PlayerInfoWidget::DisplayedLeaderInfo> PlayerInfoWidget::getDisplayedLeaders(QChar) const { return {}; }
QList<PlayerInfoWidget::MoveOption> PlayerInfoWidget::getMovesForLeader(GamePiece *) const { return {}; }

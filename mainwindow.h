#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MapWidget;
class ScoreWindow;
class WalletWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    MapWidget *m_mapWidget;
    ScoreWindow *m_scoreWindow;
    WalletWindow *m_walletWindow;
};

#endif // MAINWINDOW_H

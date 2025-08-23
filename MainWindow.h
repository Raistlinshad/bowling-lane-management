#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QDateTime>
#include <QKeyEvent>
#include "LaneServer.h"
#include "EventBus.h"
#include "Actions.h"
#include "LaneWidget.h"
#include "BowlerManagementDialog.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    LaneServer* getLaneServer() const { return m_laneServer; }
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateTime();
    void onQuickAccessButtonClicked(const QString &buttonText);
    void onLaneStatusChanged(int laneId, LaneStatus status);
    void onGameDataReceived(int laneId, const QJsonObject &gameData);
    void onBowlerManagementClicked();

private:
    void setupUI();
    void setupTopBar();
    void setupQuickAccessButtons();
    void setupLaneWidgets();
    void setupMainContent();
    
    // UI Components
    QWidget *m_centralWidget;
    QVBoxLayout *m_mainLayout;
    QFrame *m_topBar;
    QLabel *m_logoLabel;
    QLabel *m_dateTimeLabel;
    QLabel *m_statusBar;
    QFrame *m_quickAccessFrame;
    QScrollArea *m_laneScrollArea;
    QWidget *m_laneContainer;
    QGridLayout *m_laneLayout;
    QWidget *m_mainContentArea;
    QPushButton *m_bowlerManagementBtn;
    
    // Timer for updates
    QTimer *m_timeUpdateTimer;
    
    // Core components
    LaneServer *m_laneServer;
    EventBus *m_eventBus;
    Actions *m_actions;
    
    // Lane widgets
    QVector<LaneWidget*> m_laneWidgets;
    int m_totalLanes;
    
    QString getCurrentTime() const;
};

#endif // MAINWINDOW_H
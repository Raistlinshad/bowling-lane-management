#include "MainWindow.h"
#include "BowlerManagementDialog.h"
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QSplitter>
#include <QGroupBox>
#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_timeUpdateTimer(new QTimer(this))
    , m_laneServer(nullptr)
    , m_eventBus(new EventBus(this))
    , m_actions(nullptr)
    , m_totalLanes(8) // Default 8 lanes, should be configurable
{
    setupUI();
    
    // Initialize core components
    m_laneServer = new LaneServer(m_eventBus, this);
    m_actions = new Actions(this, m_eventBus, this);
    
    // Connect signals
    connect(m_timeUpdateTimer, &QTimer::timeout, this, &MainWindow::updateTime);
    connect(m_laneServer, &LaneServer::laneStatusChanged, 
            this, &MainWindow::onLaneStatusChanged);
    connect(m_laneServer, &LaneServer::gameDataReceived,
            this, &MainWindow::onGameDataReceived);

    connect(m_actions, &Actions::sendGameToLane, 
        this, [this](int laneId, const QJsonObject &gameData) {
            if (m_laneServer) {
                m_laneServer->sendToLane(laneId, "quick_game", gameData);
            }
        });
    
    // Start timer
    m_timeUpdateTimer->start(1000); // Update every second
    
    // Start lane server
    m_laneServer->start();
    
    setWindowTitle("Centre Bowling Management System");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    if (m_laneServer) {
        m_laneServer->stop();
    }
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    
    setupTopBar();
    setupQuickAccessButtons();
    setupMainContent();
    setupLaneWidgets();
}

void MainWindow::setupTopBar()
{
    m_topBar = new QFrame;
    m_topBar->setFrameStyle(QFrame::StyledPanel);
    m_topBar->setFixedHeight(60);
    m_topBar->setStyleSheet("QFrame { background-color: #1a1a1a; border-bottom: 2px solid #333; }");
    
    QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
    
    // Logo
    m_logoLabel = new QLabel("🎳 CENTRE BOWLING");
    m_logoLabel->setStyleSheet("QLabel { color: #4A90E2; font-size: 18px; font-weight: bold; }");
    
    // Date/Time
    m_dateTimeLabel = new QLabel(getCurrentTime());
    m_dateTimeLabel->setStyleSheet("QLabel { color: #4A90E2; font-size: 14px; }");
    
    topLayout->addWidget(m_logoLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_dateTimeLabel);
    
    m_mainLayout->addWidget(m_topBar);
}

void MainWindow::setupQuickAccessButtons()
{
    // Status bar for quick access shortcuts - Updated to include F4 for Bowler Management
    m_statusBar = new QLabel("F1: Quick Start   F2: League Start   F3: New Bowler   F4: Bowler Management   Esc: Main Menu");
    m_statusBar->setStyleSheet("QLabel { background-color: #2a2a2a; color: white; padding: 5px; }");
    m_statusBar->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_statusBar);
}

void MainWindow::setupMainContent()
{
    // Create splitter for main content
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal);
    
    // Left side - Quick access buttons
    m_quickAccessFrame = new QFrame;
    m_quickAccessFrame->setFixedWidth(200);
    m_quickAccessFrame->setStyleSheet("QFrame { background-color: #2a2a2a; border-right: 2px solid #333; }");
    
    QVBoxLayout *quickLayout = new QVBoxLayout(m_quickAccessFrame);
    quickLayout->setSpacing(5);
    quickLayout->setContentsMargins(10, 10, 10, 10);
    
    // Updated button list - FIXED: "Bowler Info" changed to "View Bowler" and "Bowler Management" stays as full management
    QStringList quickButtons = {"Quick Start", "Bowler Management", "Calendar", "Teams", 
                               "Leagues", "Party", "Status", "Settings", "Test Lane", "QG Diagnostic", "DB Browser"};
    
    for (const QString &buttonText : quickButtons) {
        QPushButton *button = new QPushButton(buttonText);
        
        // Special styling for Bowler Management button to make it stand out
        if (buttonText == "Bowler Management") {
            button->setStyleSheet("QPushButton { "
                                 "background-color: #4A90E2; "
                                 "color: white; "
                                 "border: 1px solid #357ABD; "
                                 "padding: 8px; "
                                 "text-align: left; "
                                 "font-weight: bold; "
                                 "} "
                                 "QPushButton:hover { background-color: #357ABD; } "
                                 "QPushButton:pressed { background-color: #2E5A87; }");
        } else {
            button->setStyleSheet("QPushButton { "
                                 "background-color: #333; "
                                 "color: white; "
                                 "border: 1px solid #555; "
                                 "padding: 8px; "
                                 "text-align: left; "
                                 "} "
                                 "QPushButton:hover { background-color: #4A90E2; } "
                                 "QPushButton:pressed { background-color: #357ABD; }");
        }
        
        connect(button, &QPushButton::clicked, [this, buttonText]() {
            onQuickAccessButtonClicked(buttonText);
        });
        
        quickLayout->addWidget(button);
    }
    quickLayout->addStretch();
    
    // Right side - Main content area
    m_mainContentArea = new QWidget;
    m_mainContentArea->setStyleSheet("QWidget { background-color: #1a1a1a; }");
    
    mainSplitter->addWidget(m_quickAccessFrame);
    mainSplitter->addWidget(m_mainContentArea);
    mainSplitter->setStretchFactor(1, 1);
    
    m_mainLayout->addWidget(mainSplitter, 1);
}

void MainWindow::setupLaneWidgets()
{
    // Create lane widgets container at bottom
    QFrame *laneFrame = new QFrame;
    laneFrame->setFixedHeight(120);
    laneFrame->setStyleSheet("QFrame { background-color: #2a2a2a; border-top: 2px solid #333; }");
    
    m_laneScrollArea = new QScrollArea;
    m_laneScrollArea->setWidgetResizable(true);
    m_laneScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_laneScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_laneScrollArea->setStyleSheet("QScrollArea { border: none; background-color: #2a2a2a; }");
    
    m_laneContainer = new QWidget;
    m_laneLayout = new QGridLayout(m_laneContainer);
    m_laneLayout->setSpacing(5);
    m_laneLayout->setContentsMargins(10, 10, 10, 10);
    
    // Create lane widgets
    int columns = 12; // 12 lanes per row
    for (int i = 1; i <= m_totalLanes; ++i) {
        LaneWidget *laneWidget = new LaneWidget(i);
        
        // Connect drag and drop signals
        connect(laneWidget, &LaneWidget::teamMoveRequested,
                this, [this](int fromLane, int toLane, const QString &teamData) {
                    // Handle team movement between lanes
                    if (m_laneServer) {
                        m_laneServer->handleTeamMove(fromLane, toLane, teamData);
                    }
                });
        
        int row = (i - 1) / columns;
        int col = (i - 1) % columns;
        m_laneLayout->addWidget(laneWidget, row, col);
        
        m_laneWidgets.append(laneWidget);
    }
    
    m_laneScrollArea->setWidget(m_laneContainer);
    
    QVBoxLayout *laneFrameLayout = new QVBoxLayout(laneFrame);
    laneFrameLayout->setContentsMargins(0, 0, 0, 0);
    laneFrameLayout->addWidget(m_laneScrollArea);
    
    m_mainLayout->addWidget(laneFrame);
}

QString MainWindow::getCurrentTime() const
{
    return QDateTime::currentDateTime().toString("ddd, MMM dd yyyy hh:mm:ss");
}

void MainWindow::updateTime()
{
    m_dateTimeLabel->setText(getCurrentTime());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_F1:
        m_actions->quickStart();
        break;
    case Qt::Key_F2:
        m_actions->leagueStart();
        break;
    case Qt::Key_F3:
        m_actions->newBowler();
        break;
    case Qt::Key_F4:
        onBowlerManagementClicked();
        break;
    case Qt::Key_Escape:
        m_actions->homeScreen();
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::onQuickAccessButtonClicked(const QString &buttonText)
{
    if (buttonText == "Quick Start") {
        m_actions->quickStart();
    } else if (buttonText == "Bowler Management") {
        onBowlerManagementClicked();
    } else if (buttonText == "Calendar") {
        m_actions->calendar();
    } else if (buttonText == "Teams") {
        m_actions->teams();
    } else if (buttonText == "Leagues") {
        m_actions->leagues();
    } else if (buttonText == "Party") {
        m_actions->party();
    } else if (buttonText == "Status") {
        m_actions->showStatus();
    } else if (buttonText == "Settings") {
        m_actions->settings();
    } else if (buttonText == "Test Lane") {
        m_actions->testLaneConnection();
    } else if (buttonText == "QG Diagnostic") {
        m_actions->runQuickGameDiagnostic();
    } else if (buttonText == "DB Browser") {
        m_actions->onDatabaseBrowserClicked();
    }
}

void MainWindow::onBowlerManagementClicked()
{
    BowlerManagementDialog dialog(this, this);
    dialog.exec();
}

void MainWindow::onLaneStatusChanged(int laneId, LaneStatus status)
{
    if (laneId > 0 && laneId <= m_laneWidgets.size()) {
        m_laneWidgets[laneId - 1]->setStatus(status);
    }
}

void MainWindow::onGameDataReceived(int laneId, const QJsonObject &gameData)
{
    if (laneId > 0 && laneId <= m_laneWidgets.size()) {
        m_laneWidgets[laneId - 1]->updateGameData(gameData);
    }
}
#include "QuickStartDialog.h"
#include "MainWindow.h"
#include "QuickGameDialog.h"
#include <QMessageBox>
#include <QApplication>
#include <QScreen>

QuickStartDialog::QuickStartDialog(MainWindow *mainWindow, QWidget *parent)
    : QDialog(parent)
    , m_mainWindow(mainWindow)
    , m_mainLayout(nullptr)
    , m_buttonGrid1(nullptr)
    , m_buttonGrid2(nullptr)
    , m_buttonGrid3(nullptr)
{
    setupUI();
    setWindowTitle("Quick Start");
    setModal(true);
    
    // Set size and center on screen
    resize(800, 600);
    
    // Center the dialog
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
}

void QuickStartDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(20);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Title
    QLabel *titleLabel = new QLabel("Quick Start Menu");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("QLabel { "
                             "font-size: 24px; "
                             "font-weight: bold; "
                             "color: #4A90E2; "
                             "margin-bottom: 20px; "
                             "}");
    m_mainLayout->addWidget(titleLabel);
    
    // First row of buttons (main game types)
    QFrame *frame1 = new QFrame;
    frame1->setFrameStyle(QFrame::StyledPanel);
    frame1->setStyleSheet("QFrame { background-color: #2a2a2a; border-radius: 10px; padding: 10px; }");
    m_buttonGrid1 = new QGridLayout(frame1);
    m_buttonGrid1->setSpacing(15);
    
    QPushButton *bingoBtn = createImageButton("BINGO", "🎯");
    QPushButton *quickGameBtn = createImageButton("Quick Game", "🎳");
    QPushButton *practiceBtn = createImageButton("Practice", "🏃");
    QPushButton *partyBtn = createImageButton("Party", "🎉");
    
    connect(bingoBtn, &QPushButton::clicked, this, &QuickStartDialog::onBingoClicked);
    connect(quickGameBtn, &QPushButton::clicked, this, &QuickStartDialog::onQuickGameClicked);
    connect(practiceBtn, &QPushButton::clicked, this, &QuickStartDialog::onPracticeClicked);
    connect(partyBtn, &QPushButton::clicked, this, &QuickStartDialog::onPartyClicked);
    
    m_buttonGrid1->addWidget(bingoBtn, 0, 0);
    m_buttonGrid1->addWidget(quickGameBtn, 0, 1);
    m_buttonGrid1->addWidget(practiceBtn, 0, 2);
    m_buttonGrid1->addWidget(partyBtn, 0, 3);
    
    m_mainLayout->addWidget(frame1);
    
    // Second row of buttons (special games)
    QFrame *frame2 = new QFrame;
    frame2->setFrameStyle(QFrame::StyledPanel);
    frame2->setStyleSheet("QFrame { background-color: #2a2a2a; border-radius: 10px; padding: 10px; }");
    m_buttonGrid2 = new QGridLayout(frame2);
    m_buttonGrid2->setSpacing(15);
    
    QPushButton *horseBtn = createImageButton("HORSE", "🐎");
    QPushButton *golfBtn = createImageButton("Golf", "⛳");
    QPushButton *baseballBtn = createImageButton("Baseball", "⚾");
    
    connect(horseBtn, &QPushButton::clicked, this, &QuickStartDialog::onHorseClicked);
    connect(golfBtn, &QPushButton::clicked, this, &QuickStartDialog::onGolfClicked);
    connect(baseballBtn, &QPushButton::clicked, this, &QuickStartDialog::onBaseballClicked);
    
    m_buttonGrid2->addWidget(horseBtn, 0, 0);
    m_buttonGrid2->addWidget(golfBtn, 0, 1);
    m_buttonGrid2->addWidget(baseballBtn, 0, 2);
    
    m_mainLayout->addWidget(frame2);
    
    // Third row of buttons (admin functions)
    QFrame *frame3 = new QFrame;
    frame3->setFrameStyle(QFrame::StyledPanel);
    frame3->setStyleSheet("QFrame { background-color: #2a2a2a; border-radius: 10px; padding: 10px; }");
    m_buttonGrid3 = new QGridLayout(frame3);
    m_buttonGrid3->setSpacing(15);
    
    QPushButton *settingsBtn = createImageButton("Settings", "⚙️");
    QPushButton *setupBtn = createImageButton("Setup", "🔧");
    QPushButton *endDayBtn = createImageButton("End Day", "🌅");
    QPushButton *newGameBtn = createImageButton("New Game Type", "🆕");
    
    connect(settingsBtn, &QPushButton::clicked, this, &QuickStartDialog::onSettingsClicked);
    connect(setupBtn, &QPushButton::clicked, this, &QuickStartDialog::onSetupClicked);
    connect(endDayBtn, &QPushButton::clicked, this, &QuickStartDialog::onEndDayClicked);
    connect(newGameBtn, &QPushButton::clicked, this, &QuickStartDialog::onNewGameTypeClicked);
    
    m_buttonGrid3->addWidget(settingsBtn, 0, 0);
    m_buttonGrid3->addWidget(setupBtn, 0, 1);
    m_buttonGrid3->addWidget(endDayBtn, 0, 2);
    m_buttonGrid3->addWidget(newGameBtn, 0, 3);
    
    m_mainLayout->addWidget(frame3);
    
    // Close button
    QPushButton *closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet("QPushButton { "
                           "background-color: #666; "
                           "color: white; "
                           "border: none; "
                           "padding: 10px 20px; "
                           "border-radius: 5px; "
                           "font-size: 14px; "
                           "} "
                           "QPushButton:hover { background-color: #777; } "
                           "QPushButton:pressed { background-color: #555; }");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);
    buttonLayout->addStretch();
    
    m_mainLayout->addLayout(buttonLayout);
}

QPushButton* QuickStartDialog::createImageButton(const QString &text, const QString &iconPath)
{
    QPushButton *button = new QPushButton;
    
    // Set button text with icon
    QString buttonText = iconPath.isEmpty() ? text : iconPath + "\n" + text;
    button->setText(buttonText);
    
    button->setFixedSize(150, 100);
    button->setStyleSheet("QPushButton { "
                         "background-color: #4A90E2; "
                         "color: white; "
                         "border: 2px solid #357ABD; "
                         "border-radius: 10px; "
                         "font-size: 12px; "
                         "font-weight: bold; "
                         "text-align: center; "
                         "} "
                         "QPushButton:hover { "
                         "background-color: #357ABD; "
                         "border-color: #2968A3; "
                         "} "
                         "QPushButton:pressed { "
                         "background-color: #2968A3; "
                         "}");
    
    return button;
}

void QuickStartDialog::onQuickGameClicked()
{
    close();
    
    QuickGameDialog dialog(m_mainWindow);
    
    // Connect signal to send game to server
    connect(&dialog, &QuickGameDialog::gameStartRequested,
            [this](const QJsonObject &gameData) {
                int laneId = gameData["lane_id"].toInt();
                // Send via MainWindow's server
                if (m_mainWindow && m_mainWindow->getLaneServer()) {
                    m_mainWindow->getLaneServer()->sendToLane(laneId, "quick_game", gameData);
                }
            });
    
    dialog.exec();
}

void QuickStartDialog::onBingoClicked()
{
    QMessageBox::information(this, "BINGO", "BINGO game mode coming soon!");
}

void QuickStartDialog::onPracticeClicked()
{
    QMessageBox::information(this, "Practice", "Practice mode coming soon!");
}

void QuickStartDialog::onPartyClicked()
{
    QMessageBox::information(this, "Party", "Party mode coming soon!");
}

void QuickStartDialog::onHorseClicked()
{
    QMessageBox::information(this, "HORSE", "HORSE game mode coming soon!");
}

void QuickStartDialog::onGolfClicked()
{
    QMessageBox::information(this, "Golf", "Golf bowling mode coming soon!");
}

void QuickStartDialog::onBaseballClicked()
{
    QMessageBox::information(this, "Baseball", "Baseball bowling mode coming soon!");
}

void QuickStartDialog::onSettingsClicked()
{
    QMessageBox::information(this, "Settings", "Settings configuration coming soon!");
}

void QuickStartDialog::onSetupClicked()
{
    QMessageBox::information(this, "Setup", "System setup coming soon!");
}

void QuickStartDialog::onEndDayClicked()
{
    QMessageBox::information(this, "End Day", "End of day procedures coming soon!");
}

void QuickStartDialog::onNewGameTypeClicked()
{
    QMessageBox::information(this, "New Game Type", "Advanced game configuration coming soon!");
}
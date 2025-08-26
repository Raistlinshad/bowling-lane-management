cmake_minimum_required(VERSION 3.16)
project(Canadian5PinBowling VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt5 components one by one for better error reporting
find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Gui REQUIRED)

# Try to find multimedia components
find_package(Qt5Multimedia QUIET)
find_package(Qt5MultimediaWidgets QUIET)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Source files
set(SOURCES
    main.cpp
    LaneClient.cpp
    QuickGame.cpp
    BowlingWidgets.cpp
    ThreeSixNineTracker.cpp
    GameStatistics.cpp
    GameRecoveryManager.cpp
    MediaManager.cpp
    client_main.cpp
)

# Header files
set(HEADERS
    LaneClient.h
    QuickGame.h
    BowlingWidgets.h
    ThreeSixNineTracker.h
    GameStatistics.h
    GameRecoveryManager.h
    MediaManager.h
)

# Check if manager classes exist, if not create stubs
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/GameRecoveryManager.cpp)
    message(STATUS "Creating GameRecoveryManager stub")
    file(WRITE ${CMAKE_BINARY_DIR}/GameRecoveryManagerStub.cpp
        "#include <QObject>\n"
        "#include <QWidget>\n"
        "#include <QJsonObject>\n"
        "class GameRecoveryManager : public QObject {\n"
        "    Q_OBJECT\n"
        "public:\n"
        "    explicit GameRecoveryManager(QObject* parent = nullptr) : QObject(parent) {}\n"
        "    void checkForRecovery(QWidget* parent) { emit recoveryDeclined(); }\n"
        "    void markGameActive(int gameNum, const QJsonObject& state) {}\n"
        "    void markGameInactive() {}\n"
        "signals:\n"
        "    void recoveryRequested(const QJsonObject& gameState);\n"
        "    void recoveryDeclined();\n"
        "};\n"
        "#include \"GameRecoveryManagerStub.moc\"\n"
    )
    list(APPEND SOURCES ${CMAKE_BINARY_DIR}/GameRecoveryManagerStub.cpp)
else()
    list(APPEND SOURCES GameRecoveryManager.cpp)
    list(APPEND HEADERS GameRecoveryManager.h)
endif()

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/GameStatistics.cpp)
    message(STATUS "Creating GameStatistics stub")
    file(WRITE ${CMAKE_BINARY_DIR}/GameStatisticsStub.cpp
        "#include <QObject>\n"
        "#include <QVector>\n"
        "#include \"QuickGame.h\"\n"
        "class GameStatistics : public QObject {\n"
        "    Q_OBJECT\n"
        "public:\n"
        "    struct HighScoreRecord { QString bowlerName; int score; };\n"
        "    struct StrikeRecord { QString bowlerName; int consecutiveStrikes; };\n"
        "    explicit GameStatistics(QObject* parent = nullptr) : QObject(parent) {}\n"
        "    void recordGameCompletion(const QVector<Bowler>&, const QString&, int) {}\n"
        "    void recordBallThrown(const QString&, int, const Ball&, bool, bool) {}\n"
        "signals:\n"
        "    void newHighScore(const HighScoreRecord& record);\n"
        "    void newStrikeRecord(const StrikeRecord& record);\n"
        "};\n"
        "#include \"GameStatisticsStub.moc\"\n"
    )
    list(APPEND SOURCES ${CMAKE_BINARY_DIR}/GameStatisticsStub.cpp)
else()
    list(APPEND SOURCES GameStatistics.cpp)
    list(APPEND HEADERS GameStatistics.h)
endif()

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/ThreeSixNineTracker.cpp)
    message(STATUS "Creating ThreeSixNineTracker stub")
    file(WRITE ${CMAKE_BINARY_DIR}/ThreeSixNineTrackerStub.cpp
        "#include <QObject>\n"
        "#include <QVector>\n"
        "class ThreeSixNineTracker : public QObject {\n"
        "    Q_OBJECT\n"
        "public:\n"
        "    enum class ParticipationMode { Everyone, Selectable };\n"
        "    explicit ThreeSixNineTracker(QObject* parent = nullptr) : QObject(parent), active(false) {}\n"
        "    void initialize(const QVector<QString>&, const QVector<int>&, ParticipationMode) { active = true; }\n"
        "    bool isActive() const { return active; }\n"
        "    bool canToggleParticipation() const { return false; }\n"
        "    void setBowlerParticipation(const QString&, bool) {}\n"
        "    void recordFrameResult(const QString&, int, int, bool) {}\n"
        "    QString getStatusText(const QString&) const { return \"\"; }\n"
        "    int getDotsCount(const QString&) const { return 0; }\n"
        "private:\n"
        "    bool active;\n"
        "signals:\n"
        "    void participantWon(const QString& bowlerName);\n"
        "    void participantAlmostWon(const QString& bowlerName);\n"
        "};\n"
        "#include \"ThreeSixNineTrackerStub.moc\"\n"
    )
    list(APPEND SOURCES ${CMAKE_BINARY_DIR}/ThreeSixNineTrackerStub.cpp)
else()
    list(APPEND SOURCES ThreeSixNineTracker.cpp)
    list(APPEND HEADERS ThreeSixNineTracker.h)
endif()

# Add MediaManager if multimedia is available, otherwise create stub
if(Qt5Multimedia_FOUND AND Qt5MultimediaWidgets_FOUND)
    message(STATUS "Qt5 Multimedia components found - checking for MediaManager")
    if(EXISTS ${CMAKE_SOURCE_DIR}/MediaManager.cpp)
        message(STATUS "Using existing MediaManager.cpp")
        list(APPEND SOURCES MediaManager.cpp)
        list(APPEND HEADERS MediaManager.h)
        add_definitions(-DMULTIMEDIA_SUPPORT)
    else()
        message(STATUS "MediaManager.cpp not found, creating multimedia stub")
        file(WRITE ${CMAKE_BINARY_DIR}/MediaManagerStub.cpp
            "#include <QStackedWidget>\n"
            "#include <QLabel>\n"
            "#include <QVBoxLayout>\n"
            "#include <QTimer>\n"
            "class MediaManager : public QStackedWidget {\n"
            "    Q_OBJECT\n"
            "public:\n"
            "    MediaManager(QWidget* parent = nullptr) : QStackedWidget(parent) {\n"
            "        QWidget* stub = new QWidget(this);\n"
            "        stub->setStyleSheet(\"background-color: blue;\");\n"
            "        QLabel* label = new QLabel(\"Media Display\\n(Multimedia available but MediaManager.cpp missing)\", stub);\n"
            "        label->setStyleSheet(\"color: white; font-size: 18px;\");\n"
            "        label->setAlignment(Qt::AlignCenter);\n"
            "        QVBoxLayout* layout = new QVBoxLayout(stub);\n"
            "        layout->addWidget(label);\n"
            "        addWidget(stub);\n"
            "    }\n"
            "    void showGameDisplay(QWidget* w) { if(w) { removeWidget(widget(0)); addWidget(w); setCurrentWidget(w); } }\n"
            "    void showEffect(const QString& e, int d = 2000) { QTimer::singleShot(d, [this,e](){ emit effectFinished(e); }); emit effectStarted(e); }\n"
            "    void showMediaRotation() {}\n"
            "    void loadSettings(const QJsonObject&) {}\n"
            "signals:\n"
            "    void effectStarted(const QString& effect);\n"
            "    void effectFinished(const QString& effect);\n"
            "};\n"
            "#include \"MediaManagerStub.moc\"\n"
        )
        list(APPEND SOURCES ${CMAKE_BINARY_DIR}/MediaManagerStub.cpp)
    endif()
else()
    message(STATUS "Building without multimedia support")
    file(WRITE ${CMAKE_BINARY_DIR}/MediaManagerStub.cpp
        "#include <QStackedWidget>\n"
        "#include <QLabel>\n"
        "#include <QVBoxLayout>\n"
        "#include <QTimer>\n"
        "class MediaManager : public QStackedWidget {\n"
        "    Q_OBJECT\n"
        "public:\n"
        "    MediaManager(QWidget* parent = nullptr) : QStackedWidget(parent) {\n"
        "        QWidget* stub = new QWidget(this);\n"
        "        stub->setStyleSheet(\"background-color: blue;\");\n"
        "        QLabel* label = new QLabel(\"Media Display\\n(Multimedia disabled)\", stub);\n"
        "        label->setStyleSheet(\"color: white; font-size: 18px;\");\n"
        "        label->setAlignment(Qt::AlignCenter);\n"
        "        QVBoxLayout* layout = new QVBoxLayout(stub);\n"
        "        layout->addWidget(label);\n"
        "        addWidget(stub);\n"
        "    }\n"
        "    void showGameDisplay(QWidget* w) { if(w) { removeWidget(widget(0)); addWidget(w); setCurrentWidget(w); } }\n"
        "    void showEffect(const QString& e, int d = 2000) { QTimer::singleShot(d, [this,e](){ emit effectFinished(e); }); emit effectStarted(e); }\n"
        "    void showMediaRotation() {}\n"
        "    void loadSettings(const QJsonObject&) {}\n"
        "signals:\n"
        "    void effectStarted(const QString& effect);\n"
        "    void effectFinished(const QString& effect);\n"
        "};\n"
        "#include \"MediaManagerStub.moc\"\n"
    )
    list(APPEND SOURCES ${CMAKE_BINARY_DIR}/MediaManagerStub.cpp)
endif()

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    Qt5::Gui
)

# Link multimedia if available
if(Qt5Multimedia_FOUND AND Qt5MultimediaWidgets_FOUND)
    target_link_libraries(${PROJECT_NAME}
        Qt5::Multimedia
        Qt5::MultimediaWidgets
    )
endif()

# Copy files
if(EXISTS ${CMAKE_SOURCE_DIR}/machine_interface.py)
    configure_file(${CMAKE_SOURCE_DIR}/machine_interface.py ${CMAKE_BINARY_DIR}/machine_interface.py COPYONLY)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/settings.json)
    configure_file(${CMAKE_SOURCE_DIR}/settings.json ${CMAKE_BINARY_DIR}/settings.json COPYONLY)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/settings.ini)
    configure_file(${CMAKE_SOURCE_DIR}/settings.ini ${CMAKE_BINARY_DIR}/settings.ini COPYONLY)
endif()

# Create directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/media)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)

message(STATUS "Build configuration complete")
message(STATUS "  Multimedia support: ${Qt5Multimedia_FOUND}")
message(STATUS "  Multimedia widgets: ${Qt5MultimediaWidgets_FOUND}")
message(STATUS "  Manager class stubs created as needed")
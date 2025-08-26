#include <QCoreApplication>
#include <QObject>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QSettings>
#include "LaneClient.h"

class LaneApplication : public QObject
{
    Q_OBJECT

class LaneApplication : public QObject
{
    Q_OBJECT

public:
    LaneApplication(QObject *parent = nullptr)
        : QObject(parent)
        , m_client(nullptr)
        , m_laneId(1)
        , m_serverHost("192.168.2.243")
        , m_serverPort(50005)
    {
        // Load settings first
        loadSettings();
        
        // Create client with loaded settings
        m_client = new LaneClient(m_laneId, this);
        m_client->setServerAddress(m_serverHost, m_serverPort);
        
        // Connect signals
        connect(m_client, &LaneClient::connected, this, &LaneApplication::onConnected);
        connect(m_client, &LaneClient::disconnected, this, &LaneApplication::onDisconnected);
        connect(m_client, &LaneClient::gameCommandReceived, this, &LaneApplication::onGameCommand);
        connect(m_client, &LaneClient::serverMessageReceived, this, &LaneApplication::onServerMessage);
        
        // Start client
        m_client->start();
        
        qDebug() << "Lane application started for lane" << m_laneId;
    }

private slots:
    void onConnected()
    {
        qDebug() << "Connected to server";
        // Send initial status
        m_client->sendStatusUpdate("ready");
    }
    
    void onDisconnected()
    {
        qDebug() << "Disconnected from server";
    }
    
    void onGameCommand(const QString &type, const QJsonObject &data)
    {
        qDebug() << "Received game command:" << type;
        
        if (type == "quick_game") {
            handleQuickGame(data);
        } else if (type == "league_game") {
            handleLeagueGame(data);
        } else if (type == "pre_bowl") {
            handlePreBowl(data);
        }
    }
    
    void onServerMessage(const QJsonObject &message)
    {
        qDebug() << "Received server message:" << message["type"].toString();
        // Handle other server messages here
    }
    
    void handleQuickGame(const QJsonObject &data)
    {
        qDebug() << "Starting quick game with data:" << data;
        
        // Get lane-specific pin settings for the machine control
        QJsonObject machineData = data;
        if (!m_laneSettings.isEmpty()) {
            machineData["lane_settings"] = m_laneSettings;
        }
        
        // Here you would interface with your Python machine control
        // Example: Save game data to file for Python to read
        QJsonDocument gameDoc(machineData);
        QFile gameFile("current_game.json");
        if (gameFile.open(QIODevice::WriteOnly)) {
            gameFile.write(gameDoc.toJson());
            gameFile.close();
        }
        
        // For now, just acknowledge
        m_client->sendStatusUpdate("game_running");
        
        // Simulate game completion after 30 seconds
        QTimer::singleShot(30000, this, [this]() {
            simulateGameComplete();
        });
    }
    
    void handleLeagueGame(const QJsonObject &data)
    {
        qDebug() << "Starting league game with data:" << data;
        
        // Include lane-specific settings
        QJsonObject machineData = data;
        if (!m_laneSettings.isEmpty()) {
            machineData["lane_settings"] = m_laneSettings;
        }
        
        // Save for Python machine control
        QJsonDocument gameDoc(machineData);
        QFile gameFile("current_league_game.json");
        if (gameFile.open(QIODevice::WriteOnly)) {
            gameFile.write(gameDoc.toJson());
            gameFile.close();
        }
        
        m_client->sendStatusUpdate("league_game_running");
        
        // Simulate game completion
        QTimer::singleShot(45000, this, [this]() {
            simulateGameComplete();
        });
    }
    
    void handlePreBowl(const QJsonObject &data)
    {
        qDebug() << "Starting pre-bowl with data:" << data;
        
        // Include lane-specific settings
        QJsonObject machineData = data;
        if (!m_laneSettings.isEmpty()) {
            machineData["lane_settings"] = m_laneSettings;
        }
        
        // Save for Python machine control
        QJsonDocument gameDoc(machineData);
        QFile gameFile("current_prebowl.json");
        if (gameFile.open(QIODevice::WriteOnly)) {
            gameFile.write(gameDoc.toJson());
            gameFile.close();
        }
        
        m_client->sendStatusUpdate("pre_bowl_running");
        
        // Simulate pre-bowl completion
        QTimer::singleShot(20000, this, [this]() {
            simulateGameComplete();
        });
    }
    
    void simulateGameComplete()
    {
        qDebug() << "Simulating game completion";
        
        // Create sample game completion data
        QJsonObject gameData;
        gameData["lane_id"] = m_laneId;
        gameData["status"] = "completed";
        gameData["final_scores"] = QJsonArray{185, 167, 201};
        gameData["total_frames"] = 10;
        gameData["completion_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        m_client->sendGameComplete(gameData);
        m_client->sendStatusUpdate("ready");
    }

private:
    void loadSettings()
    {
        // Try JSON settings first (your current format)
        QFile jsonFile("settings.json");
        if (jsonFile.open(QIODevice::ReadOnly)) {
            QByteArray jsonData = jsonFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(jsonData);
            QJsonObject json = doc.object();
            
            // Load lane ID (string in JSON, convert to int)
            QString laneStr = json["Lane"].toString();
            m_laneId = laneStr.toInt();
            if (m_laneId == 0) m_laneId = 1; // fallback
            
            // Load server settings
            m_serverHost = json["ServerIP"].toString();
            if (m_serverHost.isEmpty()) m_serverHost = "192.168.2.243";
            
            m_serverPort = json["ServerPort"].toInt();
            if (m_serverPort == 0) m_serverPort = 50005;
            
            // Load lane-specific pin settings if they exist
            QString laneKey = QString::number(m_laneId);
            if (json.contains(laneKey)) {
                m_laneSettings = json[laneKey].toObject();
                qDebug() << "Loaded lane-specific settings for lane" << m_laneId;
            }
            
            // Load game colors
            if (json.contains("GameColors")) {
                m_gameColors = json["GameColors"].toObject();
            }
            
            qDebug() << "Loaded JSON settings - Lane ID:" << m_laneId << "Server:" << m_serverHost << ":" << m_serverPort;
        } else {
            // Fallback to INI format
            QSettings settings("settings.ini", QSettings::IniFormat);
            m_laneId = settings.value("Lane/id", 1).toInt();
            m_serverHost = settings.value("Server/host", "192.168.2.243").toString();
            m_serverPort = settings.value("Server/port", 50005).toInt();
            
            qDebug() << "Loaded INI settings - Lane ID:" << m_laneId << "Server:" << m_serverHost << ":" << m_serverPort;
        }
        
        // Apply server settings to client
        if (m_client) {
            m_client->setServerAddress(m_serverHost, m_serverPort);
        }
    }
    
    int m_laneId;
    LaneClient *m_client;
    QString m_serverHost;
    quint16 m_serverPort;
    QJsonObject m_laneSettings;
    QJsonObject m_gameColors;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("LaneClient");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("BowlingCenter");
    
    // Create and start the lane application
    LaneApplication laneApp;
    
    return app.exec();
}

#include "client_main.moc"
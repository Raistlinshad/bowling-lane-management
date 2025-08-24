#ifndef LANECLIENT_H
#define LANECLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QHostAddress>
#include <QUdpSocket>
#include <QNetworkInterface>

enum class ClientConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting
};

ClientConnectionState m_connectionState;
int m_reconnectAttempts;
QTimer* m_reconnectTimer;

class LaneClient : public QObject
{
    Q_OBJECT

public:
    explicit LaneClient(int laneId, QObject *parent = nullptr);
    ~LaneClient();
    
    void setServerAddress(const QString &host, quint16 port = 50005);
    void start();
    void stop();
    
    bool isConnected() const;
    int getLaneId() const { return m_laneId; }
    
    // Game interface
    void sendGameComplete(const QJsonObject &gameData);
    void sendFrameUpdate(const QJsonObject &frameData);
    void sendStatusUpdate(const QString &status);

signals:
    void connected();
    void disconnected();
    void gameCommandReceived(const QString &type, const QJsonObject &data);
    void serverMessageReceived(const QJsonObject &message);
    void connectionStateChanged(ClientConnectionState state);

public slots:
    void connectToServer();
    void sendMessage(const QJsonObject &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void sendHeartbeat();
    void attemptReconnection();
    void onServerDiscoveryResponse();

private:
    void setupHeartbeat();
    void processMessage(const QJsonObject &message);
    void handleRegistrationResponse(const QJsonObject &message);
    void handleGameCommand(const QJsonObject &message);
    void handleHeartbeatResponse(const QJsonObject &message);
    void handleTeamMove(const QJsonObject &message);
    
    void setConnectionState(ClientConnectionState state);
    void startServerDiscovery();
    void sendRegistration();
    QString getLocalIpAddress();
    bool validateConnection();
    
    // Connection management
    int m_laneId;
    QString m_serverHost;
    quint16 m_serverPort;
    QTcpSocket *m_socket;
    ClientConnectionState m_connectionState;
    
    // Heartbeat and reconnection
    QTimer *m_heartbeatTimer;
    QTimer *m_reconnectTimer;
    QTimer *m_discoveryTimer;
    QUdpSocket *m_discoverySocket;
    
    bool m_registered;
    QDateTime m_lastHeartbeat;
    int m_reconnectAttempts;
    int m_maxReconnectAttempts;
    
    // Constants
    static const int HEARTBEAT_INTERVAL = 10000;     // 10 seconds
    static const int RECONNECT_INTERVAL = 5000;      // 5 seconds  
    static const int DISCOVERY_INTERVAL = 30000;     // 30 seconds
    static const int MAX_RECONNECT_ATTEMPTS = 10;
};


#endif // LANECLIENT_H

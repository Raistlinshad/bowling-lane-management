#ifndef LANESERVER_H
#define LANESERVER_H

#include <QObject>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QDateTime>
#include <QMap>
#include "EventBus.h"

enum class LaneStatus {
    Idle,
    Active,
    Maintenance,
    Error
};

struct LaneConnection {
    QTcpSocket *socket = nullptr;
    int laneId = -1;
    QDateTime lastSeen;
    LaneStatus status = LaneStatus::Idle;
    QJsonObject gameData;
};

class LaneServer : public QObject
{
    Q_OBJECT

public:
    explicit LaneServer(EventBus *eventBus, QObject *parent = nullptr);
    ~LaneServer();
    void start(quint16 port = 50005);
    void stop();
    void handleTeamMove(int fromLane, int toLane, const QString &teamData);

signals:
    void laneStatusChanged(int laneId, LaneStatus status);
    void gameDataReceived(int laneId, const QJsonObject &gameData);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onClientDataReady();
    void checkConnections();

private:
    void processMessage(QTcpSocket *socket, const QJsonObject &message);
    void handleRegistration(QTcpSocket *socket, const QJsonObject &message);
    void handleHeartbeat(QTcpSocket *socket, const QJsonObject &message);
    void handleGameData(QTcpSocket *socket, const QJsonObject &message);
    void updateLaneStatus(int laneId, LaneStatus status);
    void sendToLane(int laneId, const QString &command, const QJsonObject &data);
    void onLaneCommand(const QJsonObject &data);

    QTcpServer *m_server;
    EventBus *m_eventBus;
    QTimer *m_connectionTimer;
    QMap<QTcpSocket*, LaneConnection> m_connections;
    QMap<int, QTcpSocket*> m_laneToSocket;
    bool m_running;
    
    static const int HEARTBEAT_TIMEOUT = 30000; // 30 seconds
};

#endif // LANESERVER_H
#include "LaneServer.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostAddress>
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

LaneServer::LaneServer(EventBus *eventBus, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_eventBus(eventBus)
    , m_connectionTimer(new QTimer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &LaneServer::onNewConnection);
    connect(m_connectionTimer, &QTimer::timeout, this, &LaneServer::checkConnections);
        
    m_connectionTimer->start(10000); // Check every 10 seconds
}

LaneServer::~LaneServer()
{
    stop();
}

void LaneServer::start(quint16 port)
{
    if (m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "Lane server started on port" << port;
    } else {
        qDebug() << "Failed to start server:" << m_server->errorString();
    }
}

void LaneServer::onNewConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    if (!socket) return;
    
    qDebug() << "New connection from" << socket->peerAddress().toString();
    
    // Connect socket signals
    connect(socket, &QTcpSocket::disconnected, this, &LaneServer::onClientDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &LaneServer::onClientDataReady);
    
    // Initialize connection data
    LaneConnection connection;
    connection.socket = socket;
    connection.laneId = -1; // Will be set during registration
    connection.lastSeen = QDateTime::currentDateTime();
    connection.status = LaneStatus::Idle;
    
    m_connections[socket] = connection;
}

void LaneServer::onClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    if (m_connections.contains(socket)) {
        LaneConnection connection = m_connections[socket];
        if (connection.laneId > 0) {
            updateLaneStatus(connection.laneId, LaneStatus::Idle);
            m_laneToSocket.remove(connection.laneId);
            qDebug() << "Lane" << connection.laneId << "disconnected";
        }
        m_connections.remove(socket);
    }
    
    socket->deleteLater();
}

void LaneServer::stop()
{
    if (m_server->isListening()) {
        m_server->close();
        qDebug() << "Server stopped";
    }
    
    // Clean up all connections
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        QTcpSocket *socket = it.key();
        if (socket) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }
    m_connections.clear();
    m_laneToSocket.clear();
}

void LaneServer::onClientDataReady()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    while (socket->canReadLine()) {
        QByteArray data = socket->readLine();
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        
        if (error.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << error.errorString();
            continue;
        }
        
        if (doc.isObject()) {
            processMessage(socket, doc.object());
        }
    }
}

void LaneServer::processMessage(QTcpSocket *socket, const QJsonObject &message)
{
    QString type = message["type"].toString();
    
    if (type == "registration") {
        handleRegistration(socket, message);
    } else if (type == "heartbeat") {
        handleHeartbeat(socket, message);
    } else if (type == "game_data") {
        handleGameData(socket, message);
    } else if (type == "team_move") {
        // Handle team move between lanes
        QJsonObject data = message["data"].toObject();
        int fromLane = data["from_lane"].toInt();
        int toLane = data["to_lane"].toInt();
        QString teamData = data["team_data"].toString();
        handleTeamMove(fromLane, toLane, teamData);
    }
}

void LaneServer::handleRegistration(QTcpSocket *socket, const QJsonObject &message)
{
    int laneId = message["lane_id"].toInt();
    
    if (laneId <= 0) {
        qWarning() << "Invalid lane ID in registration";
        return;
    }
    
    // Update connection info
    if (m_connections.contains(socket)) {
        m_connections[socket].laneId = laneId;
        m_connections[socket].lastSeen = QDateTime::currentDateTime();
        m_connections[socket].status = LaneStatus::Active;
        
        m_laneToSocket[laneId] = socket;
        
        // Send registration response
        QJsonObject response;
        response["type"] = "registration_response";
        response["status"] = "success";
        response["lane_id"] = laneId;
        response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        QJsonDocument doc(response);
        socket->write(doc.toJson(QJsonDocument::Compact) + "\n");
        
        updateLaneStatus(laneId, LaneStatus::Active);
        qDebug() << "Lane" << laneId << "registered successfully";
    }
}

void LaneServer::handleHeartbeat(QTcpSocket *socket, const QJsonObject &message)
{
    if (m_connections.contains(socket)) {
        m_connections[socket].lastSeen = QDateTime::currentDateTime();
        
        // Send heartbeat response
        QJsonObject response;
        response["type"] = "heartbeat_response";
        response["status"] = "ok";
        response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        QJsonDocument doc(response);
        socket->write(doc.toJson(QJsonDocument::Compact) + "\n");
    }
}

void LaneServer::handleGameData(QTcpSocket *socket, const QJsonObject &message)
{
    if (m_connections.contains(socket)) {
        LaneConnection &connection = m_connections[socket];
        connection.gameData = message["data"].toObject();
        connection.lastSeen = QDateTime::currentDateTime();
        
        // Update status to game running if we have game data
        if (connection.status != LaneStatus::Active) {
            connection.status = LaneStatus::Active;
            updateLaneStatus(connection.laneId, LaneStatus::Active);
        }
        
        emit gameDataReceived(connection.laneId, connection.gameData);
    }
}

void LaneServer::checkConnections()
{
    QDateTime now = QDateTime::currentDateTime();
    
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        LaneConnection &connection = it.value();
        
        if (connection.laneId > 0) {
            qint64 secondsSinceLastSeen = connection.lastSeen.secsTo(now);
            
            if (secondsSinceLastSeen > HEARTBEAT_TIMEOUT / 1000) {
                // Connection is stale
                if (connection.status != LaneStatus::Idle) {
                    updateLaneStatus(connection.laneId, LaneStatus::Idle);
                    connection.status = LaneStatus::Idle;
                    qDebug() << "Lane" << connection.laneId << "connection timeout";
                }
            }
        }
    }
}

void LaneServer::updateLaneStatus(int laneId, LaneStatus status)
{
    emit laneStatusChanged(laneId, status);
}

void LaneServer::sendToLane(int laneId, const QString &command, const QJsonObject &data)
{
    if (!m_laneToSocket.contains(laneId)) {
        qWarning() << "Lane" << laneId << "not connected";
        return;
    }
    
    QTcpSocket *socket = m_laneToSocket[laneId];
    if (!socket || socket->state() != QTcpSocket::ConnectedState) {
        qWarning() << "Lane" << laneId << "socket not valid";
        return;
    }
    
    QJsonObject message;
    message["type"] = command;
    message["data"] = data;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(message);
    socket->write(doc.toJson(QJsonDocument::Compact) + "\n");
    
    qDebug() << "Sent" << command << "to lane" << laneId;
}

void LaneServer::handleTeamMove(int fromLane, int toLane, const QString &teamData)
{
    qDebug() << "Team move request: from lane" << fromLane << "to lane" << toLane;
    
    // Send team data to target lane
    QJsonObject moveData;
    moveData["type"] = "team_move";
    moveData["from_lane"] = fromLane;
    moveData["to_lane"] = toLane;
    moveData["team_data"] = teamData;
    
    sendToLane(toLane, "team_move", moveData);
    
    // Optionally send confirmation back to source lane
    QJsonObject confirmData;
    confirmData["type"] = "team_move_confirm";
    confirmData["target_lane"] = toLane;
    confirmData["status"] = "sent";
    
    sendToLane(fromLane, "team_move_confirm", confirmData);
}

void LaneServer::onLaneCommand(const QJsonObject &data)
{
    int laneId = data["lane_id"].toInt();
    QString type = data["type"].toString();
    QJsonObject commandData = data["data"].toObject();
    
    qDebug() << "Lane command:" << type << "for lane" << laneId;
    
    sendToLane(laneId, type, commandData);
}

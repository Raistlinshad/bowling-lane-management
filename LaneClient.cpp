#include "LaneClient.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QNetworkInterface>
#include <QHostAddress>

LaneClient::LaneClient(int laneId, QObject *parent)
    : QObject(parent)
    , m_laneId(laneId)
    , m_serverHost("192.168.2.243") // Default server
    , m_serverPort(50005)
    , m_socket(new QTcpSocket(this))
    , m_connectionState(ClientConnectionState::Disconnected)
    , m_heartbeatTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_discoveryTimer(new QTimer(this))
    , m_discoverySocket(new QUdpSocket(this))
    , m_registered(false)
    , m_reconnectAttempts(0)
    , m_maxReconnectAttempts(MAX_RECONNECT_ATTEMPTS)
{
    // Setup socket connections
    connect(m_socket, &QTcpSocket::connected, this, &LaneClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &LaneClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &LaneClient::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &LaneClient::onError);
    
    // Setup timers
    connect(m_heartbeatTimer, &QTimer::timeout, this, &LaneClient::sendHeartbeat);
    connect(m_reconnectTimer, &QTimer::timeout, this, &LaneClient::attemptReconnection);
    connect(m_discoveryTimer, &QTimer::timeout, this, &LaneClient::startServerDiscovery);
    
    // Setup discovery socket
    connect(m_discoverySocket, &QUdpSocket::readyRead, this, &LaneClient::onServerDiscoveryResponse);
    
    // Configure timers
    m_heartbeatTimer->setInterval(HEARTBEAT_INTERVAL);
    m_reconnectTimer->setInterval(RECONNECT_INTERVAL);
    m_reconnectTimer->setSingleShot(true);
    m_discoveryTimer->setInterval(DISCOVERY_INTERVAL);
    
    qDebug() << "LaneClient initialized for lane" << m_laneId;
}

LaneClient::~LaneClient()
{
    stop();
}

void LaneClient::setServerAddress(const QString &host, quint16 port)
{
    m_serverHost = host;
    m_serverPort = port;
    qDebug() << "Server address set to" << host << ":" << port;
}

void LaneClient::start()
{
    qDebug() << "Starting lane client for lane" << m_laneId;
    
    // Start server discovery
    startServerDiscovery();
    m_discoveryTimer->start();
    
    // Attempt initial connection
    connectToServer();
}

void LaneClient::stop()
{
    qDebug() << "Stopping lane client";
    
    // Stop all timers
    m_heartbeatTimer->stop();
    m_reconnectTimer->stop();
    m_discoveryTimer->stop();
    
    // Close socket
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(3000);
        }
    }
    
    setConnectionState(ClientConnectionState::Disconnected);
    m_registered = false;
}

bool LaneClient::isConnected() const
{
    return m_connectionState == ClientConnectionState::Connected && m_registered;
}

void LaneClient::connectToServer()
{
    if (m_connectionState == ClientConnectionState::Connecting || 
        m_connectionState == ClientConnectionState::Connected) {
        return;
    }
    
    setConnectionState(ClientConnectionState::Connecting);
    m_registered = false;
    
    qDebug() << "Connecting to server at" << m_serverHost << ":" << m_serverPort;
    m_socket->connectToHost(m_serverHost, m_serverPort);
}

void LaneClient::onConnected()
{
    qDebug() << "Connected to server";
    setConnectionState(ClientConnectionState::Connected);
    m_reconnectAttempts = 0;
    
    // Send registration immediately
    sendRegistration();
    
    emit connected();
}

void LaneClient::onDisconnected()
{
    qDebug() << "Disconnected from server";
    
    bool wasRegistered = m_registered;
    m_registered = false;
    m_heartbeatTimer->stop();
    
    if (m_connectionState != ClientConnectionState::Disconnected) {
        setConnectionState(ClientConnectionState::Reconnecting);
        
        // Start reconnection attempts
        m_reconnectTimer->start();
    }
    
    if (wasRegistered) {
        emit disconnected();
    }
}

void LaneClient::onError(QAbstractSocket::SocketError error)
{
    qWarning() << "Socket error:" << error << m_socket->errorString();
    
    if (m_connectionState == ClientConnectionState::Connecting ||
        m_connectionState == ClientConnectionState::Connected) {
        setConnectionState(ClientConnectionState::Reconnecting);
        m_reconnectTimer->start();
    }
}

void LaneClient::onReadyRead()
{
    while (m_socket->canReadLine()) {
        QByteArray data = m_socket->readLine();
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        
        if (error.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << error.errorString();
            continue;
        }
        
        if (doc.isObject()) {
            processMessage(doc.object());
        }
    }
}

void LaneClient::processMessage(const QJsonObject &message)
{
    QString type = message["type"].toString();
    
    if (type == "registration_response") {
        handleRegistrationResponse(message);
    } else if (type == "heartbeat_response") {
        handleHeartbeatResponse(message);
    } else if (type == "quick_game" || type == "league_game" || type == "pre_bowl") {
        handleGameCommand(message);
    } else if (type == "team_move") {
        handleTeamMove(message);
    } else if (type == "ping") {
        // Respond to server ping
        QJsonObject response;
        response["type"] = "pong";
        response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        sendMessage(response);
    } else {
        // Forward other messages
        emit serverMessageReceived(message);
    }
}

void LaneClient::handleRegistrationResponse(const QJsonObject &message)
{
    QString status = message["status"].toString();
    
    if (status == "success") {
        m_registered = true;
        setupHeartbeat();
        qDebug() << "Successfully registered with server";
    } else {
        qWarning() << "Registration failed:" << message["message"].toString();
        // Try to reconnect
        setConnectionState(ClientConnectionState::Reconnecting);
        m_reconnectTimer->start();
    }
}

void LaneClient::handleGameCommand(const QJsonObject &message)
{
    QString type = message["type"].toString();
    QJsonObject data = message["data"].toObject();
    
    qDebug() << "Received game command:" << type;
    emit gameCommandReceived(type, data);
}

void LaneClient::handleHeartbeatResponse(const QJsonObject &message)
{
    m_lastHeartbeat = QDateTime::currentDateTime();
    // Heartbeat acknowledged - connection is healthy
}

void LaneClient::handleTeamMove(const QJsonObject &message)
{
    qDebug() << "Received team move request";
    emit serverMessageReceived(message);
}

void LaneClient::sendRegistration()
{
    QJsonObject registration;
    registration["type"] = "registration";
    registration["lane_id"] = m_laneId;
    registration["client_ip"] = getLocalIpAddress();
    registration["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    sendMessage(registration);
    qDebug() << "Sent registration for lane" << m_laneId;
}

void LaneClient::setupHeartbeat()
{
    m_lastHeartbeat = QDateTime::currentDateTime();
    m_heartbeatTimer->start();
}

void LaneClient::sendHeartbeat()
{
    if (!m_registered) {
        m_heartbeatTimer->stop();
        return;
    }
    
    QJsonObject heartbeat;
    heartbeat["type"] = "heartbeat";
    heartbeat["lane_id"] = m_laneId;
    heartbeat["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    sendMessage(heartbeat);
}

void LaneClient::sendMessage(const QJsonObject &message)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Cannot send message - not connected";
        return;
    }
    
    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    
    m_socket->write(data);
    m_socket->flush();
}

void LaneClient::attemptReconnection()
{
    if (m_reconnectAttempts >= m_maxReconnectAttempts) {
        qWarning() << "Max reconnection attempts reached, starting server discovery";
        startServerDiscovery();
        m_reconnectAttempts = 0;
        return;
    }
    
    m_reconnectAttempts++;
    qDebug() << "Reconnection attempt" << m_reconnectAttempts << "of" << m_maxReconnectAttempts;
    
    connectToServer();
}

void LaneClient::startServerDiscovery()
{
    qDebug() << "Starting server discovery";
    
    // Bind to discovery port
    if (!m_discoverySocket->bind(QHostAddress::Any, 50005, QUdpSocket::ShareAddress)) {
        qWarning() << "Failed to bind discovery socket";
        return;
    }
    
    // Join multicast group
    QHostAddress multicastAddress("224.3.29.71");
    if (!m_discoverySocket->joinMulticastGroup(multicastAddress)) {
        qWarning() << "Failed to join multicast group";
    }
    
    // Send discovery request
    QByteArray request = "LANE_DISCOVERY_REQUEST";
    m_discoverySocket->writeDatagram(request, multicastAddress, 50005);
}

void LaneClient::onServerDiscoveryResponse()
{
    while (m_discoverySocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_discoverySocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        
        m_discoverySocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        if (datagram.startsWith("LANE_DISCOVERY_RESPONSE")) {
            // Extract server info
            QByteArray jsonData = datagram.mid(24); // Skip "LANE_DISCOVERY_RESPONSE"
            
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
            
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject serverInfo = doc.object();
                QString host = serverInfo["host"].toString();
                int port = serverInfo["port"].toInt();
                
                if (!host.isEmpty() && port > 0) {
                    qDebug() << "Server discovered at" << host << ":" << port;
                    setServerAddress(host, port);
                    
                    // Close discovery socket
                    m_discoverySocket->close();
                    
                    // Attempt connection
                    connectToServer();
                }
            }
        }
    }
}

void LaneClient::setConnectionState(ClientConnectionState state)
{
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

QString LaneClient::getLocalIpAddress()
{
    // Get the first non-loopback IPv4 address
    const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
    for (const QHostAddress &address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost) {
            return address.toString();
        }
    }
    return QHostAddress(QHostAddress::LocalHost).toString();
}

// Game interface methods
void LaneClient::sendGameComplete(const QJsonObject &gameData)
{
    QJsonObject message;
    message["type"] = "game_complete";
    message["lane_id"] = m_laneId;
    message["data"] = gameData;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    sendMessage(message);
}

void LaneClient::sendFrameUpdate(const QJsonObject &frameData)
{
    QJsonObject message;
    message["type"] = "frame_update";
    message["lane_id"] = m_laneId;
    message["data"] = frameData;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    sendMessage(message);
}

void LaneClient::sendStatusUpdate(const QString &status)
{
    QJsonObject message;
    message["type"] = "status_update";
    message["lane_id"] = m_laneId;
    message["status"] = status;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    sendMessage(message);
}
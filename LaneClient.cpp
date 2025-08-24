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
    // startServerDiscovery();
    // m_discoveryTimer->start();
    
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
    qDebug() << "=== LaneClient::onConnected() ===";
    qDebug() << "Successfully connected to server at" << m_serverHost << ":" << m_serverPort;
    
    setConnectionState(ClientConnectionState::Connected);
    m_reconnectAttempts = 0;
    
    qDebug() << "About to send registration";
    // Send registration immediately
    sendRegistration();
    qDebug() << "Registration sent";
    
    qDebug() << "About to emit connected signal";
    emit connected();
    qDebug() << "Emitted connected signal";
    
    qDebug() << "=== LaneClient::onConnected() - Complete ===";
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
    
    // FIXED: Use ClientConnectionState instead of ConnectionState
    if (m_connectionState == ClientConnectionState::Connected || 
        m_connectionState == ClientConnectionState::Connecting) {
        m_connectionState = ClientConnectionState::Reconnecting;
        m_registered = false;
        m_heartbeatTimer->stop();
        
        // Start reconnection with backoff
        int delay = std::min(1000 * (1 << m_reconnectAttempts), 30000);
        m_reconnectTimer->start(delay);
        m_reconnectAttempts++;
    }
}

void LaneClient::onReadyRead()
{
    qDebug() << "=== LaneClient::onReadyRead() - Data available ===";
    
    while (m_socket->canReadLine()) {
        QByteArray data = m_socket->readLine();
        qDebug() << "Raw data received:" << data;
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        
        if (error.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << error.errorString();
            qWarning() << "Failed to parse data:" << data;
            continue;
        }
        
        if (doc.isObject()) {
            QJsonObject message = doc.object();
            qDebug() << "Parsed JSON message:" << message;
            processMessage(message);
        } else {
            qWarning() << "Received JSON is not an object:" << doc;
        }
    }
    
    qDebug() << "=== LaneClient::onReadyRead() - Finished processing ===";
}

void LaneClient::processMessage(const QJsonObject &message)
{
    QString type = message["type"].toString();
    qDebug() << "=== LaneClient::processMessage() ===" << type;
    qDebug() << "Full message:" << message;
    
    if (type == "registration_response") {
        qDebug() << "Processing registration_response";
        handleRegistrationResponse(message);
    } else if (type == "heartbeat_response") {
        qDebug() << "Processing heartbeat_response";
        handleHeartbeatResponse(message);
    } else if (type == "quick_game" || type == "league_game" || type == "pre_bowl") {
        qDebug() << "Processing game command:" << type;
        qDebug() << "About to call handleGameCommand()";
        handleGameCommand(message);
        qDebug() << "Finished handleGameCommand()";
    } else if (type == "team_move") {
        qDebug() << "Processing team_move";
        handleTeamMove(message);
    } else if (type == "ping") {
        qDebug() << "Processing ping - sending pong response";
        // Respond to server ping
        QJsonObject response;
        response["type"] = "pong";
        response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        sendMessage(response);
    } else {
        qDebug() << "Forwarding unknown message type:" << type;
        // Forward other messages
        emit serverMessageReceived(message);
    }
    
    qDebug() << "=== LaneClient::processMessage() - Complete ===";
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
    
    qDebug() << "=== LaneClient::handleGameCommand() ===";
    qDebug() << "Command type:" << type;
    qDebug() << "Command data:" << data;
    qDebug() << "Data keys:" << data.keys();
    
    qDebug() << "About to emit gameCommandReceived signal";
    emit gameCommandReceived(type, data);
    qDebug() << "Emitted gameCommandReceived signal";
    
    qDebug() << "=== LaneClient::handleGameCommand() - Complete ===";
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
    qDebug() << "=== LaneClient::sendRegistration() ===";
    
    QJsonObject registration;
    registration["type"] = "registration";
    registration["lane_id"] = QString::number(m_laneId);
    registration["client_ip"] = getLocalIpAddress();
    registration["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    qDebug() << "Registration data:" << registration;
    qDebug() << "Lane ID:" << m_laneId;
    qDebug() << "Client IP:" << getLocalIpAddress();
    
    sendMessage(registration);
    qDebug() << "Sent registration for lane" << m_laneId;
    
    qDebug() << "=== LaneClient::sendRegistration() - Complete ===";
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

    if (!validateConnection()) {
        qWarning() << "Connection validation failed during heartbeat";
        // Trigger reconnection logic here
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

bool LaneClient::validateConnection()
{
    if (!m_socket || m_socket->state() != QTcpSocket::ConnectedState) {
        return false;
    }
    
    // FIXED: sendMessage returns void, so we can't return its result
    // Instead, check if we can write to the socket
    QJsonObject ping;
    ping["type"] = "ping";
    ping["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Send the ping message
    sendMessage(ping);
    
    // Return true if socket is still connected after sending
    return m_socket->state() == QTcpSocket::ConnectedState;
}
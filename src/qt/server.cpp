/*
 * Chrome Token Signing Native Host
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "server.h"
#include "qt_host.h" // XXX: circular


#include "Logger.h"
#include <QSslCertificate>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>

#include <QLocalServer>

WSServer::WSServer(QObject *parent):
    QObject(parent),
    srv(new QWebSocketServer(QStringLiteral("Web eID"), QWebSocketServer::SecureMode, this)),
    srv6(new QWebSocketServer(QStringLiteral("Web eID"), QWebSocketServer::SecureMode, this))
{
    quint16 port = 12345; // FIXME: 3 ports to try.
    QSslConfiguration sslConfiguration;
    QFile keyFile(":/app.web-eid.com.key");
    keyFile.open(QIODevice::ReadOnly);
    QSslKey sslKey(&keyFile, QSsl::Rsa, QSsl::Pem);
    keyFile.close();
    sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfiguration.setLocalCertificateChain(QSslCertificate::fromPath(QStringLiteral(":/app.web-eid.com.pem")));
    sslConfiguration.setPrivateKey(sslKey);
    sslConfiguration.setProtocol(QSsl::TlsV1SslV3);

    // Listen on v4 and v6
    srv->setSslConfiguration(sslConfiguration);
    srv6->setSslConfiguration(sslConfiguration);

    if (srv6->listen(QHostAddress::LocalHostIPv6, port)) {
        _log("Server running on %s", qPrintable(srv6->serverUrl().toString()));
        connect(srv6, &QWebSocketServer::newConnection, this, &WSServer::processConnect);
    } else {
        _log("Could not listen on v6 %d", port);
    }

    if (srv->listen(QHostAddress::LocalHost, port)) {
        _log("Server running on %s", qPrintable(srv->serverUrl().toString()));
        connect(srv, &QWebSocketServer::newConnection, this, &WSServer::processConnect);
    } else {
        _log("Could not listen on %d", port);
    }

    // TODO: shared file between app and nm-proxy
    // Set up local server
    QString serverName;
#if defined(Q_OS_MACOS)
    // /tmp/martin-webeid
    serverName = QDir("/tmp").filePath(qgetenv("USER") + "-webeid");
#elif defined(Q_OS_WIN32)
    // \\.\pipe\Martin_Paljak-webeid
    serverName = qgetenv("USERNAME").simplified().replace(" ", "_") + "-webeid";
#elif defined(Q_OS_LINUX)
    // /run/user/1000/webeid-socket
    serverName = QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).filePath("webeid-socket");
#else
    #error "Unsupported platform"
#endif

    local = new QLocalServer(this);
    local->setSocketOptions(QLocalServer::UserAccessOption);

    _log("Listening in %s", qPrintable(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)));
    if (local->listen(serverName)) {
        _log("Listening on %s", qPrintable(local->fullServerName()));
        connect(local, &QLocalServer::newConnection, this, &WSServer::processConnectLocal);
    }
}

void WSServer::processConnectLocal() {
    _log("Connection to local");
    QLocalSocket *socket = local->nextPendingConnection();
    connect(socket, &QLocalSocket::readyRead, [this, socket] {
        _log("Handling data from socket");
        quint32 msgsize = 0;
        if (socket->read((char*)&msgsize, sizeof(msgsize)) == sizeof(msgsize)) {
            _log("Reading  message of %d bytes", msgsize);
            QByteArray msg(int(msgsize), 0);
            if (socket->read(msg.data(), msgsize) == msgsize) {
                _log("Read message of %d bytes", msgsize);
                // Make JSON
                QJsonObject jo = QJsonDocument::fromJson(msg).object();
                QVariantMap json = jo.toVariantMap();
                
                // add to map
                id2localsocket[json["id"].toString()] = socket;
                // re-serialize msg
                QByteArray response =  QJsonDocument::fromVariant(json).toJson();
                _log("Read message: %s", response.constData());
                
                // now call processing
                qobject_cast<QtHost*>(parent())->incoming(jo);
            } else {
                _log("Could not read message");
                socket->abort();
            }
        } else {
            _log("Could not read message size");
            socket->abort();
        }
    });
}


void WSServer::processConnect() {
    QWebSocket *client = srv->nextPendingConnection();
    _log("Connection to %s from %s:%d (%s)", qPrintable(client->requestUrl().toString()), qPrintable(client->peerAddress().toString()), client->peerPort(), qPrintable(client->origin()));
    // TODO: make sure that pairing is authorized
    QUuid uuid = QUuid::createUuid();
    _log("Assigned context ID %s", qPrintable(uuid.toString()));

    connect(client, &QWebSocket::textMessageReceived, this, &WSServer::processIncoming);
    connect(client, &QWebSocket::disconnected, this, &WSServer::processDisconnect);
}

void WSServer::processIncoming(QString message) {
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    _log("Received from %s: \"%s\"", qPrintable(client->origin()), qPrintable(message));
    QJsonObject json = QJsonDocument::fromJson(message.toUtf8()).object();
    json["origin"] = client->origin();
    id2websocket[json["id"].toString()] = client;

    //_log("Identified context: %s", qPrintable(id2socket.keys(client).at(0)));
    // FIXME
    qobject_cast<QtHost*>(parent())->incoming(json);
}

void WSServer::processOutgoing(QVariantMap message) {
    QByteArray response =  QJsonDocument::fromVariant(message).toJson();
    _log("Sending outgoing message %s", response.constData());
    QString msgid = message["id"].toString();
    // Find the socket where to send
    if (id2websocket.contains(msgid)) {
        _log("Sending message to websocket");
        QWebSocket *s = id2websocket.take(msgid);
        s->sendTextMessage(QString(response));
    } else if (id2localsocket.contains(msgid)) {
        _log("Sending message to localsocket");
        QLocalSocket *s = id2localsocket.take(msgid);
        quint32 msgsize = response.size();
        s->write((char *)&msgsize, sizeof(msgsize));
        s->write(response);
    } else {
        _log("Do not know where to send a reply for %s", qPrintable(msgid));
    }     
}

void WSServer::processDisconnect() {
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    _log("Disconnected: %s:%d (%s) %s", qPrintable(client->peerAddress().toString()), client->peerPort(), qPrintable(client->origin()), qPrintable(client->closeReason()));
}

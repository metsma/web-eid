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
    
    QLocalServer *local = new QLocalServer(this);
    local->setSocketOptions(QLocalServer::UserAccessOption);
    _log("Listening in %s", qPrintable(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)));
    if (local->listen("martin-webeid")) {
        _log("Listening on %s", qPrintable(local->fullServerName()));
    }
}

void WSServer::processConnect() {
    QWebSocket *client = srv->nextPendingConnection();
    _log("Connection to %s from %s:%d (%s)", qPrintable(client->requestUrl().toString()), qPrintable(client->peerAddress().toString()), client->peerPort(), qPrintable(client->origin()));
    // TODO: make sure that pairing is authorized
    // FIXME: Add to some list/map
    QUuid uuid = QUuid::createUuid();
    _log("Assigned context ID %s", qPrintable(uuid.toString()));
    connect(client, &QWebSocket::textMessageReceived, this, &WSServer::processMessage);
    connect(client, &QWebSocket::disconnected, this, &WSServer::processDisconnect);
}

void WSServer::processMessage(QString message) {
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    _log("Received from %s: \"%s\"", qPrintable(client->origin()), qPrintable(message));
    QJsonObject json = QJsonDocument::fromJson(message.toUtf8()).object();
    
    
    // FIXME: 
    client->sendTextMessage(QStringLiteral("PONG"));
}

void WSServer::processDisconnect() {
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    _log("Disconnected: %s:%d (%s) %s", qPrintable(client->peerAddress().toString()), client->peerPort(), qPrintable(client->origin()), qPrintable(client->closeReason()));
}

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


WSServer::WSServer(quint16 port, QObject *parent):
    QObject(parent), 
    srv(new QWebSocketServer(QStringLiteral("Web eID"), QWebSocketServer::SecureMode, this))
{

    QSslConfiguration sslConfiguration;
    QFile keyFile(QStringLiteral("localhost.key"));
    keyFile.open(QIODevice::ReadOnly);
    QSslKey sslKey(&keyFile, QSsl::Rsa, QSsl::Pem);
    keyFile.close();
    sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
    //sslConfiguration.setLocalCertificateChain(QSslCertificate::fromPath(QStringLiteral("localhost.pem")));
    sslConfiguration.setLocalCertificate(QSslCertificate::fromPath(QStringLiteral("cert.pem")).at(0));
    sslConfiguration.setPrivateKey(sslKey);
    //sslConfiguration.setProtocol(QSsl::TlsV1SslV3);
    srv->setSslConfiguration(sslConfiguration);
//    QTcpServer *tcp = new QTcpServer();
//    tcp->listen(QHostAddress::LocalHost, 12344);
//    connect(srv, &QTcpServer::newConnection, this, &WSServer::onNewConnection);

    if (srv->listen(QHostAddress::LocalHost, port)) {
        _log("Server running on %s %d", srv->serverUrl().toString().toStdString().c_str(), srv->isListening());
        _log("Presenting cert with %s", srv->sslConfiguration().localCertificate().subjectInfo(QSslCertificate::CommonName).at(0).toStdString().c_str());
        connect(srv, &QWebSocketServer::newConnection, this, &WSServer::onNewConnection);
//        connect(srv, &QWebSocketServer::sslErrors, this, &WSServer::onSslErrors);
        connect(srv, &QWebSocketServer::acceptError, this, &WSServer::acceptError);
        connect(srv, &QWebSocketServer::serverError, this, &WSServer::serverError);
    }
}

void WSServer::acceptError(QAbstractSocket::SocketError socketError) {
    _log("Accept error");
}


void WSServer::serverError(QWebSocketProtocol::CloseCode closeCode) {
    _log("Accept server error");
}
void WSServer::onNewConnection() {
    _log("New connection");
    QWebSocket *socket = srv->nextPendingConnection();
    _log("Accepted connection from %s %s", socket->origin().toStdString().c_str(), socket->peerName().toStdString().c_str());
    connect(socket, &QWebSocket::textMessageReceived, this, &WSServer::processMessage);
}

void WSServer::processMessage(QString message) {
    _log("Received: %s", message.toStdString().c_str());
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    client->sendTextMessage(QStringLiteral("PONG"));
}


void WSServer::onSslErrors(const QList<QSslError> &)
{
    _log("SSL error");
    qDebug() << "Ssl errors occurred";
}
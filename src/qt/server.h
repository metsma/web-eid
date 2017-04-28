/*
 * Web eID app, (C) 2017 Web eID team and contributors
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

#pragma once

#include <QObject>

#include <QtWebSockets/QtWebSockets>
#include <QWebSocketServer>
#include <QLocalServer>

// Handled WebSocket communication
class WSServer: public QObject {
    Q_OBJECT

public:
    WSServer(QObject *parent);

public slots:
    void processConnect();
    void processIncoming(QString message);
    void processDisconnect();
    void processOutgoing(QVariantMap message);

    void processConnectLocal();

private:
    QWebSocketServer *srv;
    QWebSocketServer *srv6; // IPv6
    QLocalServer *local;

    // message id to socket
    QMap<QString, QWebSocket *> id2websocket;
    QMap<QString, QLocalSocket *> id2localsocket;
};


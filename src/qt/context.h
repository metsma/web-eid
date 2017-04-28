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

#include <QWebSocket>
#include <QLocalSocket>
#include <QWidget>

// Handles a browser context, either
// via WebSocket or LocalSocket, which is owns.
// Lives in main thread, is created by server
class WebContext: public QObject {
    Q_OBJECT

public:
    WebContext(QObject *parent, const QString &origin);

public slots:
    void processDisconnect(); // WS or LS disconnects abruptly
    void processIncoming(QString message); // Message received from client
    void processOutgoing(QVariantMap message); // Message sent to client

private:
    // message transport
    QWebSocket *ws;
    QLocalSocket *ls;

    // browser context
    QString msgid;
    QString origin;
    
    // Any running UI widget, associated with the context
    QWidget *ui = nullptr;
};


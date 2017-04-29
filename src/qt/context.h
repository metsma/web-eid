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

#include "internal.h"

#include <QObject>

#include <QWebSocket>
#include <QLocalSocket>
#include <QWidget>
#include <QUuid>
#include <QTimer>


// Handles a browser context, either
// via WebSocket or LocalSocket, which is owns.
// Lives in main thread, is created by server
class WebContext: public QObject {
    Q_OBJECT

public:
    WebContext(QObject *parent, const QString &origin);
    WebContext(QObject *parent, QWebSocket *client);
    WebContext(QObject *parent, QLocalSocket *client);

    const QString id = QUuid::createUuid().toString();
    QString origin; // TODO: access

    QString friendlyOrigin();

    QTimer timer;
public slots:
    void receiveIPC(const InternalMessage &msg);

signals:
    void sendIPC(const InternalMessage &msg);

private:
    void processMessage(const QVariantMap &message); // Message received from client
    void outgoing(QVariantMap message);
    bool terminate();

    // message transport
    QWebSocket *ws = nullptr;
    QLocalSocket *ls = nullptr;

    // browser context
    QString msgid;

    // Any running UI widget, associated with the context
    QWidget *ui = nullptr;

    // functions to be invoced
    InternalMessage authenticate(const QVariantMap &data);
};

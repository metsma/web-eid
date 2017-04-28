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
    WebContext(QObject *parent, QWebSocket *client);
    WebContext(QObject *parent, QLocalSocket *client);


public slots:
    void processMessage(const QVariantMap &message); // Message received from client

private:
    void outgoing(QVariantMap &message);
    bool terminate();

    // message transport
    QWebSocket *ws = nullptr;
    QLocalSocket *ls = nullptr;

    // browser context
    QString msgid;
    QString origin;

    // Any running UI widget, associated with the context
    QWidget *ui = nullptr;
};


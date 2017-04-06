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

#pragma once

#include "Logger.h"

#include <QCoreApplication>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

class InputChecker: public QThread {
    Q_OBJECT

public:
    InputChecker(QObject *parent): QThread(parent) {}

    void run() {
        setTerminationEnabled(true);
        quint32 messageLength = 0;
        _log("Waiting for messages");
        // Here we do busy-sleep
        while (std::cin.read((char*)&messageLength, sizeof(messageLength))) {
            _log("Message size: %u", messageLength);
            if (messageLength > 1024*8) {
                _log("Invalid message size: %u", messageLength);
                // This will result in a properly terminated connection
                return emit messageReceived(QJsonObject({}));
            } else {
                QByteArray msg(int(messageLength), 0);
                std::cin.read(msg.data(), msg.size());
                _log("Message (%u): %s", messageLength, msg.constData());
                QJsonObject json = QJsonDocument::fromJson(msg).object();
                emit messageReceived(json);
            }
        }
        _log("Input reading thread is done.");
        // If input is closed, we quit
        QCoreApplication::exit(0);
    }

signals:
    void messageReceived(const QJsonObject &msg);
};

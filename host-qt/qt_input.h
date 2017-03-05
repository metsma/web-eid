#pragma once

#include "Logger.h"

#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

class InputChecker: public QThread {
    Q_OBJECT

public:
    InputChecker(QObject *parent): QThread(parent) {}

    void run() {
        quint32 messageLength = 0;
        _log("Waiting for messages");
        // Here we do busy-sleep
        while (std::cin.read((char*)&messageLength, sizeof(messageLength))) {
            _log("Message size: %u", messageLength);
            if (messageLength > 1024*8) {
                _log("Invalid message size: %u", messageLength);
                // This will result in a properly terminated connection
                emit messageReceived(QJsonObject({}));
            } else {
                QByteArray msg(int(messageLength), 0);
                std::cin.read(msg.data(), msg.size());
                _log("Message (%u): %s", messageLength, msg.constData());
                QJsonObject json = QJsonDocument::fromJson(msg).object();
                emit messageReceived(json);
            }
        }
        _log("Input reading thread is done.");
    }

signals:
    void messageReceived(const QJsonObject &msg);
};

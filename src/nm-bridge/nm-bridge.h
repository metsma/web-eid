#pragma once

#include "../Logger.h"

#include <QCoreApplication>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QThread>


#ifndef _WIN32
#include <unistd.h>
#endif

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
            QByteArray msg(int(messageLength), 0);
            std::cin.read(msg.data(), msg.size());
            _log("Message (%u): %s", messageLength, msg.constData());
            emit messageReceived(msg);
        }
        _log("Input reading thread is done.");
        // If input is closed, we quit
        QCoreApplication::exit(0);
    }

signals:
    void messageReceived(const QByteArray &msg);
};




class NMProxy: public QCoreApplication
{
    Q_OBJECT

public:
    NMProxy(int &argc, char *argv[]) : QCoreApplication(argc, argv),
        sock(new QLocalSocket(this))
    {
        Logger::setFile("nm-proxy.log");
        _log("Running %s", qPrintable(applicationFilePath()));

        // TODO: figure out the right paths depending on free-form app location
#if defined(Q_OS_MACOS)
        serverApp = "open -b com.web-eid.app";
        // /tmp/martin-webeid
        serverName = QDir("/tmp").filePath(qgetenv("USER") + "-webeid");
#elif defined(Q_OS_WIN32)
        serverApp = QDir("C:/Program Files (x86)/Web eID/Web-eID.exe")

        // \\.\pipe\Martin_Paljak-webeid
        serverName = qgetenv("USERNAME").simplified().replace(" ", "_") + "-webeid";
#elif defined(Q_OS_LINUX)
        serverApp = "/usr/bin/web-eid";

        // /run/user/1000/webeid-socket
        serverName = QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation))).filePath("webeid-socket");
#else
    #error "Unsupported platform"
#endif
        _log("Connecting to %s", qPrintable(serverName));

        // Starting app something something
        connect(sock, &QLocalSocket::connected, this, &NMProxy::connected);

        connect(sock, static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error), [this] (QLocalSocket::LocalSocketError socketError) {
            if (socketError == QLocalSocket::PeerClosedError) {
                _log("Server disconnected");
                // We no like it, try connecting again
                QTimer::singleShot(500, [this] {sock->connectToServer(serverName);});
                return;
            } else if (socketError == QLocalSocket::ConnectionRefusedError) {
                _log("Socket exists but server is not listening");
            } else if (socketError == QLocalSocket::ServerNotFoundError) {
                _log("Socket does not exist");
            }
            if ((socketError == QLocalSocket::ConnectionRefusedError) || (socketError == QLocalSocket::ServerNotFoundError)) {
                // Start the server
                if (!server_started) {
                    // leave some time for startup
                    // TODO: possibly use QFileSystemWatcher ?
                    QTimer::singleShot(1000, [this] {sock->connectToServer(serverName);});
                    // TODO: set working folder
                    if (QProcess::startDetached(serverApp)) {
                        server_started = true;
                    } else {
                        _log("Could not start server");
                    }
                } else {
                    _log("Server has already been started, trying to reconnect");
                    QTimer::singleShot(500, [this] {sock->connectToServer(serverName);});
                }
            } else {
                _log("Connection failed: %d", socketError);
            }
        });

        out.open(stdout, QFile::WriteOnly);
        input = new InputChecker(this);
        connect(input, &InputChecker::messageReceived, this, &NMProxy::messageFromBrowser, Qt::QueuedConnection);

        connect(sock, &QLocalSocket::disconnected, [this] {
            // XXX: on Linux, error() with QLocalSocket::PeerClosedError is also thrown
            _log("Socket disconnected");
        });
        sock->connectToServer(serverName);
    }

public slots:
    void connected() {
        _log("Connected to %s", qPrintable(sock->fullServerName()));
        // Start input reading thread, if not already running
        if (!input->isRunning())
            input->start();

        connect(sock, &QLocalSocket::readyRead, [this] {
            // Data available from app, read message and pass to browser
            _log("Reading from app");
            quint32 msgsize = 0;
            if (sock->read((char*)&msgsize, sizeof(msgsize)) == sizeof(msgsize)) {
                if (msgsize > 8 * 1024) {
                    _log("Bad message size, closing");
                    sock->abort();
                    return;
                }
                QByteArray msg(int(msgsize), 0);
                qint64 readsize = sock->read(msg.data(), msgsize) ;
                if (readsize == msgsize) {
                    _log("Read message of %d bytes", msgsize);
                    // Pass verbatim
                    quint32 responseLength = msg.size();
                    _log("Response(%u) %s", responseLength, msg.constData());
                    out.write((const char*)&responseLength, sizeof(responseLength));
                    out.write(msg);
                    out.flush();
                } else {
                    _log("Could not read message, read %d of %d", readsize, msgsize);
                    sock->abort();
                }
            } else {
                _log("Could not read message size");
                // TODO: shutdown? close? abort?
                sock->abort();
            }
        });
    }

    void messageFromBrowser(const QByteArray &msg) {
        _log("Handling message from browser");
        quint32 responseLength = msg.size();
        // TODO: error handling?
        sock->write((const char*)&responseLength, sizeof(responseLength));
        sock->write(msg);
    }

private:
    void shutdown() {
        input->terminate();
    }
    // We have a single connection to the server app
    QLocalSocket *sock;
    bool server_started = false;
    QString serverName;
    QString serverApp;
    InputChecker *input;
    QFile out;
};
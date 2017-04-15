#pragma once 

#include "../Logger.h"

#include <QCoreApplication>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>
#include <QFile>

#ifndef _WIN32
#include <unistd.h>
#endif


class NMProxy: public QCoreApplication
{
    Q_OBJECT

public:
    NMProxy(int &argc, char *argv[]) : QCoreApplication(argc, argv),
        sock(new QLocalSocket(this))
    {
        _log("Running %s", qPrintable(applicationFilePath()));

        // TODO: possibly play with Figure out the server name
        serverName = "martin-webeid";
        
        // Starting app something something
        connect(sock, &QLocalSocket::connected, this, &NMProxy::connected);

        connect(sock, static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error), [this] (QLocalSocket::LocalSocketError socketError) {
            if (socketError == QLocalSocket::PeerClosedError) {
                printf("Server disconnected\n");
                // We no like it, try connecting again
                QTimer::singleShot(500, [this] {sock->connectToServer(serverName);});
                return;
            } else if (socketError == QLocalSocket::ConnectionRefusedError) {
                printf("Socket exists but server is not listening\n");
            } else if (socketError == QLocalSocket::ServerNotFoundError) {
                printf("Socket does not exist\n");
            }
            if ((socketError == QLocalSocket::ConnectionRefusedError) || (socketError == QLocalSocket::ServerNotFoundError)) {
                // Start the server
                if (!server_started) {
                    // leave some time for startup
                    QTimer::singleShot(500, [this] {sock->connectToServer(serverName);});
                    // TODO: set working folder
                    if (QProcess::startDetached("/home/martin/efforts/hwcrypto/hwcrypto-native/src/Web-eID", QStringList())) {
                        server_started = true;
                    } else {
                        printf("Coult not start server\n");
                    }
                } else {
                    printf("Server has already been started, trying to reconnect\n");
                    QTimer::singleShot(500, [this] {sock->connectToServer(serverName);});
                }
            } else {
                printf("Connection failed: %d\n", socketError);
            }
        });
        sock->connectToServer();
    }

public slots:
    void connected() {
        printf("Connected to %s\n", qPrintable(sock->fullServerName()));
    }

private:
    // We have a single connection to the server app
    QLocalSocket *sock;
    bool server_started = false;
    QString serverName;
};

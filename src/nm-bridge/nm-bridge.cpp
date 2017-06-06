/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "../debuglog.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QStandardPaths>
#include <QJsonDocument>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
// for setting stdio mode
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

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
            QVariantMap json = QJsonDocument::fromJson(msg).toVariant().toMap();
            emit fromBrowser(json);
        }
        _log("Input reading thread is done.");
        // If input is closed, we quit
        QCoreApplication::exit(0);
    }

signals:
    void fromBrowser(const QVariantMap &msg);
};

class NMBridge: public QCoreApplication
{
    Q_OBJECT

public:
    NMBridge(int &argc, char *argv[]): QCoreApplication(argc, argv),
        sock(new QLocalSocket(this))
    {
        Logger::setFile("nm-bridge.log");
        _log("Running %s", qPrintable(applicationFilePath()));
        args = arguments();
        const char *msg = "This is not a regular program, it is expected to be run from a browser.\n";
        // Check if run as a browser extension
        if (args.size() < 2) {
            printf("%s", msg);
            exit(1);
        }

        // Allow to signal quit from command line
        if (!args.contains("--quit")) {
            browser = "unknown";
            if (args.at(1).startsWith("chrome-extension://")) {
                browser = "chrome";
            } else if (QFile::exists(args.at(1))) {
                browser = "firefox";
            }
            // Check that input is a pipe (the app is not run from command line)
            bool isPipe = false;
#ifdef _WIN32
            isPipe = GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_PIPE;
#else
            struct stat sb;
            if (fstat(fileno(stdin), &sb) != 0) {
                exit(1);
            }
            isPipe = S_ISFIFO(sb.st_mode);
#endif
            if (!isPipe) {
                printf("%s", msg);
// FIXME        exit(1);
            }
#ifdef _WIN32
            // Set files to binary mode, to be able to read the uint32 msg size
            _setmode(_fileno(stdin), O_BINARY);
            _setmode(_fileno(stdout), O_BINARY);
#endif
        }

        // TODO: figure out the right paths depending on free-form app location
#if defined(Q_OS_MACOS)
        serverApp = QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath("Web eID"));
        // /tmp/martin-webeid
        serverName = QDir("/tmp").filePath(qgetenv("USER") + "-webeid");
#elif defined(Q_OS_WIN32)
        serverApp = QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath("Web-eID.exe"));

        // \\.\pipe\Martin_Paljak-webeid
        serverName = qgetenv("USERNAME").simplified().replace(" ", "_") + "-webeid";
#elif defined(Q_OS_LINUX)
        serverApp = "/usr/bin/web-eid";

        // /run/user/1000/webeid-socket
        serverName = QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).filePath("webeid-socket");
#else
#error "Unsupported platform"
#endif
        // Override, for testing purposes
        if (qEnvironmentVariableIsSet("WEB_EID_APP"))
            serverApp = qgetenv("WEB_EID_APP");

        _log("Connecting to %s", qPrintable(serverName));
        _log("Running \"%s\" to get server", qPrintable(serverApp));

        // Starting app
        connect(sock, &QLocalSocket::connected, this, &NMBridge::connected);

        connect(sock, static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error), [this] (QLocalSocket::LocalSocketError socketError) {
            if (socketError == QLocalSocket::PeerClosedError) {
                _log("QLocalSocket::PeerClosedError");
                return quit();
            } else if (socketError == QLocalSocket::ConnectionRefusedError) {
                _log("QLocalSocket::ConnectionRefusedError");
            } else if (socketError == QLocalSocket::ServerNotFoundError) {
                _log("QLocalSocket::ServerNotFoundError");
            }
            if ((socketError == QLocalSocket::ConnectionRefusedError) || (socketError == QLocalSocket::ServerNotFoundError)) {
                if (args.contains("--quit")) {
                    _log("Quit, do nothing");
                    // Assume it is not running and quit the native agent
                    _exit(1); // FIXME: quit() hangs on all platforms, aboutToQuit is not called
                    return quit();
                }
                // Start the server
                if (!server_started) {
                    // leave some time for startup
                    // TODO: possibly use QFileSystemWatcher ?
                    QTimer::singleShot(1000, [this] {sock->connectToServer(serverName);});
                    // TODO: set working folder
                    if (QProcess::startDetached(serverApp, args.contains("--debug") ? QStringList("--debug") : QStringList())) {
                        server_started++;
                        _log("Started %s", qPrintable(serverApp));
                    } else {
                        _log("Could not start server");
                    }
                } else {
                    if (server_started < 4) {
                        _log("Server has already been started, trying to reconnect (%d)", server_started);
                        server_started++;
                        QTimer::singleShot(500, [this] {sock->connectToServer(serverName);});
                    }
                }
            } else {
                _log("Connection failed: %d", socketError);
            }
        });

        connect(sock, &QLocalSocket::disconnected, [this] {
            // XXX: on Linux, error() with QLocalSocket::PeerClosedError is also thrown
            _log("Socket disconnected");
            quit();
        });
        connect(this, &QCoreApplication::aboutToQuit, [this] {
            _log("Quitting ...");
            if (out.isOpen())
                out.close();
#ifdef Q_OS_WIN
            input->terminate();
#else
            close(0);
            input->wait();
#endif
        });

        // Try to establish connection to app
        sock->connectToServer(serverName);
    }

public slots:
    void connected() {
        _log("Connected to %s", qPrintable(sock->fullServerName()));

        out.open(stdout, QFile::WriteOnly);
        input = new InputChecker(this);
        connect(input, &InputChecker::fromBrowser, this, &NMBridge::toApp, Qt::QueuedConnection);

        server_started = 0; //FIXME: remove

        connect(sock, &QLocalSocket::readyRead, [this] {
            // Data available from app, read message and pass to browser
            _log("Handling message from application");
            _log("%d bytes available from app", sock->bytesAvailable());
            quint32 msgsize = 0;
            if (sock->bytesAvailable() < qint64(sizeof(msgsize) + 1)) {
                _log("Not enought data available %d, waiting for next update", sock->bytesAvailable());
                return;
            }
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
                    // Pass verbatim from app to browser
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

        // Quit the app
        if (args.contains("--quit")) {
            toApp({{"internal", "quit"}});
            return quit();
        }

        // Start input reading thread, if not already running
        if (!input->isRunning()) {
            input->start();
        }
    }

    void toApp(QVariantMap msg) {
        _log("Handling message from browser");
        // Enrich with information about browser
        msg["browser"] = browser;
        QByteArray json =  QJsonDocument::fromVariant(msg).toJson();
        quint32 msglen = json.size();
        // TODO: error handling?
        sock->write((const char*)&msglen, sizeof(msglen));
        sock->write(json);
        sock->flush();
    }

private:
    // We have a single connection to the server app
    QLocalSocket *sock;
    int server_started = 0;
    QString serverName;
    QString serverApp;
    InputChecker *input;
    QFile out;
    QString browser;
    QStringList args;
};

int main(int argc, char *argv[]) {
    return NMBridge(argc, argv).exec();
}

#include "nm-bridge.moc"

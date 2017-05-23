/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "main.h"

#include "autostart.h"

#include "util.h"
#include "Logger.h" // TODO: rename

#include "dialogs/debug.h"
#include "dialogs/about.h"

#include <QIcon>
#include <QJsonDocument>
#include <QSslCertificate>
#include <QCommandLineParser>
#include <QTranslator>
#include <QUrl>
#include <QString>
#include <QMenu>
#include <QDesktopServices>
#include <QLockFile>
#include <QDir>
#include <QStandardPaths>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <iostream>

#ifdef Q_OS_WIN
// for setting stdio mode
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#ifdef Q_OS_MAC
void nshideapp(bool);
#endif

QtHost::QtHost(int &argc, char *argv[]) : QApplication(argc, argv), PKI(&this->PCSC), tray(this) {

    _log("Starting Web eID app v%s", VERSION);
    QCoreApplication::setOrganizationName("Web eID");
    QCoreApplication::setOrganizationDomain("web-eid.com");
    QCoreApplication::setApplicationName("Web eID");

    QCommandLineParser parser;
    QCommandLineOption debug("debug");
    parser.addOption(debug);
    parser.process(arguments());
    if (parser.isSet(debug)) {
        once = true;
        // TODO: set debug mode
    }

    // Enable autostart, if not explicitly disabled
    QSettings settings;
    if (settings.value("startAtLogin", 1).toBool()) {
        StartAtLoginHelper::setEnabled(true);
    }

    // Construct tray icon and related menu
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    tray.setIcon(QIcon(":/inactive-web-eid.png"));
#else
    tray.setIcon(QIcon(":/web-eid.png"));
#endif
    connect(&tray, &QSystemTrayIcon::activated, this, [this] (QSystemTrayIcon::ActivationReason reason) {
        // TODO: show some generic dialog here.
        autostart->setChecked(StartAtLoginHelper::isEnabled());
        // Construct active sites menu.
        usage->clear();
        usage->setTitle(tr("%1 active site%2").arg(contexts.size()).arg(contexts.size() == 1 ? "" : "s"));
        QMap<QString, int> active;

        for(const auto &c: contexts) {
            QString o = c->friendlyOrigin();
            if (!active.contains(o)) {
                active[o] = 1;
            } else {
                active[o]++;
            }
        }
        if (active.size() > 0 ) {
            QAction *instruction = usage->addAction(tr("Click to terminate"));
            instruction->setEnabled(false);
            usage->addSeparator();
            for (const auto &o: active.keys()) {
                QAction *item;
                if (active[o] > 1) {
                    item = usage->addAction(tr("%1x %2").arg(active[o]).arg(o));
                } else {
                    item = usage->addAction(o);
                }
                // FIXME: terminate with a nice JSON status ?
                connect(item, &QAction::triggered, this, [this, o] {
                    QList<WebContext *> all(contexts.values());
                    for (const auto &c: this->contexts) {
                        if (c->friendlyOrigin() == o) {
                            all.append(c);
                        }
                    }
                    for (const auto &c: all) {
                        c->terminate();
                    }
                });
            }
            usage->addSeparator();
            QAction *kamikaze = usage->addAction(tr("Terminate all"));
            connect(kamikaze, &QAction::triggered, this, [this] {
                QList<WebContext *> all(contexts.values());
                for (const auto &c: all) {
                    c->terminate();
                }
            });
        }
        _log("activated: %d", reason);
    });

    // Context menu
    QMenu *menu = new QMenu();
    // TODO: have about dialog
    QAction *about = menu->addAction(tr("About Web eID"));
    connect(about, &QAction::triggered, this, [=] {
        new AboutDialog();
    });

    // Start at login
    autostart = menu->addAction(tr("Start at Login"));
    autostart->setCheckable(true);
    autostart->setChecked(StartAtLoginHelper::isEnabled());
    connect(autostart, &QAction::toggled, this, [=] (bool checked) {
        _log("Setting start at login to %d", checked);
        QSettings settings;
        settings.setValue("startAtLogin", checked);
        StartAtLoginHelper::setEnabled(checked);
    });

    // Debug menu
    if (parser.isSet(debug)) {
        QAction *dbg = menu->addAction(tr("Debug"));
        connect(dbg, &QAction::triggered, this, [=] {
            new QtDebugDialog(this);
        });
    }

    // Number of active sites
    menu->addSeparator();
    usage = new QMenu(tr("0 active sites"), menu);
    menu->addMenu(usage);
    menu->addSeparator();

    // Quit
    QAction *a2 = menu->addAction(tr("Quit"));
    connect(a2, &QAction::triggered, this, &QApplication::quit);

    // Initialize listening servers
    ws = new QWebSocketServer(QStringLiteral("Web eID"), QWebSocketServer::SecureMode, this);
    ws6 = new QWebSocketServer(QStringLiteral("Web eID"), QWebSocketServer::SecureMode, this);
    ls = new QLocalServer(this);
    quint16 port = 42123; // TODO: 3 ports to try.

    // Now this will probably get some bad publicity ...
    QSslConfiguration sslConfiguration;

    // TODO: make this configurable
    QFile keyFile(":/app.web-eid.com.key");
    keyFile.open(QIODevice::ReadOnly);
    QSslKey sslKey(&keyFile, QSsl::Rsa, QSsl::Pem);
    keyFile.close();

    sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfiguration.setLocalCertificateChain(QSslCertificate::fromPath(QStringLiteral(":/app.web-eid.com.pem")));
    sslConfiguration.setPrivateKey(sslKey);
    sslConfiguration.setProtocol(QSsl::TlsV1SslV3);

    // Listen on v4 and v6
    ws->setSslConfiguration(sslConfiguration);
    ws6->setSslConfiguration(sslConfiguration);

    if (ws6->listen(QHostAddress::LocalHostIPv6, port)) {
        _log("Server running on %s", qPrintable(ws6->serverUrl().toString()));
        connect(ws6, &QWebSocketServer::originAuthenticationRequired, this, &QtHost::checkOrigin);
        connect(ws6, &QWebSocketServer::newConnection, this, &QtHost::processConnect);
    } else {
        _log("Could not listen on v6 %d", port);
    }

    if (ws->listen(QHostAddress::LocalHost, port)) {
        _log("Server running on %s", qPrintable(ws->serverUrl().toString()));
        connect(ws, &QWebSocketServer::originAuthenticationRequired, this, &QtHost::checkOrigin);
        connect(ws, &QWebSocketServer::newConnection, this, &QtHost::processConnect);
    } else {
        _log("Could not listen on %d", port);
    }

    // TODO: shared file between app and nm-proxy
    // Set up local server
    QString serverName;
#if defined(Q_OS_MACOS)
    // /tmp/martin-webeid
    serverName = QDir("/tmp").filePath(qgetenv("USER") + "-webeid");
#elif defined(Q_OS_WIN32)
    // \\.\pipe\Martin_Paljak-webeid
    serverName = qgetenv("USERNAME").simplified().replace(" ", "_") + "-webeid";
#elif defined(Q_OS_LINUX)
    // /run/user/1000/webeid-socket
    serverName = QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).filePath("webeid-socket");
#else
#error "Unsupported platform"
#endif

    ls->setSocketOptions(QLocalServer::UserAccessOption);

    if (ls->listen(serverName)) {
        _log("Listening on %s", qPrintable(ls->fullServerName()));
        connect(ls, &QLocalServer::newConnection, this, &QtHost::processConnectLocal);
    }

    tray.setContextMenu(menu);
    tray.setToolTip(tr("Web eID is running on port %1").arg(12345)); // FIXME
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray.show();
    } else {
        _log("Tray is not (yet) available");
#ifdef Q_OS_LINUX
        QTimer::singleShot(1000, &tray, &QSystemTrayIcon::show);
#endif
    }




    setWindowIcon(QIcon(":/web-eid.png"));
    setQuitOnLastWindowClosed(false);

    // Register slots and signals
    // FRAGILE: registered types and explicit queued connections are necessary to
    // make the Qt signal magic work. Otherwise runtime errors of type "could not serialize type CK_RV" will happen

    qRegisterMetaType<CertificatePurpose>();
    qRegisterMetaType<P11Token>();

    // Start PC/SC event thread
    PCSC.start();

    connect(&PKI, &QPKI::certificateListChanged, [=] (QVector<QByteArray> certs) {
        printf("Certificate list changed, contains %d entries\n", certs.size());
    });

    // TODO: show UI on severe errors
    connect(&PCSC, &QtPCSC::error, [=] (QString reader, LONG err) {
        printf("error in %s %s\n", qPrintable(reader), QtPCSC::errorName(err));
    });

    connect(this, &QApplication::aboutToQuit, [this] {
        _log("About to quit");
        PCSC.cancel();
        PCSC.wait();
        _log("Done");
    });
#ifdef Q_OS_MAC
    // Never grab focus from other apps, even on starting
    nshideapp(true);
#endif
}

void QtHost::checkOrigin(QWebSocketCorsAuthenticator *authenticator) {
    if (WebContext::isSecureOrigin(authenticator->origin())) {
        authenticator->setAllowed(true);
    } else {
        authenticator->setAllowed(false);
    }
}

void QtHost::processConnectLocal() {
    QLocalSocket *socket = ls->nextPendingConnection();
    _log("New connection to local socket");
    WebContext *ctx = new WebContext(this, socket);
    newConnection(ctx);
}

void QtHost::processConnect() {
    QWebSocket *client;
    if (ws->hasPendingConnections()) {
        client = ws->nextPendingConnection();
    } else if (ws6->hasPendingConnections()) {
        client = ws6->nextPendingConnection();
    } else {
        return;
    }
    _log("Connection to %s from %s:%d (%s)", qPrintable(client->requestUrl().toString()), qPrintable(client->peerAddress().toString()), client->peerPort(), qPrintable(client->origin()));
    _log("UA: %s", qPrintable(client->request().header(QNetworkRequest::UserAgentHeader).toString()));
    WebContext *ctx = new WebContext(this, client);
    newConnection(ctx);
}

void QtHost::newConnection(WebContext *ctx) {
    contexts[ctx->id] = ctx;
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    tray.setIcon(QIcon(":/web-eid.png"));
#endif
    // Keep count of active contexts
    tray.setToolTip(tr("%1 active site%2").arg(contexts.size()).arg(contexts.size() == 1 ? "" : "s"));
    usage->setTitle(tr("%1 active site%2").arg(contexts.size()).arg(contexts.size() == 1 ? "" : "s"));
    connect(ctx, &WebContext::disconnected, this, [this, ctx] {
        if (contexts.remove(ctx->id)) {
            tray.setToolTip(tr("%1 active site%2").arg(contexts.size()).arg(contexts.size() == 1 ? "" : "s"));
            usage->setTitle(tr("%1 active site%2").arg(contexts.size()).arg(contexts.size() == 1 ? "" : "s"));
            ctx->deleteLater();
            if (contexts.size() == 0) {
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
                tray.setIcon(QIcon(":/inactive-web-eid.png"));
#endif
                if (once) {
                    _log("Context count is zero, quitting");
                    quit();
                }
            }
        }
    });
}

int main(int argc, char *argv[]) {
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    QString lockfile_folder = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#elif defined (Q_OS_WIN)
    QString lockfile_folder = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#endif
    // Check for too fast startup
    QLockFile lf(QDir(lockfile_folder).filePath("webeid.lock"));
    if (!lf.tryLock(100)) {
        _log("Could not get lockfile");
        exit(1);
    }

    return QtHost(argc, argv).exec();
}

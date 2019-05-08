/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "main.h"

#include "autostart.h"
#include "webextension.h"

//#include "util.h"
#include "debuglog.h"

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

// Do the "magic" obfuscation
static void selgita(QByteArray &bytes) {
    for (int i = 0; i < bytes.size(); i++) {
        bytes[i] = bytes[i] ^ ((42 + i) % 255);
    }
}

QtHost::QtHost(int &argc, char *argv[]) : QApplication(argc, argv), PKI(&this->PCSC), tray(this) {

    _log("Starting Web eID app v%s", VERSION);
    QCoreApplication::setOrganizationName("Web eID");
    QCoreApplication::setOrganizationDomain("web-eid.com");
    QCoreApplication::setApplicationName("Web eID");

    QSettings settings;
    settings.setFallbacksEnabled(false);

    QCommandLineParser parser;
    QCommandLineOption debug("debug");
    parser.addOption(debug);
    parser.process(arguments());
    if (parser.isSet(debug)) {
        once = true;
        settings.setValue("debug", true);
    }

    // On first run, open a welcome page
    if (settings.value("firstRun", true).toBool()) {
        QDesktopServices::openUrl(QUrl(settings.value("welcomeUrl", "https://web-eid.com/welcome").toString()));
        settings.setValue("firstRun", false);
    }

    // Enable autostart, if not explicitly disabled
    if (settings.value("startAtLogin", true).toBool()) {
        // We always overwrite
        StartAtLoginHelper::setEnabled(true);
    }

    // Register extension, if not explicitly disabled
    if (settings.value("registerExtension", true).toBool()) {
        WebExtensionHelper::setEnabled(true);
    }

    // Construct tray icon and related menu
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    QIcon trayicon(":/inactive-web-eid.svg");
    trayicon.setIsMask(true);
    tray.setIcon(trayicon);
#else
    // Windows hides the icon and w10 has a dark bacground by default
    tray.setIcon(QIcon(":/web-eid.svg"));
#endif
    connect(&tray, &QSystemTrayIcon::activated, this, [this] (QSystemTrayIcon::ActivationReason reason) {
        QSettings settings;

        autostart->setChecked(StartAtLoginHelper::isEnabled());
        debugMenu->menuAction()->setVisible(settings.value("debug").toBool());
        debugEnabled->setChecked(settings.value("debug").toBool());
        debugLogEnabled->setChecked(QFile(Logger::getLogFilePath()).exists());
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
            usage->menuAction()->setVisible(true);
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
        } else {
            usage->menuAction()->setVisible(false);
        }
        _log("activated: %d", reason);
    });

    // Context menu
    QMenu *menu = new QMenu();
    // About dialog, that also enables debug menu
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
    debugMenu = new QMenu(tr("Debug"), menu);
    menu->addMenu(debugMenu);

    debugEnabled = debugMenu->addAction(tr("Debug menu enabled"));
    debugEnabled->setCheckable(true);
    debugEnabled->setChecked(true);
    connect(debugEnabled, &QAction::toggled, this, [=] (bool checked) {
        QSettings settings;
        settings.setValue("debug", checked);
    });

    debugLogEnabled = debugMenu->addAction(tr("Debug logging enabled"));
    debugLogEnabled->setCheckable(true);
    debugLogEnabled->setChecked(Logger::isEnabled());
    connect(debugLogEnabled, &QAction::toggled, this, [=] (bool checked) {
        if (checked) {
            // create file.
            QFile logfile(Logger::getLogFilePath());
            logfile.open(QIODevice::WriteOnly | QIODevice::Text);
        }
    });
    QAction *viewLog = debugMenu->addAction(tr("View log"));
    connect(viewLog, &QAction::triggered, this, [=] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Logger::getLogFilePath()));
    });

    softCertEnabled = debugMenu->addAction(tr("Enable softcerts"));
    softCertEnabled->setCheckable(true);
    softCertEnabled->setChecked(settings.value("softCert", false).toBool());
    connect(softCertEnabled, &QAction::toggled, this, [=] (bool checked) {
        QSettings settings;
        settings.setValue("softCert", checked);
    });

#ifdef Q_OS_WIN
    ownDialogsEnabled = debugMenu->addAction(tr("Use own certificate dialogs"));
    ownDialogsEnabled->setCheckable(true);
    ownDialogsEnabled->setChecked(settings.value("ownDialogs", false).toBool());
    connect(ownDialogsEnabled, &QAction::toggled, this, [=] (bool checked) {
        QSettings settings;
        settings.setValue("ownDialogs", checked);
    });
#endif


    QAction *websocket = debugMenu->addAction(tr("WebSocket enabled"));
    websocket->setCheckable(true);
    websocket->setChecked(true);
    connect(websocket, &QAction::toggled, this, [=] (bool checked) {
        // XXX this is redundant if no accepts are done
        wsEnabled = checked;
        if (wsEnabled) {
            ws->pauseAccepting();
            ws6->pauseAccepting();
        } else {
            ws->resumeAccepting();
            ws6->resumeAccepting();
        }
    });

    QAction *localsocket = debugMenu->addAction(tr("WebExtension enabled"));
    localsocket->setCheckable(true);
    localsocket->setChecked(true);
    connect(localsocket, &QAction::toggled, this, [=] (bool checked) {
        lsEnabled = checked;
    });

    QAction *native = debugMenu->addAction(tr("WebExtension registered"));
    native->setCheckable(true);
    native->setChecked(WebExtensionHelper::isEnabled());
    connect(native, &QAction::toggled, this, [=] (bool checked) {
        WebExtensionHelper::setEnabled(checked);
    });
    QAction *clearsettings = debugMenu->addAction(tr("Clear settings"));
    connect(clearsettings, &QAction::triggered, this, [=] {
        QSettings settings;
        settings.clear();
    });

    // Number of active sites, visible only if > 0
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
    QFile keyFile(settings.value("teenusevoti", ":/teenusevoti").toString());
    keyFile.open(QIODevice::ReadOnly);
    QByteArray privkeyBytes = keyFile.readAll();
    if (keyFile.fileName().startsWith(":/")) {
        selgita(privkeyBytes);
    }
    QSslKey sslKey(privkeyBytes, QSsl::Rsa, QSsl::Pem);
    keyFile.close();
    if (sslKey.isNull()) {
        _log("NULL :(");
    }
    QFile certFile(settings.value("teenusetunnus", ":/teenusetunnus").toString());
    certFile.open(QIODevice::ReadOnly);
    QByteArray certBytes = certFile.readAll();
    if (certFile.fileName().startsWith(":/")) {
        selgita(certBytes);
    }
    QList<QSslCertificate> serviceCerts = QSslCertificate::fromData(certBytes);

    // Notify 14 days before cert expires
    if (QDateTime::currentDateTime().addDays(14) >= serviceCerts.at(0).expiryDate()) {
        QDesktopServices::openUrl(QUrl(settings.value("welcomeUrl", "https://web-eid.com/app/?expires=" + serviceCerts.at(0).expiryDate().toString(Qt::RFC2822Date)).toString()));
    }

    sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfiguration.setLocalCertificateChain(serviceCerts);
    sslConfiguration.setPrivateKey(sslKey);
    sslConfiguration.setProtocol(QSsl::TlsV1SslV3);

    // Listen on v4 and v6
    ws->setSslConfiguration(sslConfiguration);
    ws6->setSslConfiguration(sslConfiguration);
    QString serverUrlDescription;

    if (ws6->listen(QHostAddress::LocalHostIPv6, port)) {
        serverUrlDescription = ws6->serverUrl().toString();
        _log("Server running on %s", qPrintable(serverUrlDescription));
        connect(ws6, &QWebSocketServer::originAuthenticationRequired, this, &QtHost::checkOrigin);
        connect(ws6, &QWebSocketServer::newConnection, this, &QtHost::processConnect);
    } else {
        _log("Could not listen on v6 %d", port);
    }

    if (ws->listen(QHostAddress::LocalHost, port)) {
        serverUrlDescription = ws->serverUrl().toString();
        _log("Server running on %s", qPrintable(serverUrlDescription));
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
    tray.setToolTip(tr("Web eID is running %1").arg(serverUrlDescription));
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray.show();
    } else {
        _log("Tray is not (yet) available");
#ifdef Q_OS_LINUX
        // Practically happen only on Linux where all session items are started at once
        // Show the icon after 1 second.
        QTimer::singleShot(1000, &tray, &QSystemTrayIcon::show);
#endif
    }

    setWindowIcon(QIcon(":/web-eid.svg"));
    setQuitOnLastWindowClosed(false);

    // Register slots and signals
    // FRAGILE: registered types and explicit queued connections are necessary to
    // make the Qt signal magic work. Otherwise runtime errors of type "could not serialize type CK_RV" will happen

    qRegisterMetaType<CertificatePurpose>();
    qRegisterMetaType<P11Token>();

    connect(&PKI, &QPKI::certificateListChanged, [=] (QVector<QByteArray> certs) {
        printf("Certificate list changed, contains %d entries\n", certs.size());
    });

    connect(this, &QApplication::aboutToQuit, [this] {
        _log("About to quit");
        // Destructor cancels if necessary PCSC.cancel();
        _log("Done");
    });
#ifdef Q_OS_MAC
    // Never grab focus from other apps, even on starting
    nshideapp(true);
#endif
    for (auto &x: settings.childGroups()) {
        _log("Settings group: %s", qPrintable(x));
    }
    for (auto &x: settings.childKeys()) {
        _log("Settings key: %s", qPrintable(x));
    }

}

// We allo websocket connections only from secure origins
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
    if (!lsEnabled)
        return socket->abort();
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
    if (!wsEnabled) {
        return client->close(QWebSocketProtocol::CloseCodePolicyViolated);
    }
    WebContext *ctx = new WebContext(this, client);
    newConnection(ctx);
}

void QtHost::newConnection(WebContext *ctx) {
    contexts[ctx->id] = ctx; // FIXME: have pointers instead
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    // Activate the system icon
    tray.setIcon(QIcon(":/web-eid.svg"));
#endif
    // Keep count of active contexts XXX l10n
    tray.setToolTip(tr("%1 active %2").arg(contexts.size()).arg(contexts.size() == 1 ? tr("site") : tr("sites")));
    usage->setTitle(tr("%1 active %2").arg(contexts.size()).arg(contexts.size() == 1 ? tr("site") : tr("sites")));
    usage->menuAction()->setVisible(true);
    connect(ctx, &WebContext::disconnected, this, [this, ctx] {
        if (contexts.remove(ctx->id)) {
            tray.setToolTip(tr("%1 active %2").arg(contexts.size()).arg(contexts.size() == 1 ? tr("site") : tr("sites")));
            usage->setTitle(tr("%1 active %2").arg(contexts.size()).arg(contexts.size() == 1 ? tr("site") : tr("sites")));
            ctx->deleteLater();
            if (contexts.size() == 0) {
                usage->menuAction()->setVisible(false);
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
                QIcon icon = QIcon(":/inactive-web-eid.svg");
                icon.setIsMask(true); // So that inverted colors would work on OSX
                tray.setIcon(icon);
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
    // web-eid-bridge starts the app, have a simple lockfile to avoid launching several instances
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

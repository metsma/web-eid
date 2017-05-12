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

#include "main.h"

#include "autostart.h"
#include "qt/qt_pki.h"

#include "qt/dialogs/select_reader.h"
#include "util.h"
#include "Logger.h" // TODO: rename

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

#ifdef _WIN32
// for setting stdio mode
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

QtHost::QtHost(int &argc, char *argv[]) : QApplication(argc, argv), tray(this) {

    _log("Starting Web eID app v%s", VERSION);

    QCommandLineParser parser;
    QCommandLineOption debug("debug");
    parser.addOption(debug);
    parser.process(arguments());
    if (parser.isSet(debug)) {
        once = true;
        // TODO: set debug mode
    }

    // Construct tray icon and related menu
    tray.setIcon(QIcon(":/web-eid.png"));
    connect(&tray, &QSystemTrayIcon::activated, [&] (QSystemTrayIcon::ActivationReason reason) {
        // TODO: show some generic dialog here.
        _log("activated: %d", reason);
    });

    // Context menu
    QMenu *menu = new QMenu();
    QAction *about = menu->addAction("About");
    connect(about, &QAction::triggered, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://web-eid.com")));
    });
    QAction *a1 = menu->addAction("Start at login");
    a1->setCheckable(true);
    a1->setChecked(StartAtLoginHelper::isEnabled());
    connect(a1, &QAction::toggled, [] (bool checked) {
        _log("Setting start at login to %d", checked);
        StartAtLoginHelper::setEnabled(checked);
    });

    QAction *a2 = menu->addAction("Quit");
    connect(a2, &QAction::triggered, [&] {
        shutdown(0);
    });

    // Initialize listening servers
    ws = new QWebSocketServer(QStringLiteral("Web eID"), QWebSocketServer::SecureMode, this);
    ws6 = new QWebSocketServer(QStringLiteral("Web eID"), QWebSocketServer::SecureMode, this);
    ls = new QLocalServer(this);

    // Set up listening

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
        connect(ws6, &QWebSocketServer::newConnection, this, &QtHost::processConnect);
    } else {
        _log("Could not listen on v6 %d", port);
    }

    if (ws->listen(QHostAddress::LocalHost, port)) {
        _log("Server running on %s", qPrintable(ws->serverUrl().toString()));
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

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray.setContextMenu(menu);
        tray.setToolTip(tr("Web eID is running on port %1").arg(12345));
        tray.show();
    }
    //tray.showMessage(tr("Web eID started"), tr("Click the icon for more information"), QSystemTrayIcon::Information, 2000); // Show message for 2 seconds

    setWindowIcon(QIcon(":/web-eid.png"));
    setQuitOnLastWindowClosed(false);

    // Register slots and signals
    // FRAGILE: registered types and explicit queued connections are necessary to
    // make the Qt signal magic work. Otherwise runtime errors of type "could not serialize type CK_RV" will happen

    qRegisterMetaType<CertificatePurpose>();
    qRegisterMetaType<P11Token>();
    qRegisterMetaType<InternalMessage>();

    connect(this, &QtHost::toPKI, &PKI, &QPKI::receiveIPC, Qt::QueuedConnection);
    connect(&PKI, &QPKI::sendIPC, this, &QtHost::receiveIPC, Qt::QueuedConnection);

    // Start worker threads
    pki_thread = new QThread;
    pki_thread->start();
    PKI.moveToThread(pki_thread);

    // Start PC/SC event thread
    PCSC.start();

    // Refresh PKI tokens when a card is inserted
    //connect(&PCSC, &QtPCSC::cardInserted, &PKI, &QPKI::cardInserted, Qt::QueuedConnection);
    //connect(&PCSC, &QtPCSC::cardRemoved, &PKI, &QPKI::cardRemoved, Qt::QueuedConnection);

    // Executed in pki thread
    connect(&PKI, &QPKI::certificateListChanged, [=] (QVector<QByteArray> certs) {
        _log("Certificate list changed, contains %d entries\n", certs.size());
    });

    // TODO: show UI on severe errors
    connect(&PCSC, &QtPCSC::error, [=] (QString reader, LONG err) {
        _log("error in %s %s\n", qPrintable(reader), QtPCSC::errorName(err));
    });

}


void QtHost::processConnectLocal() {
    QLocalSocket *socket = ls->nextPendingConnection();
    _log("New connection to local socket");
    WebContext *ctx = new WebContext(this, socket);
    newConnection(ctx);
}

void QtHost::processConnect() {
    QWebSocket *client = ws->nextPendingConnection(); // FIXME: v6 vs v4
    _log("Connection to %s from %s:%d (%s)", qPrintable(client->requestUrl().toString()), qPrintable(client->peerAddress().toString()), client->peerPort(), qPrintable(client->origin()));
    WebContext *ctx = new WebContext(this, client);
    newConnection(ctx);
}

void QtHost::newConnection(WebContext *ctx) {
    contexts[ctx->id] = ctx;
    connect(ctx, &WebContext::disconnected, [this, ctx] {
        contexts.remove(ctx->id);
        ctx->deleteLater();
        if (contexts.size() == 0) {
            shutdown(0);
        }
    });

    // When context needs something from PKI or PCSC, dispatch through this
    connect(ctx, &WebContext::sendIPC, this, &QtHost::dispatchIPC, Qt::DirectConnection);
}

// Invoked from another thread (PKI, PCSC)
void QtHost::receiveIPC(InternalMessage message) {
    // TODO: handle disconnected context
    WebContext *ctx = contexts[message.data["id"].toString()];

    // Handle dialog requests
    if (message.type == ShowSelectCertificate) {
        _log("Showing certificate selection window");
        QtSelectCertificate *dialog = new QtSelectCertificate(ctx, Authentication);
        // Signal result back to PKI
        connect(dialog, &QtSelectCertificate::sendIPC, &PKI, &QPKI::receiveIPC, Qt::QueuedConnection);
        // Timeout the dialog, if present
        if (ctx->timer.isActive()) {
            connect(&ctx->timer, &QTimer::timeout, dialog, &QDialog::reject);
        }
        return;
    } else {
        // Dispatch to context
        ctx->receiveIPC(message);
    }
}


void QtHost::dispatchIPC(const InternalMessage &message) {
    // Context we are working from
    WebContext *ctx = qobject_cast<WebContext *>(sender());
    _log("Dispatching for %s", qPrintable(ctx->id));

    // Make a copy
    InternalMessage m = message;
    // Send the context id as a simple prameter
    m.data["id"] = ctx->id;

    switch (m.type) {
    case MessageType::Authenticate:
        return emit toPKI(m);
    case MessageType::CardConnect:
//        ctx->dialog = new QtSelectReader(ctx); // FIXME
//        ((QtSelectReader *)ctx->dialog)->update(PCSC.getReaders());
//        connect(&PCSC, &QtPCSC::readerListChanged, (QtSelectReader *)ctx->dialog, &QtSelectReader::update, Qt::QueuedConnection);
//        connect(&PCSC, &QtPCSC::cardInserted, (QtSelectReader *)ctx->dialog, &QtSelectReader::inserted, Qt::QueuedConnection);
//
//        connect((QDialog *)ctx->dialog, &QDialog::rejected, [=] {
//            ctx->receiveIPC({CardConnect, {{"error", "SCARD_E_CANCELLED"}}});
//        });
        return;
    default:
        _log("Don't know how to process message");
    }

    // Depending on message type, we send messages to other threads from here.
    // Other thread emtis a message and we process it in receiveIPC and
    // call directly the public slot of the context, that keeps state

    // TODO: document message signatures and necessary enum-s

}

void QtHost::shutdown(int exitcode) {
    _log("Exiting with %d", exitcode);
    pki_thread->exit(0);
    pki_thread->wait();

    // Send SCardCancel
    PCSC.cancel();
    PCSC.wait();
    exit(exitcode);
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

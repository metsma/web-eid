/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "pkcs11module.h"
#include "qpki.h"
#include "context.h"

// Dialogs
#include "dialogs/select_cert.h"

#include <QApplication>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QFile>
#include <QVariantMap>
#include <QJsonObject>

#include <QtWebSockets/QtWebSockets>
#include <QWebSocketServer>
#include <QLocalServer>

#ifdef _WIN32
#include <qt_windows.h>
#endif

class QtPCSC;

Q_DECLARE_METATYPE(CertificatePurpose)
Q_DECLARE_METATYPE(P11Token)

class QtHost: public QApplication
{
    Q_OBJECT

public:
    QtHost(int &argc, char *argv[]);

    // PCSC and PKI subsystems
    QtPCSC PCSC;
    QPKI PKI;

public slots:
    // connect new clients
    void processConnect();
    void processConnectLocal();

    void checkOrigin(QWebSocketCorsAuthenticator *authenticator);

private:
    void newConnection(WebContext *ctx);
    QSystemTrayIcon tray;
    QAction *autostart;
    QMenu *usage;

    QWebSocketServer *ws; // IPv4
    QWebSocketServer *ws6; // IPv6
    QLocalServer *ls; // localsocket

    // Active contexts
    QMap<QString, WebContext *> contexts;

    QTranslator translator;
    bool once = false;
};

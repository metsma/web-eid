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

#include "pkcs11module.h"
#include "context.h"

#include "qt_pcsc.h"
#include "qt_pki.h"

#include "internal.h"

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

Q_DECLARE_METATYPE(CertificatePurpose)
Q_DECLARE_METATYPE(P11Token)
Q_DECLARE_METATYPE(InternalMessage)

class QtHost: public QApplication
{
    Q_OBJECT

public:
    QtHost(int &argc, char *argv[]);

    static QString friendlyOrigin(const QString &origin);

    // TODO: It is currently assumed that all invocations from one origin
    // go to the same PKCS#11 module
//    PKCS11Module pkcs11;

    // And the chosen signing certificate can not change either
    // Only with a new cert message
    std::vector<unsigned char> signcert;

    // PCSC and PKI subsystems
    QtPCSC PCSC;
    QtPKI PKI;

    // both in a separate thread
    QThread *pcsc_thread;
    QThread *pki_thread;

public slots:
    void processConnect();
    void processConnectLocal();

    // Called when a message is to be sent back to the browser
    void outgoing(const QVariantMap &resp);

    // PKI
    void sign_done(const CK_RV status, const QByteArray &signature);
    void authentication_done(const CK_RV status, const QString &token);
    void select_certificate_done(const CK_RV status, const QByteArray &certificate);

    void show_cert_select(const QString origin, std::vector<std::vector<unsigned char>> certs, CertificatePurpose purpose);
    void show_pin_dialog(const CK_RV last, P11Token token, QByteArray cert, CertificatePurpose purpose);
    void hide_pin_dialog();

    // PCSC
    void reader_connected(LONG status, const QString &reader, const QString &protocol, const QByteArray &atr);
    void apdu_sent(LONG status, const QByteArray &response);
    void reader_disconnected();

    void show_insert_card(bool show, const QString &name, const SCARDCONTEXT ctx);
    void show_select_reader(const QString &protocol);
    void cancel_insert(const SCARDCONTEXT ctx); // TODO: move to PCSC and call directly from dialog

    // From different threads and subsystems, dispatched to contexts
    void receiveIPC(InternalMessage message);

    // From contexts, in same thread, direct connection
    void dispatchIPC(const InternalMessage &message);

signals:
    void authenticate(const QString &origin, const QString &nonce);
    void sign(const QString &origin, const QByteArray &cert, const QByteArray &hash, const QString &hashalgo);
    void select_certificate(const QString &origin, CertificatePurpose purpose, bool silent);

    void login(const QString &pin, CertificatePurpose purpose);

    void connect_reader(const QString &protocol);
    void send_apdu(const QByteArray &apdu);
    void disconnect_reader();

    void toPKI(InternalMessage message);
    void toPCSC(InternalMessage message);

private:
    QSystemTrayIcon tray;

    QWebSocketServer *ws; // IPv4
    QWebSocketServer *ws6; // IPv6
    QLocalServer *ls; // localsocket

    // Active contexts
    QMap<QString, WebContext *> contexts;

    void shutdown(int exitcode);
    QTranslator translator;
};

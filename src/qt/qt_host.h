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

#include "pkcs11module.h"
#include "qt_input.h"
#include "qt_pcsc.h"
#include "qt_pki.h"

#include <QApplication>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QFile>
#include <QVariantMap>
#include <QJsonObject>

#ifdef _WIN32
#include <qt_windows.h>
#endif

Q_DECLARE_METATYPE(CertificatePurpose)
Q_DECLARE_METATYPE(P11Token)

class QtHost: public QApplication
{
    Q_OBJECT

public:
    QtHost(int &argc, char *argv[], bool standalone);

    // TODO: It is currently assumed that all invocations from one origin
    // go to the same PKCS#11 module
    PKCS11Module pkcs11;

    // Thus the origin can not change, once set
    QString origin;

    // Friently origin is something that can be shown to the user
    QString friendly_origin;

    // And the chosen signing certificate can not change either
    // Only with a new cert message
    std::vector<unsigned char> signcert;

    // We keep a flag around that show if the selected cert is from CAPI
    bool winsign = false;

    // PCSC and PKI subsystems
    QtPCSC PCSC;
    QtPKI PKI;

    // both in a separate thread
    QThread *pcsc_thread;
    QThread *pki_thread;

public slots:
    // Called when a message has been received from the
    // browser, using the Qt signaling mechanism
    void incoming(const QJsonObject &json);

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

signals:
    void authenticate(const QString &origin, const QString &nonce);
    void sign(const QString &origin, const QByteArray &cert, const QByteArray &hash, const QString &hashalgo);
    void select_certificate(const QString &origin, CertificatePurpose purpose, bool silent);

    void login(const QString &pin, CertificatePurpose purpose);

    void connect_reader(const QString &protocol);
    void send_apdu(const QByteArray &apdu);
    void disconnect_reader();

private:
    QString msgid; // If a message is being processed, set to ID
    QSystemTrayIcon tray;

    QFile out;
    void write(QVariantMap &resp);
    void shutdown(int exitcode);
    InputChecker *input;

    QTranslator translator;
};

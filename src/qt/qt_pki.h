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

#include <QObject>

#include "pkcs11module.h"

#include "dialogs/select_cert.h"
#include "dialogs/pin.h"

#include <vector>

class QtPKI: public QObject {
    Q_OBJECT

public:
    QtCertSelect select_dialog;
    QtPINDialog pin_dialog;

    static const char *errorName(const CK_RV err);

public slots:
    void authenticate(const QString &origin, const QString &nonce);
    void sign(const QString &origin, const QByteArray &cert, const QByteArray &hash, const QString &hashalgo);
    void select_certificate(const QString &origin, CertificatePurpose purpose, bool silent);

    void cert_selected(const CK_RV status, const QByteArray &cert, CertificatePurpose purpose);


    void login(const CK_RV status, const QString &pin, CertificatePurpose purpose);
    void pkcs11_sign(const CK_RV status);

    void receiveIPC(const QVariantMap &message);

private:
    void authenticate_with(const CK_RV status, const QByteArray &cert);

    void start_signature(const QByteArray &cert, const QByteArray &hash, const QString &hashalgo, CertificatePurpose purpose);
    void finish_signature(const CK_RV status, const QByteArray &signature);


signals:
    void sign_done(const CK_RV status, const QByteArray &signature);
    void authentication_done(const CK_RV status, const QString &token);
    void select_certificate_done(const CK_RV status, const QByteArray &certificate);

    void show_cert_select(const QString origin, std::vector<std::vector<unsigned char>> certs, CertificatePurpose purpose);
    void show_pin_dialog(const CK_RV last, P11Token token, const QByteArray &cert, CertificatePurpose purpose);
    void hide_pin_dialog();

    void sendIPC(const QVariantMap &message);

public:
    void clear() {
        cert.clear();
        hash.clear();
        hashalgo.clear();
        origin.clear();
        nonce.clear();
        purpose = UnknownPurpose;
    }

private:
    // Valid for whole session
    PKCS11Module pkcs11;

    static QByteArray authenticate_dtbs(const QSslCertificate &cert, const QString &origin, const QString &nonce);

    // Valid for a single transaction
    QByteArray cert;
    QByteArray hash;
    QString hashalgo; // FIXME: enum
    CertificatePurpose purpose;
    // Authentication
    QString origin;
    QString nonce;
    QByteArray jwt_token;
};

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

#include "authenticate.h"

#include "qt_signer.h"
#include "qt_certselect.h"
#include "util.h"
#include "Logger.h"
#include "pcsc.h"
#include "modulemap.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>

#ifdef _WIN32
#include "WinCertSelect.h"
#include "WinSigner.h"
#endif

QVariantMap Authenticate::authenticate(QtHost *h, const QJsonObject &msg) {
    // Check for mandatory parameters
    if (!msg.contains("nonce")) {
        return {{"result", "invalid_argument"}};
    }

    // Get the list of connected cards
    std::vector<std::vector<unsigned char>> atrs = PCSC::atrList(false);

    // Check if we have a known PKCS#11 module for any of the cards
    std::vector<std::string> modules = P11Modules::getPaths(atrs);

    // Try to locate a usable certificate
    std::vector<unsigned char> cert;

    // If a PKCS#11 module was found for the card(s), we use it
    if (!modules.empty()) {
        h->pkcs11.load(modules[0]); // FIXME: multiple modules/module handler
        if (!h->pkcs11.isLoaded()) {
            return {{"result", "technical_error"}}; // FIXME: TBS
        }
        // Get authentication certificates in the module
        auto certs = h->pkcs11.getCerts(Authentication);
        if (certs.size() == 1) {
            // XXX: QtCertSelect::getCert validates that cert is valid, here not
            cert = certs.at(0);
        } else if (certs.size() > 1) {
            // Show a certificate selection window.
            cert = QtCertSelect::getCert(certs, h->friendly_origin, Authentication);
        }
    }
    bool wincert = false;
#ifdef _WIN32
    // On Windows, if PKCS1#11 did not provide a certificate, we try certstore
    if (cert.empty()) {
        // XXX: wording
        cert = WinCertSelect::getCert(CertificatePurpose::Authentication, LPWSTR(tr("Authenticating on %1, please select certificate").arg(h->friendly_origin).utf16()));
        if (!cert.empty()) {
            wincert = true;
        } else {
            // XXX: breaks unified flow
            return {{"result", "user_cancel"}}; // FIXME: exception? or a constant/enum
        }
    }
#endif
    // If no certificate, return the information
    if (cert.empty()) {
        return {{"result", "no_certificates"}}; // XXX: see 6 lines above
    }
    QSslCertificate x509 = v2cert(cert);
    // FIXME: check length
    _log("Found certificate: %s", x509.subjectInfo(QSslCertificate::CommonName).at(0).toStdString().c_str());
    // Get the first part of the token
    QByteArray dtbs = authenticate_dtbs(x509, msg.value("origin").toString(), msg.value("nonce").toString());
    // Calculate the hash to be signed
    QByteArray hash = QCryptographicHash::hash(dtbs, QCryptographicHash::Sha256);
    // 2. Sign the token hash with the selected certificate
    QByteArray signature;
    if (wincert) {
        // XXX: never true on non-win32
#ifdef _WIN32
        signature = v2ba(WinSigner::sign(ba2v(hash), cert));
#endif
    } else {
        signature = v2ba(QtSigner::sign(h->pkcs11, ba2v(hash), cert, h->friendly_origin, Authentication));
    }
    // 3. Construct the JWT token to be returned
    QByteArray jwt = dtbs + "." + signature.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    // 4. profit
    return {{"token", jwt.data()}};
}

QByteArray Authenticate::authenticate_dtbs(const QSslCertificate &cert, const QString &origin, const QString &nonce) {
    // Construct the data to be signed
    _log("Constructing JWT for %s", cert.subjectInfo(QSslCertificate::CommonName).at(0).toStdString().c_str());

    // Header
    QJsonDocument header_map({
        {"alg", "RS256"}, // TODO: ES256 as well
        {"typ", "JWT"},
        // XXX: Qt 5.5 fails with the following, x5c will be null
        {"x5c", QJsonArray({ QString(cert.toDer().toBase64()) })},
    });
    QByteArray header_json = header_map.toJson(QJsonDocument::Compact);
    QByteArray header = header_json.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    _log("JWT header: %s", header_map.toJson().toStdString().c_str());

    // Payload
    QJsonDocument payload_map({
        {"exp", int(QDateTime::currentDateTimeUtc().toTime_t() + 5*60)}, // TODO: TBS, expires in 5 minutes
        {"iat", int(QDateTime::currentDateTimeUtc().toTime_t())},
        {"aud", origin},
        {"iss", cert.issuerInfo(QSslCertificate::CommonName).at(0)}, // TODO: TBS
        {"sub", cert.subjectInfo(QSslCertificate::CommonName).at(0)}, // TODO: TBS
        {"nonce", nonce},
    });

    QByteArray payload_json = payload_map.toJson(QJsonDocument::Compact);
    QByteArray payload = payload_json.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    _log("JWT payload: %s", payload_map.toJson().toStdString().c_str());

    // calculate DTBS (Data To Be Signed)
    QByteArray dtbs = header + "." + payload;

    return dtbs;
}

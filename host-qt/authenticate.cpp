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
    if (!msg.contains("auth_nonce")) {
        return {{"result", "invalid_argument"}};
    } else {
        // Get the list of connected cards
        std::vector<std::vector<unsigned char>> atrs = PCSC::atrList(false);
        // Check if we have a known PKCS#11 module for any of the cards
        std::vector<std::string> modules = P11Modules::getPaths(atrs);
        if (modules.empty()) {
            // No modules. On Windows we will see if windows knows any
#ifdef _WIN32
            _log("No PKCS#11 modules defined, checking windows store");
            std::vector<unsigned char> ac = WinCertSelect::getCert();
            if (!ac.empty()) {
                QSslCertificate x509 = v2cert(ac);
                _log("Found certificate: %s", x509.subjectInfo(QSslCertificate::CommonName).at(0).toStdString().c_str());

                // Get the first part of the token
                QByteArray dtbs = authenticate_dtbs(x509, msg.value("origin").toString(), msg.value("auth_nonce").toString());
                // Calculate the hash to be signed
                QByteArray hash = QCryptographicHash::hash(dtbs, QCryptographicHash::Sha256);
                // 2. Sign the token hash with the selected certificate
                QByteArray signature = v2ba(WinSigner::sign(ba2v(hash), ac));
                // 3. Construct the JWT token to be returned
                QByteArray jwt = dtbs + "." + signature.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
                // 4. profit
                return {{"auth_token", jwt.data()}};
            } else {
                return {{"result", "no_certificates"}};
            }
#else
            // On Unix, a PKCS#11 module must be present
            return {{"result", "no_certificates"}};
#endif
        } else {
            // PKCS#11 module
            _log("Looking for authentication certificates");
            h->pkcs11.load(modules[0]);
            if (h->pkcs11.isLoaded()) {
                auto certs = h->pkcs11.getAuthCerts();
                std::vector<unsigned char> cert;
                if (certs.empty()) {
                    _log("Did not find any possible authentication certificates from PKCS#11");
                    // FIXME: to prevent remote probing, return always user cancel?
                    return {{"result", "no_certificates"}};
                } else if (certs.size() == 1) {
                    cert = certs.at(0);
                } else {
                    cert = QtCertSelect::getCert(certs);
                }
                if (cert.empty()) {
                    // user pressed cancel
                    return {{"result", "user_cancel"}};
                }
                // We are silent if only one certificate is present
                QSslCertificate x509 = v2cert(cert);
                _log("Found certificate: %s", x509.subjectInfo(QSslCertificate::CommonName).at(0).toStdString().c_str());

                // Get the first part of the token
                QByteArray dtbs = authenticate_dtbs(x509, msg.value("origin").toString(), msg.value("auth_nonce").toString());
                // Calculate the hash to be signed
                QByteArray hash = QCryptographicHash::hash(dtbs, QCryptographicHash::Sha256);
                // 2. Sign the token hash with the selected certificate
                QByteArray signature = v2ba(QtSigner::sign(h->pkcs11, ba2v(hash), cert));
                // 3. Construct the JWT token to be returned
                QByteArray jwt = dtbs + "." + signature.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
                // 4. profit
                return {{"auth_token", jwt.data()}};
            } else {
                // No PKCS#11 successfully loaded
                return {{"result", "no_certificates"}};
            }
        }
    }
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

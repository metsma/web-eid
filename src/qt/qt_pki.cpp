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

#include "qt_pki.h"

#include "Common.h"
#include "Logger.h"
#include "util.h"
#include "pcsc.h"


#include "modulemap.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>


#ifdef _WIN32
#include "WinCertSelect.h"
#include "WinSigner.h"
#endif

// process SIGN message
void QtPKI::sign(const QString &origin, const QByteArray &cert, const QByteArray &hash, const QString &hashalgo) {
    _log("Signing %s:%s", hashalgo.toStdString().c_str(), toHex(ba2v(hash)).c_str());
    this->cert = cert;
    this->hash = hash;
    this->hashalgo = hashalgo;
    this->purpose = Signing;
    // FIXME: remove origin from signature, available in UI thread, if not needed for windows.
    _log("PKI: Signing stuff");
    start_signature(cert, hash, hashalgo, Signing);
}

// Called from the PIN dialog to do actual login on pkcs11
void QtPKI::login(const CK_RV status, const QString &pin, CertificatePurpose purpose) {
    CK_RV result = status;
    // If dialog was canceled, do not login
    if (result != CKR_FUNCTION_CANCELED) {
        _log("Calling C_Login with %s", pin.toStdString().c_str());

        // This call blocks with a pinpad
        result = pkcs11.login(ba2v(cert), pin.toStdString().c_str());
        emit hide_pin_dialog();
    }

    if (result == CKR_PIN_INCORRECT) {
        // Show again the pin dialog
        _log("showing again pin dialog");
        emit show_pin_dialog(result, *pkcs11.getP11Token(ba2v(cert)), cert, purpose);
    } else {
        pkcs11_sign(result);
    }
}

// all calls hapepning on this thread
void QtPKI::start_signature(const QByteArray &cert, const QByteArray &hash, const QString &hashalgo, CertificatePurpose purpose) {
    const std::vector<unsigned char> crt = ba2v(cert);

#ifdef _WIN32
    if (!pkcs11.getP11Token(crt)) {
        std::vector<unsigned char> signature;
        // if not in PKCS#11, it must be  Windows cert. We make a blocking call to CryptoAPI
        CK_RV status = WinSigner::sign(ba2v(hash), crt, signature);
        QByteArray qsignature = v2ba(signature);
        finish_signature(status, qsignature);
		return;
    }
#endif

    _log("PKCS#11 signing. Showing PIN dialog");
    emit show_pin_dialog(CKR_OK, *pkcs11.getP11Token(ba2v(cert)), cert, purpose);
}

// Login has been successful. Finish ongoing operation
void QtPKI::pkcs11_sign(const CK_RV status) {
    _log("Doing C_Sign()");
    if (status != CKR_OK) {
        return finish_signature(status, 0);
    }

    std::vector<unsigned char> signature_vector;
    CK_RV rv = pkcs11.sign(ba2v(cert), ba2v(hash), signature_vector);
    _log("PKI: signature: %s %d", toHex(signature_vector).c_str(), purpose);
    QByteArray signature = v2ba(signature_vector);
    finish_signature(rv, signature);
}

void QtPKI::finish_signature(const CK_RV status, const QByteArray &signature) {
    if (purpose == Signing) {
        clear();
        // Nothing else to do
        return emit sign_done(status, signature);
    } else if (purpose == Authentication) {
        // Construct the authentication token.
        // FIXME: check before concat ?
        QByteArray token = jwt_token + "." + signature.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        return emit authentication_done(status, QString(token));
    }
}

// process AUTH message
void QtPKI::authenticate(const QString &origin, const QString &nonce) {
    _log("PKI: Authenticating");
    this->origin = origin;
    this->nonce = nonce;
    // Get certificate // FIXME: silent handling
    select_certificate(origin, Authentication, true);
}

void QtPKI::authenticate_with(const CK_RV status, const QByteArray &cert) {

    if (status != CKR_OK) {
        return emit authentication_done(status, QString());
    }

    // Construct dtbs
    jwt_token = authenticate_dtbs(QSslCertificate(cert, QSsl::Der), origin, nonce);

    // Calculate hash
    hash = QCryptographicHash::hash(jwt_token, QCryptographicHash::Sha256);

    // Sign the hash
    start_signature(cert, hash, hashalgo, Authentication);
}

// Selects a certificate for the PKI context.
// Called from web or internally (for authenticate)
void QtPKI::select_certificate(const QString &origin, CertificatePurpose purpose, bool silent) {
    _log("PKI: selecting certificate");

    // FIXME: single place where this happens
    std::vector<std::vector<unsigned char>> atrs = PCSC::atrList();
    std::vector<std::string> modules = P11Modules::getPaths(atrs);

    std::vector<unsigned char> cert;

    if (modules.empty()) {
#ifdef _WIN32
        // No PKCS#11 modules detected for any of the connected cards.
        // Check if we can find a cert from certstore
        // FIXME: UI string
        CK_RV rv = WinCertSelect::getCert(purpose, LPWSTR(tr("Signing on %1, please select certificate").arg(origin).utf16()), cert);
        return emit cert_selected(rv, v2ba(cert), purpose);
#endif
        cert_selected(CKR_KEY_NEEDED, 0, purpose);
    } else {
        // FIXME: only one module currently
        pkcs11.load(modules[0]);
        std::vector<std::vector<unsigned char>> certs = pkcs11.getCerts(purpose);
        if (certs.size() == 1 && silent) {
            return cert_selected(CKR_OK, v2ba(certs[0]), purpose);
        }
        if (certs.empty()) {
            // TODO: what return code to use ?
            return cert_selected(CKR_KEY_NEEDED, 0, purpose);
        } else {
            // TODO: silent handling
            return emit show_cert_select(origin, certs, purpose); // FIXME: remove origin from signature, available from main
        }
    }
}

void QtPKI::cert_selected(const CK_RV status, const QByteArray &cert, CertificatePurpose purpose) {
    _log("Certificate was selected %s", errorName(status));
    this->cert = cert;
    this->purpose = purpose;
    // FIXME: calling from sign()
    if (purpose == Signing) {
        return emit select_certificate_done(status, cert);
    }
    if (purpose == Authentication) {
        // finish authentication with the certificate
        return authenticate_with(status, cert);
    }
}

// FIXME: move to pkcs11module.h
const char *QtPKI::errorName(const CK_RV err) {
    return PKCS11Module::errorName(err);
}

// static
QByteArray QtPKI::authenticate_dtbs(const QSslCertificate &cert, const QString &origin, const QString &nonce) {
    QByteArray dtbs;
    // Construct the data to be signed
    auto subject = cert.subjectInfo(QSslCertificate::CommonName);
    auto issuer = cert.issuerInfo(QSslCertificate::CommonName);
    if (subject.size() < 1 || issuer.size() < 1) {
        _log("No CN for subject or issuer!");
        return dtbs;
    }
    _log("Constructing JWT for %s", subject.at(0).toStdString().c_str());

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
        {"iss", issuer.at(0)}, // TODO: TBS
        {"sub", subject.at(0)}, // TODO: TBS
        {"nonce", nonce},
    });

    QByteArray payload_json = payload_map.toJson(QJsonDocument::Compact);
    QByteArray payload = payload_json.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    _log("JWT payload: %s", payload_map.toJson().toStdString().c_str());

    // calculate DTBS (Data To Be Signed)
    dtbs = header + "." + payload;

    return dtbs;
}

/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "qpki.h"

#include "Common.h"
#include "Logger.h"
#include "util.h"

#include "modulemap.h"
#include "qwincrypt.h"

#include "dialogs/select_cert.h"
#include "dialogs/pin.h"

#include <QtConcurrent>

#include <QSslCertificate>
#include <QSslCertificateExtension>

void QPKIWorker::cardInserted(const QString &reader, const QByteArray &atr) {
    _log("Card inserted to %s (%s), refreshing available certificates", qPrintable(reader), qPrintable(atr.toHex()));
    // Check if module already present
    std::vector<std::string> mods = P11Modules::getPaths({ba2v(atr)});
    if (mods.size() > 0) {
        for (const auto &m: mods) {
            _log("Trying module %s", m.c_str());
            if (!modules.contains(QString::fromStdString(m))) {
                _log("Module not yet loaded, doing it");
                PKCS11Module *module = new PKCS11Module();
                if (module->load(m) == CKR_OK) {
                    modules[QString::fromStdString(m)] = module;
                    refresh(); // TODO: optimize
                    _log("Module loaded with %d certificates", module->getCerts().size());
                    break; // Use first module that reports certificates
                } else {
                    _log("Could not load module %s", m.c_str());
                }
            } else {
                _log("%s is already loaded", m.c_str());
                refresh();
            }
        }
    }
}

void QPKIWorker::cardRemoved(const QString &reader) {
    _log("Card removed from %s, refreshing PKCS#11 certificates", qPrintable(reader));
    refresh();
}


void QPKIWorker::refresh() {
    QMap<QByteArray, P11Token> certs;
    for (const auto &m: modules.keys()) {
        modules[m]->refresh();
        for (auto &c: modules[m]->getCerts()) {
            c.second.module = m.toStdString();
            certs[v2ba(c.first)] = c.second;
        }
    }
    if (certificates.size() != certs.size()) {
        certificates = certs;
        emit refreshed(certificates);
    }
}

void QPKIWorker::login(const QByteArray &cert, const QString &pin) {
    _log("Login in worker");
    PKCS11Module *m = modules[QString::fromStdString(certificates[cert].module)];
    // Block with pinpad
    CK_RV rv = m->login(ba2v(cert), pin.toLatin1().data());
    emit loginDone(rv);
}

// FIXME: hashtype
void QPKIWorker::sign(const QByteArray &cert, const QByteArray &hash) {
    _log("Signing in worker");
    PKCS11Module *m = modules[QString::fromStdString(certificates[cert].module)];
    // Takes a sec or so
    std::vector<unsigned char> result;
    CK_RV rv = m->sign(ba2v(cert), ba2v(hash), result);
    emit signDone(rv, v2ba(result));
}

void QPKI::updateCertificates(const QMap<QByteArray, P11Token> certs) {
    // FIXME
    certificates = certs;
    _log("Updated certificates, emitting as well. %d", certificates.size());
    return emit certificateListChanged(QVector<QByteArray>::fromList(certificates.keys()));
}

QVector<QByteArray> QPKI::getCertificates() {
    return QVector<QByteArray>::fromList(certificates.keys());
}

// Select a single certificate for a web context, signal certificate() when done
void QPKI::select(const WebContext *context, const CertificatePurpose type) {
    _log("Selecting certificate for %s", qPrintable(context->friendlyOrigin()));
    // If we have PKCS#11 certs, show a Qt window, otherwise
    // FIXME: logic is baaaaaad.
#ifndef Q_OS_WIN
    QtSelectCertificate *dlg = new QtSelectCertificate(context, type);
    connect(context, &WebContext::disconnected, dlg, &QDialog::reject);
    connect(this, &QPKI::certificateListChanged, dlg, &QtSelectCertificate::update);
    connect(dlg, &QDialog::rejected, [this, context] {
        return emit certificate(context, CKR_FUNCTION_CANCELED, 0);
    });
    connect(dlg, &QtSelectCertificate::certificateSelected, [this, context] (const QByteArray &cert) {
        return emit certificate(context, CKR_OK, cert);
    });
    dlg->update(QVector<QByteArray>::fromList(certificates.keys()));
#endif

#ifdef Q_OS_WIN
    connect(&winop, &QFutureWatcher<QWinCrypt::ErroredResponse>::finished, [this, context] {
        this->winop.disconnect(); // remove signals
        QWinCrypt::ErroredResponse result = this->winop.result();
        _log("Winop done: %s %d", QPKI::errorName(result.error), result.result.size());
        if (result.error == CKR_OK) {
            return emit certificate(context, result.error, result.result.at(0)); // FIXME: range check
        } else {
            return emit certificate(context, result.error, 0);
        }
    });
    // run future
    winop.setFuture(QtConcurrent::run(&QWinCrypt::selectCertificate, type, context->friendlyOrigin(), QStringLiteral("Dummy text about type")));
#endif
    //return emit certificate(context->id, CKR_FUNCTION_CANCELED, 0);
}

// Calculate a signature, emit signature() when done
// FIXME: add message to function signature, signature type as enum
void QPKI::sign(const WebContext *context, const QByteArray &cert, const QByteArray &hash, const QString &hashalgo) {
    _log("Signing on %s %s:%s", qPrintable(context->friendlyOrigin()), qPrintable(hashalgo), qPrintable(hash.toHex()));

#ifndef Q_OS_WIN
    QtPINDialog *dlg = new QtPINDialog(context, cert, certificates[cert], CKR_OK, Signing);
    connect(context, &WebContext::disconnected, dlg, &QDialog::reject);
    connect(dlg, &QDialog::rejected, [this, context] {
        return emit signature(context, CKR_FUNCTION_CANCELED, 0);
    });
    connect(dlg, &QtPINDialog::failed, [this, context] (CK_RV rv) {
        return emit signature(context, rv, 0);
    });
    // from dialog to worker
    connect(dlg, &QtPINDialog::login, &worker, &QPKIWorker::login, Qt::QueuedConnection);
    connect(&worker, &QPKIWorker::loginDone, dlg, &QtPINDialog::update, Qt::QueuedConnection);
    connect(dlg, &QDialog::accepted, [=] {
        _log("PIN dialog OK, signing stuff");
        // PIN has been successfully verified. Issue a C_Sign, subscribing to the result
        connect(this, &QPKI::signDone, [this, context] (CK_RV rv, QByteArray result) {
            // Remove lambda
            QObject::disconnect(this, &QPKI::signDone, this, nullptr);
            _log("Sign done, result is %s", QPKI::errorName(rv));
            return emit signature(context, rv, result);
        });
        _log("Emitting p11sign");
        return emit p11sign(cert, hash);
    });
#endif


// and wire up signals to the worker, that does actual signing
// otherwise run it in a future and wire up signals.
#ifdef Q_OS_WIN
    connect(&winop, &QFutureWatcher<QWinCrypt::ErroredResponse>::finished, [this, context] {
        this->winop.disconnect(); // remove signals
        QWinCrypt::ErroredResponse result = this->winop.result();
        _log("Winop done: %s %d", QPKI::errorName(result.error), result.result.size());
        if (result.error == CKR_OK) {
            return emit signature(context, result.error, result.result.at(0)); // FIXME: range check
        } else {
            return emit signature(context, result.error, 0);
        }
    });
    // run future
    winop.setFuture(QtConcurrent::run(&QWinCrypt::sign, cert, hash, QWinCrypt::HashType::SHA256));
#endif

}



// FIXME: move to pkcs11module.h
const char *QPKI::errorName(const CK_RV err) {
    return PKCS11Module::errorName(err);
}

// static
QByteArray QPKI::authenticate_dtbs(const QSslCertificate &cert, const QString &origin, const QString &nonce) {
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

// TODO: move this to QtPKI
bool QPKI::usageMatches(const QByteArray &crt, CertificatePurpose type)
{
    QSslCertificate cert(crt, QSsl::Der);
    bool isCa = true;
    bool isSSLClient = false;
    bool isNonRepudiation = false;

    for (const QSslCertificateExtension &ext: cert.extensions()) {
        QVariant v = ext.value();
//      _log("ext: %s", ext.name().toStdString().c_str());
        if (ext.name() == "basicConstraints") {
            QVariantMap m = ext.value().toMap();
            isCa = m.value("ca").toBool();
        } else if (ext.oid() == "2.5.29.37") {
            // 2.5.29.37 - extendedKeyUsage
            // XXX: these are not declared stable by Qt.
            // Linux returns parsed map (OpenSSL?) OSX 5.8 returns QByteArrays, 5.5 works
            if (v.canConvert<QByteArray>()) {
                // XXX: this is 06082b06010505070302 what is 1.3.6.1.5.5.7.3.2 what is "TLS Client"
                if (v.toByteArray().toHex().contains("06082b06010505070302")) {
                    isSSLClient = true;
                }
            } else if (v.canConvert<QList<QVariant>>()) {
                // Linux
                if (v.toList().contains("TLS Web Client Authentication")) {
                    isSSLClient = true;
                }
            }
        } else if (ext.name() == "keyUsage") {
            if (v.canConvert<QList<QVariant>>()) {
                // Linux
                if (v.toList().contains("Non Repudiation")) {
                    isNonRepudiation = true;
                }
            }
            // FIXME: detect NR from byte array
            // Do a ugly trick for esteid only
            QList<QString> ou = cert.subjectInfo(QSslCertificate::OrganizationalUnitName);
            if (!isNonRepudiation && ou.size() > 0 && ou.at(0) == "digital signature") {
                isNonRepudiation = true;
            }
            _log("keyusage: %s", v.toByteArray().toHex().toStdString().c_str());
        }
    }
    _log("Certificate flags: ca=%d auth=%d nonrepu=%d", isCa, isSSLClient, isNonRepudiation);
    return !isCa &&
           ((type & Authentication && isSSLClient) || (type & Signing && isNonRepudiation));
}
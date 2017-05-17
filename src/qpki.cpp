/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "qpki.h"

#include "Common.h"
#include "Logger.h"
#include "util.h"

#include "modulemap.h"
#include "qwincrypt.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

#include <QtConcurrent>


void QPKIWorker::cardInserted(const QString &reader, const QByteArray &atr) {
    _log("Card inserted to %s (%s), refreshing available certificates", qPrintable(reader), qPrintable(atr.toHex()));
    // Check which module to try
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

void QPKIWorker::refresh() {
    QMap<QByteArray, PKIToken> certs;
    for (const auto &m: modules.keys()) {
        modules[m]->refresh();
        for (const auto &c: modules[m]->getCerts()) {
            certs[v2ba(c)] = {PKCS11, m};
        }
    }
    if (certificates.size() != certs.size()) {
        certificates = certs;
        emit certificateListChanged(getCertificates());
    }
}

void QPKIWorker::cardRemoved(const QString &reader) {
    _log("Card removed from %s, refreshing", qPrintable(reader));
    refresh();
}

QVector<QByteArray> QPKIWorker::getCertificates() {
    QVector<QByteArray> result;
    for (const auto &c: certificates.keys()) {
        result.append(c);
    }
    return result;
}

QVector<QByteArray> QPKI::getCertificates() {
    QMutexLocker locker(&worker.mutex);
    return worker.getCertificates();
}

// Select a single certificate for a web context, signal certificate() when done
void QPKI::select(const WebContext *context, const CertificatePurpose type) {
    _log("Selecting certificate for %s", qPrintable(context->friendlyOrigin()));
    // If we have PKCS#11 certs, show a Qt window, otherwise
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
    winop.setFuture(QtConcurrent::run(&QWinCrypt::selectCertificate, type, QStringLiteral("dummy string FIXME")));
#endif
    //return emit certificate(context->id, CKR_FUNCTION_CANCELED, 0);
}

// Calculate a signature, emit signature() when done
// FIXME: add message to function signature, signature type as enum
void QPKI::sign(const WebContext *context, const QByteArray &cert, const QByteArray &hash, const QString &hashalgo) {
    _log("Signing on %s %s:%s", qPrintable(context->friendlyOrigin()), qPrintable(hashalgo), qPrintable(hash.toHex()));
// if p11 cert, show a PIN dialog (only if required?)
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
    _log("PKI: Signing stuff");
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

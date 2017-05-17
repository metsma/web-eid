/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "Logger.h"

#include <QThread>
#include <QSslCertificate>

#include <QFutureWatcher>

#include "pkcs11module.h"
#include "qpcsc.h"


/*
PKI keeps track of available certificates
- lives in a separate thread
- listens to pcsc events, updates state, emits pki events
- keeps track of loaded pkcs11 modules
*/

// PKIWorker owns PKCS11 modules. CAPI is handled with futures in QPKI
class QPKIWorker: public QObject {
    Q_OBJECT

public:
    // FIXME: remove from worker
    enum TokenType {
        CAPI,
        PKCS11
    };
    struct PKIToken {
        TokenType tktype;
        QString module; // if PKCS#11
    };

    ~QPKIWorker() {
        for (const auto &m: modules.values()) {
            delete m;
        }
    }

    QMutex mutex;
    QVector<QByteArray> getCertificates();

public slots:
    //refresh available certificates. Triggered by PCSC on cardInserted()
    void cardInserted(const QString &reader, const QByteArray &atr);
    void cardRemoved(const QString &reader);

    // Refresh all certificate sources
    void refresh();
signals:
    // If list of available certificates changes
    void certificateListChanged(const QVector<QByteArray> certs);

private:
    QMap<QString, PKCS11Module *> modules; // loaded PKCS#11 modules
    QMap<QByteArray, PKIToken> certificates; // available certificates

};


class QPKI: public QObject {
    Q_OBJECT

public:
    QPKI(QtPCSC *pcsc): PCSC(pcsc) {
        thread.start();
        worker.moveToThread(&thread);
        // FIXME: proxy
        connect(&worker, &QPKIWorker::certificateListChanged, this, &QPKI::certificateListChanged, Qt::QueuedConnection);
        resume();
    }

    ~QPKI() {
        thread.quit();
        thread.wait();
    }

    static const char *errorName(const CK_RV err);

    QVector<QByteArray> getCertificates();

    void pause() {
        _log("Pausing PCSC event handling");
        disconnect(PCSC, 0, &worker, 0);
    };
    void resume() {
        _log("Resuimg PCSC event handling");
        connect(PCSC, &QtPCSC::cardInserted, &worker, &QPKIWorker::cardInserted, Qt::QueuedConnection);
        connect(PCSC, &QtPCSC::cardRemoved, &worker, &QPKIWorker::cardRemoved, Qt::QueuedConnection);
    };

    // TODO: type => list of oid-s to match
    void select(const WebContext *context, const CertificatePurpose type);
    // sign a hash with a given certificate
    void sign(const WebContext *context, const QByteArray &cert, const QByteArray &hash, const QString &hashalgo);

    static QByteArray authenticate_dtbs(const QSslCertificate &cert, const QString &origin, const QString &nonce);

signals:
    void certificateListChanged(const QVector<QByteArray> certs); // for dialog. FIXME. aggregate win + p11
    // Signature has been calculated
    void signature(const WebContext *context, const CK_RV result, const QByteArray &value);
    // Certificate has been chose (either p11 or win)
    void certificate(const WebContext *context, const CK_RV result, const QByteArray &value);

private:
    void refresh();

#ifdef Q_OS_WIN
    // Windows operation
    QFutureWatcher<QWinCrypt::ErroredResponse> winop; // Refreshes windows cert stores on demand.
    QFutureWatcher<QWinCrypt::ErroredResponse> wincerts; // Refreshes windows cert stores on demand.
#endif

    QtPCSC *PCSC;
    QThread thread;
    QPKIWorker worker;
};

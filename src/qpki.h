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
    ~QPKIWorker() {
        for (const auto &m: modules.values()) {
            delete m;
        }
    }

public slots:
    //refresh available certificates. Triggered by PCSC on cardInserted()
    void cardInserted(const QString &reader, const QByteArray &atr);
    void cardRemoved(const QString &reader);

    // Refresh all certificate sources
    void refresh(const QByteArray &atr = 0);

    void login(const QByteArray &cert, const QString &pin);
    void sign(const QByteArray &cert, const QByteArray &hash); // FIXME: hashtype

signals:
    // If list of available certificates changes after refresh
    void refreshed(const QMap<QByteArray, P11Token> certs);

    void loginDone(const CK_RV rv);
    void signDone(const CK_RV rv, const QByteArray &signature);

    void noDriver(const QString &reader, const QByteArray &atr, const QByteArray &extra);

private:
    QMap<QString, PKCS11Module *> modules; // loaded PKCS#11 modules
    QMap<QByteArray, P11Token> certificates; // from which module a certificate comes
};


class QPKI: public QObject {
    Q_OBJECT

public:
    QPKI(QtPCSC *pcsc): PCSC(pcsc) {
        thread.start();
        worker.moveToThread(&thread);
        // FIXME: proxy
        connect(&worker, &QPKIWorker::refreshed, this, &QPKI::updateCertificates, Qt::QueuedConnection);
        // control signals
        //connect(this, &QPKI::login, &worker, &QPKIWorker::login, Qt::QueuedConnection);
        // FIXME: have this tied to the PIN dialog maybe ?
        connect(this, &QPKI::p11sign, &worker, &QPKIWorker::sign, Qt::QueuedConnection);
        connect(&worker, &QPKIWorker::signDone, this, &QPKI::signDone, Qt::QueuedConnection);
        connect(&worker, &QPKIWorker::noDriver, this, &QPKI::noDriver, Qt::QueuedConnection);
        // Start listening for pcsc events.
        resume();
    }

    ~QPKI() {
        thread.quit();
        thread.wait();
    }

    static const char *errorName(const CK_RV err);
    static bool usageMatches(const QByteArray &crt, CertificatePurpose type);

    QVector<QByteArray> getCertificates();

    void pause() {
        _log("Pausing PCSC event handling");
        disconnect(PCSC, 0, &worker, 0);
    };
    void resume() {
        _log("Resuming PCSC event handling");
        connect(PCSC, &QtPCSC::cardInserted, &worker, &QPKIWorker::cardInserted, Qt::QueuedConnection);
        connect(PCSC, &QtPCSC::cardRemoved, &worker, &QPKIWorker::cardRemoved, Qt::QueuedConnection);
    };

    // TODO: type => list of oid-s to match
    void select(const WebContext *context, const CertificatePurpose type);
    // sign a hash with a given certificate
    void sign(const WebContext *context, const QByteArray &cert, const QByteArray &hash, const QString &hashalgo);

    static QByteArray authenticate_dtbs(const QSslCertificate &cert, const QString &origin, const QString &nonce);

    void updateCertificates(const QMap<QByteArray, P11Token> certs);

signals:
    // TODO: enrich with PKCS#11 information (tries remainign etc)
    void certificateListChanged(const QVector<QByteArray> certs); // for dialog. FIXME. aggregate win + p11
    // Signature has been calculated
    void signature(const WebContext *context, const CK_RV result, const QByteArray &value);
    // Certificate has been chose (either p11 or win)
    void certificate(const WebContext *context, const CK_RV result, const QByteArray &value);

    void noDriver(const QString &reader, const QByteArray &atr, const QByteArray &extra);

    // Control signals with worker
    void login(const QByteArray &cert, const QString &pin);
    void p11sign(const QByteArray &cert, const QByteArray &hash); // FIXME: hashtype
    void signDone(const CK_RV rv, const QByteArray &signature);

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
    QMap<QByteArray, P11Token> certificates; // FIXME: type?
};

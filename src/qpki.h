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

#include <QThread>
#include <QSslCertificate>

#include <QFutureWatcher>

#include "internal.h"
#include "pkcs11module.h"
#include "qpcsc.h"


/*
PKI keeps track of available certificates
- lives in a separate thread
- listens to pcsc events, updates state, emits pki events
- keeps track of loaded pkcs11 modules
*/
class QPKIWorker: public QObject {
    Q_OBJECT

public:
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

    QFutureWatcher<QVector<QByteArray>> wincerts; // Refreshes windows cert stores on demand.
};


class QPKI: public QObject {
    Q_OBJECT

public:
    QPKI(QtPCSC *PCSC) {
        thread.start();
        worker.moveToThread(&thread);

        connect(PCSC, &QtPCSC::cardInserted, &worker, &QPKIWorker::cardInserted, Qt::QueuedConnection);
        //connect(PCSC, &QtPCSC::cardRemoved, &worker, &QPKIWorker::cardRemoved, Qt::QueuedConnection);

        connect(&worker, &QPKIWorker::certificateListChanged, this, &QPKI::certificateListChanged, Qt::QueuedConnection);
    }

    ~QPKI() {
        thread.quit();
        thread.wait();
    }

    static const char *errorName(const CK_RV err);

    QVector<QByteArray> getCertificates();

    // sign a hash with a given certificate
    void sign(const QString &origin, const QByteArray &cert, const QByteArray &hash, const QString &hashalgo);

signals:
    // Useful main thread signals
    void certificateListChanged(const QVector<QByteArray> certs);

private:
    QMap<QString, MessageType> ongoing; // Keep track of ongoing operations

    void refresh();
    static QByteArray authenticate_dtbs(const QSslCertificate &cert, const QString &origin, const QString &nonce);

    // send message to main
    void select_certificate(const QVariantMap &msg);

    QThread thread;
    QPKIWorker worker;
};

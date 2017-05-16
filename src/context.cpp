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


#include "context.h"

#include "Logger.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QtConcurrent>

#include "main.h" // for parent

#include "dialogs/select_reader.h"

WebContext::WebContext(QObject *parent, QLocalSocket *client): QObject(parent)  {
    this->ls = client;
    connect(client, &QLocalSocket::readyRead, [this, client] {
        _log("Handling data from local socket");
        _log("Available: %d", client->bytesAvailable());
        quint32 msgsize = 0;
        if (client->bytesAvailable() < qint64(sizeof(msgsize) + 1)) {
            return;
        }
        if (client->read((char*)&msgsize, sizeof(msgsize)) == sizeof(msgsize)) {
            _log("Read message size: %d bytes", msgsize);
            QByteArray msg(int(msgsize), 0);
            quint64 numread = client->read(msg.data(), msgsize);
            if (numread == msgsize) {
                _log("Read message of %d bytes", msgsize);
                // Make JSON
                QVariantMap json = QJsonDocument::fromJson(msg).toVariant().toMap();

                // re-serialize msg
                QByteArray response =  QJsonDocument::fromVariant(json).toJson();
                _log("Read message:\n%s", response.constData());

                // Check for mandatory fields
                if (!json.contains("origin") || !json.contains("id")) {
                    _log("No id or origin, terminating");
                    terminate();
                }

                // Check origin
                if (origin.isEmpty()) {
                    origin = json.value("origin").toString();
                } else {
                    if (origin != json.value("origin").toString()) {
                        _log("Origin mismatch, terminating");
                        terminate();
                    }
                }
                processMessage(json);
            } else {
                _log("Could not read message, read %d of %d", numread, msgsize);
                terminate();
            }
        } else {
            _log("Could not read message size");
            terminate();
        }
    });
    connect(client, &QLocalSocket::disconnected, [this, client] {
        _log("Local client disconnected (%s)", qPrintable(this->origin));
        emit disconnected();
    });
    // Save references to PKI and PCSC
    PCSC = &((QtHost *)parent)->PCSC;
    PKI = &((QtHost *)parent)->PKI;
}

WebContext::WebContext(QObject *parent, QWebSocket *client): QObject(parent) {
    this->ws = client;
    this->origin = client->origin();
    connect(client, &QWebSocket::textMessageReceived, [this, client] (QString message) {
        _log("Message received from %s", qPrintable(origin));
        QVariantMap json = QJsonDocument::fromJson(message.toUtf8()).toVariant().toMap();
        if (!json.contains("id")) {
            _log("No id, terminating");
            terminate();
        }
        // re-serialize msg
        QByteArray response =  QJsonDocument::fromVariant(json).toJson();
        _log("Read message: %s", response.constData());

        // Add origin for uniform message processing
        json["origin"] = origin;
        processMessage(json);
    });
    connect(client, &QWebSocket::disconnected, [this, client] {
        _log("%s disconnected", qPrintable(client->origin()));
        emit disconnected();
    });

    // Save references to PKI and PCSC
    PCSC = &((QtHost *)parent)->PCSC;
    PKI = &((QtHost *)parent)->PKI;
}

// Process a message from a browsing context
void WebContext::processMessage(const QVariantMap &message) {
    _log("Processing message");
    QVariantMap resp;

    // Current message ID
    msgid = message.value("id").toString();

    // Origin. If unset for context, set
    // Check if origin is secure
    QString msgorigin = message.value("origin").toString();
    if (!isSecureOrigin(msgorigin)) {
        _log("Insecure origin, dropping connection");
        terminate();
        return;
    }
    origin = msgorigin;

    // TODO: have lanagueg in app settings
    // Setting the language is also a onetime operation, thus do it here.
    QLocale locale = message.contains("lang") ? QLocale(message.value("lang").toString()) : QLocale::system();
    _log("Setting language to %s", locale.name().toStdString().c_str());
    // look up translation rom resource :/translations/strings_XX.qm
    /*
        FIXME: multiple translators if passed from message. Or have one single translator configurable from tray
        if (translator.load(QLocale(message.value("lang").toString()), QLatin1String("strings"), QLatin1String("_"), QLatin1String(":/translations"))) {
            if (installTranslator(&translator)) {
                _log("Language set");
            } else {
                _log("Language NOT set");
            }
        } else {
            _log("Failed to load translation");
        }
    */

    // FIXME: Check if timeout specified
    timer.setSingleShot(true);
    timer.start(5000); // 5 seconds
    // Command dispatch
    if (message.contains("version")) {
        return outgoing({{"id", msgid}, {"version", VERSION}});
    } else if (message.contains("SCardConnect")) {
        // Show reader selection or confirmation dialog
        // TODO: suppress PKI insertion/removal detection until connect or disconnect
        // to avoid races for card reader resources
        dialog = new QtSelectReader(this); // FIXME
        PKI->pause();
        ((QtSelectReader *)dialog)->update(PCSC->getReaders());
        connect(PCSC, &QtPCSC::readerListChanged, (QtSelectReader *)dialog, &QtSelectReader::update, Qt::QueuedConnection);
        connect(PCSC, &QtPCSC::cardInserted, (QtSelectReader *)dialog, &QtSelectReader::cardInserted, Qt::QueuedConnection);
        connect(PCSC, &QtPCSC::readerAttached, (QtSelectReader *)dialog, &QtSelectReader::readerAttached, Qt::QueuedConnection);

        connect(dialog, &QDialog::rejected, [=] {
            PKI->resume();
            outgoing({{"error", QtPCSC::errorName(SCARD_E_CANCELLED)}});
        });
        // Connect to the reader once the reader name is known
        connect((QtSelectReader *)dialog, &QtSelectReader::readerSelected, [this, message] (QString name) {
            QPCSCReader *r = PCSC->connectReader(this, name, message.value("SCardConnect").toMap().value("protocol", "*").toString(), true);
            readers[name] = r;
            connect(r, &QPCSCReader::disconnected, [this, name] (LONG err) {
                _log("Disconnected: %s", QtPCSC::errorName(err));
                PKI->resume();
                if (readers.contains(name)) {
                    QPCSCReader *rd = readers.take(name);
                    // If disconnect happens in between of messages (eg dialog cancel)
                    // error will be given on next incoming message
                    if (!msgid.isEmpty()) {
                        if (err != SCARD_S_SUCCESS) {
                            outgoing({{"error", QtPCSC::errorName(err)}});
                        } else {
                            outgoing({});
                        }
                    } else {
                        // TODO: store lasterror
                    }
                    rd->deleteLater();
                } else {
                    // Not yet connected
                }
            });
            connect(r, &QPCSCReader::connected, [=] (QByteArray atr, QString proto) {
                _log("connected: %s %s", qPrintable(proto), qPrintable(atr.toHex()));
                PKI->resume();
                outgoing({{"reader", name}, {"protocol", proto}, {"atr", atr.toHex()}});
            });
            connect(r, &QPCSCReader::received, [=] (QByteArray apdu) {
                _log("Rreceived apdu");
                outgoing({{"bytes", apdu.toHex()}});
            });
        });
    } else if (message.contains("SCardDisconnect")) {
        if (!readers.isEmpty()) {
            QPCSCReader *r = readers.first(); // FIXME
            r->disconnect();
        } else {
            // Already disconnected, do nothing
            outgoing({});
        }
    } else if (message.contains("SCardTransmit")) {
        if (!readers.isEmpty()) {
            QPCSCReader *r = readers.first(); // FIXME
            r->transmit(QByteArray::fromHex(message.value("SCardTransmit").toMap().value("bytes").toString().toLatin1()));
        } else {
            outgoing({{"error", QtPCSC::errorName(SCARD_E_NO_SMARTCARD)}});
        }
    } else if (message.contains("sign")) {
        QVariantMap sign = message.value("sign").toMap();
        const QByteArray cert = QByteArray::fromBase64(sign.value("cert").toString().toLatin1());
        const QByteArray hash = QByteArray::fromBase64(sign.value("hash").toString().toLatin1());
        connect(PKI, &QPKI::signature, [this] (const QString &context, const CK_RV result, const QByteArray &value) {
            disconnect(PKI, 0, this, 0);
            if (result == CKR_OK) {
                outgoing({{"signature", value.toBase64()}}); // FIXME
            } else {
                outgoing({{"error", QPKI::errorName(result)}});
            }
        });
        PKI->sign(this, cert, hash, QStringLiteral("SHA-256")); // FIXME: signature
    } else if (message.contains("cert")) {
        connect(PKI, &QPKI::certificate, [this] (const QString &context, const CK_RV result, const QByteArray &value) {
            disconnect(PKI, 0, this, 0);
            if (result == CKR_OK) {
                outgoing({{"certificate", value.toBase64()}}); // FIXME
            } else {
                outgoing({{"error", QPKI::errorName(result)}});
            }
        });
        PKI->select(this, Authentication);
    } else if (message.contains("auth")) {
        // TODO: Select certificate if needed
        connect(PKI, &QPKI::certificate, [this] (const QString &context, const CK_RV result, const QByteArray &value) {
            disconnect(PKI, 0, this, 0);
            if (result == CKR_OK) {
                outgoing({{"token", value}}); // FIXME
            } else {
                outgoing({{"error", QPKI::errorName(result)}});
            }
        });
        PKI->select(this, Authentication);
    } else {
        outgoing({{"error", "protocol"}});
    }
}

void WebContext::outgoing(QVariantMap message) {
    message["id"] = msgid;
    msgid.clear();

    QByteArray response = QJsonDocument::fromVariant(message).toJson(QJsonDocument::Compact);
    QByteArray logmsg = QJsonDocument::fromVariant(message).toJson();
    _log("Sending outgoing message:\n%s", logmsg.constData());
    if (this->ls) {
        quint32 msgsize = response.size();
        ls->write((char *)&msgsize, sizeof(msgsize));
        ls->write(response);
    } else if(this->ws) {
        ws->sendTextMessage(QString(response));
    } else {
        _log("Do not know where to send a reply for %s", qPrintable(msgid));
    }
}

bool WebContext::terminate() {
    if (ws) {
        ws->disconnect();
    } else if(ls) {
        ls->abort();
    }
    return true;
}

QString WebContext::friendlyOrigin() const {

    QUrl url(origin);
    if (url.scheme() == "file") {
        return "localhost";
    } else {
        // FIXME: port, if not standard
        return url.host();
    }
}

bool WebContext::isSecureOrigin(const QString &origin) {
    QUrl url(origin, QUrl::StrictMode);
    if (!url.isValid())
        return false;
    if (url.scheme() == "https" || url.scheme() == "file" || url.host() == "localhost" || url.scheme() == "moz-extension" || url.scheme() == "chrome-extension") {
        return true;
    }
    return false;
}

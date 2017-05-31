/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "context.h"

#include "debuglog.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QtConcurrent>

#include "main.h" // for parent

#include "dialogs/select_reader.h"

WebContext::WebContext(QObject *parent, QLocalSocket *client): QObject(parent)  {
    this->ls = client;
    connect(client, &QLocalSocket::readyRead, this, [this, client] {
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

                // Handle internal messages
                if (json.contains("internal")) {
                    if (json["internal"] == "quit") {
                        return QApplication::quit();
                    }
                }
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
    connect(client, &QLocalSocket::disconnected, this, [this, client] {
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
    connect(client, &QWebSocket::textMessageReceived, this, [this, client] (QString message) {
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
    connect(client, &QWebSocket::disconnected, this, [this, client] {
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
    if (!msgid.isEmpty()) {
        return outgoing({{"error", "protocol"}});
    }
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

    timer.setSingleShot(true);
    timer.start(5000); // 5 seconds
    // Command dispatch
    if (message.contains("version")) {
        return outgoing({{"id", msgid}, {"version", VERSION}});
    } else if (message.contains("SCardConnect")) {
        auto params = message.value("SCardConnect").toMap();
        // Show reader selection or confirmation dialog
        // to avoid races for card reader resources
        QList<QByteArray> atrs;
        if (params.contains("atrs")) {
            for (const auto &a: params.value("atrs").toList()) {
                atrs.append(QByteArray::fromBase64(a.toString().toLatin1()));
            }
        }
        dialog = new QtSelectReader(this, atrs); // FIXME
        if (params.contains("timeout")) {
            timer.setSingleShot(true);
            timer.setInterval(params.value("timeout", 60).toInt() * 1000); // FIXME: define "infinity"
            connect(&timer, &QTimer::timeout, dialog, &QDialog::reject);
        }
        PKI->pause();
        ((QtSelectReader *)dialog)->update(PCSC->getReaders());
        connect(PCSC, &QtPCSC::readerListChanged, (QtSelectReader *)dialog, &QtSelectReader::update, Qt::QueuedConnection);
        connect(PCSC, &QtPCSC::cardInserted, (QtSelectReader *)dialog, &QtSelectReader::cardInserted, Qt::QueuedConnection);
        connect(PCSC, &QtPCSC::cardRemoved, (QtSelectReader *)dialog, &QtSelectReader::cardRemoved, Qt::QueuedConnection);
        connect(PCSC, &QtPCSC::readerAttached, (QtSelectReader *)dialog, &QtSelectReader::readerAttached, Qt::QueuedConnection);
        connect(this, &WebContext::disconnected, dialog, &QDialog::reject);
        connect(dialog, &QDialog::rejected, this, [=] {
            PKI->resume();
            outgoing({{"error", QtPCSC::errorName(SCARD_E_CANCELLED)}});
        });
        // Connect to the reader once the reader name is known
        connect((QtSelectReader *)dialog, &QtSelectReader::readerSelected, this, [this, params] (QString name) {
            QPCSCReader *r = PCSC->connectReader(this, name, params.value("protocol", "*").toString(), true);
            readers[name] = r;
            connect(r, &QPCSCReader::disconnected, this, [this, name] (LONG err) {
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
            connect(r, &QPCSCReader::connected, this, [=] (QByteArray atr, QString proto) {
                _log("connected: %s %s", qPrintable(proto), qPrintable(atr.toHex()));
                PKI->resume();
                outgoing({{"name", name}, {"protocol", proto}, {"atr", atr.toBase64()}});
            });
            connect(r, &QPCSCReader::received, this, [=] (QByteArray apdu) {
                _log("Received apdu");
                outgoing({{"bytes", apdu.toBase64()}});
            });
        });
    } else if (message.contains("SCardDisconnect")) {
        auto params = message.value("SCardDisconnect").toMap();
        if (!params.contains("reader"))
            return outgoing({{"error", "protocol"}});
        if (!readers.contains(params.value("reader").toString()))
            return outgoing({{"error", QtPCSC::errorName(SCARD_E_READER_UNAVAILABLE)}});
        QPCSCReader *r = readers[params.value("reader").toString()];
        r->disconnect();
    } else if (message.contains("SCardTransmit")) {
        auto params = message.value("SCardTransmit").toMap();
        if (!params.contains("reader") || !params.contains("bytes"))
            return outgoing({{"error", "protocol"}});
        if (!readers.contains(params.value("reader").toString()))
            return outgoing({{"error", QtPCSC::errorName(SCARD_E_READER_UNAVAILABLE)}});
        QPCSCReader *r = readers[params.value("reader").toString()];
        r->transmit(QByteArray::fromBase64(params.value("bytes").toString().toLatin1()));
    } else if (message.contains("SCardReconnect")) {
        auto params = message.value("SCardReconnect").toMap();
        if (!params.contains("reader") || !params.contains("protocol"))
            return outgoing({{"error", "protocol"}});
        if (!readers.contains(params.value("reader").toString()))
            return outgoing({{"error", QtPCSC::errorName(SCARD_E_READER_UNAVAILABLE)}});
        QPCSCReader *r = readers[params.value("reader").toString()];
        r->reconnect(params.value("protocol").toString());
    } else if (message.contains("sign")) {
        QVariantMap sign = message.value("sign").toMap();
        const QByteArray cert = QByteArray::fromBase64(sign.value("certificate").toString().toLatin1());
        const QByteArray hash = QByteArray::fromBase64(sign.value("hash").toString().toLatin1());
        connect(PKI, &QPKI::signature, this, [this] (const WebContext *context, const CK_RV result, const QByteArray &value) {
            if (this != context) {
                _log("Not us, ignore");
                return;
            }
            disconnect(PKI, &QPKI::signature, this, 0);
            if (result == CKR_OK) {
                outgoing({{"signature", value.toBase64()}});
            } else {
                outgoing({{"error", QPKI::errorName(result)}});
            }
        });
        PKI->sign(this, cert, hash, QStringLiteral("SHA-256")); // FIXME: signature
    } else if (message.contains("certificate")) {
        connect(PKI, &QPKI::certificate, this, [this] (const WebContext *context, const CK_RV result, const QByteArray &value) {
            if (this != context) {
                _log("Not us, ignore");
                return;
            }
            disconnect(PKI, &QPKI::certificate, this, 0);
            if (result == CKR_OK) {
                outgoing({{"certificate", value.toBase64()}});
            } else {
                outgoing({{"error", QPKI::errorName(result)}});
            }
        });
        PKI->select(this, Signing);
    } else if (message.contains("authenticate")) {
        QVariantMap auth = message.value("authenticate").toMap();

        // TODO: Select certificate if needed
        connect(PKI, &QPKI::certificate, this, [this, auth] (const WebContext *context, const CK_RV result, const QByteArray &value) {
            const QString nonce = auth.value("nonce").toString().toLatin1();
            if (this != context) {
                _log("Not us, ignore");
                return;
            }
            disconnect(PKI, &QPKI::certificate, this, 0);
            if (result == CKR_OK) {
                // We have the certificate
                QByteArray jwt_token = QPKI::authenticate_dtbs(QSslCertificate(value, QSsl::Der), context->origin, nonce);
                QByteArray hash = QCryptographicHash::hash(jwt_token, QCryptographicHash::Sha256);
                connect(PKI, &QPKI::signature, this, [this, jwt_token] (const WebContext* ctx, const CK_RV rv, const QByteArray& val) {
                    if (this != ctx) {
                        _log("Not us, ignore");
                        return;
                    }
                    disconnect(PKI, &QPKI::signature, this, 0);
                    if (rv == CKR_OK) {
                        QByteArray token = jwt_token + "." + val.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
                        outgoing({{"token", QString(token)}, {"type", "JWT"}});
                    } else {
                        outgoing({{"error", QPKI::errorName(rv)}});
                    }
                });
                PKI->sign(this, value, hash, QStringLiteral("SHA-256"));
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

void WebContext::terminate() {
    if (ws) {
        ws->abort();
    } else if(ls) {
        ls->abort();
    }
}

QString WebContext::friendlyOrigin() const {

    QUrl url(origin);
    if (url.scheme() == "file") {
        return "localhost";
    } else {
        if (url.scheme() == "https" && url.port(443) != 443) {
            return  QString("%1:%2").arg(url.host()).arg(url.port(443));
        } else if (url.scheme() == "http" && url.port(80) != 80) {
            return QString("%1:%2").arg(url.host()).arg(url.port(80));
        } else {
            return url.host();
        }
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

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

WebContext::WebContext(QObject *parent, QLocalSocket *client) {
    this->ls = client;
    connect(client, &QLocalSocket::readyRead, [this, client] {
        _log("Handling data from local socket");
        quint32 msgsize = 0;
        if (client->read((char*)&msgsize, sizeof(msgsize)) == sizeof(msgsize)) {
            _log("Read message size: %d bytes", msgsize);
            QByteArray msg(int(msgsize), 0);
            if (client->read(msg.data(), msgsize) == msgsize) {
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
                _log("Could not read message");
                terminate();
            }
        } else {
            _log("Could not read message size");
            terminate();
        }
    });
    connect(client, &QLocalSocket::disconnected, [this, client] {
        _log("Local client disconnected (%s)", qPrintable(this->origin));
        deleteLater();
    });
}

WebContext::WebContext(QObject *parent, QWebSocket *client) {
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
        deleteLater();
    });
}


// Messages from main application. This means
void WebContext::receiveIPC(const InternalMessage &message) {
    _log("Received message from main");
    if (message.type == Authenticate) {
        if (message.error()) {
            _log("Auth failed");
            outgoing(message.data);
        }
    }
}

// Process a message from a browsing context
void WebContext::processMessage(const QVariantMap &message) {
    _log("Processing message");
    QVariantMap resp;

    // Current message ID
    msgid = message.value("id").toString();

    // Origin. If unset for context, set
    // Check if origin is secure
    QUrl url(message.value("origin").toString());
    // https, file or localhost
    if (url.scheme() == "https" || url.scheme() == "file" || url.host() == "localhost" || url.scheme() == "moz-extension" || url.scheme() == "chrome-extension") {
        origin = message.value("origin").toString();
    } else {
        // FIXME: response ? Drop connection?
    }

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

    // Command dispatch
    if (message.contains("version")) {
        resp = {{"id", msgid}, {"version", VERSION}}; // TODO: add something here
    } else if (message.contains("SCardConnect")) {
//        emit connect_reader(message.value("SCardConnect").toMap().value("protocol").toString());
    } else if (message.contains("SCardDisconnect")) {
//        emit disconnect_reader();
    } else if (message.contains("SCardTransmit")) {
//        emit send_apdu(QByteArray::fromHex(message.value("SCardTransmit").toMap().value("bytes").toString().toLatin1()));
    } else if (message.contains("sign")) {
        QVariantMap sign = message.value("sign").toMap();
//        emit sign(origin, QByteArray::fromBase64(sign.value("cert").toString().toLatin1()), QByteArray::fromBase64(sign.value("hash").toString().toLatin1()), sign.value("hashalgo").toString());
    } else if (message.contains("cert")) {
//        emit select_certificate(origin, Signing, false);
    } else if (message.contains("auth")) {
        return emit sendIPC(authenticate(message));
    } else {
        resp = {{"error", "protocol"}};
    }
    if (!resp.empty()) {
        outgoing(resp);
    }
    // Otherwise wait for a response from other threads
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
}

QString WebContext::friendlyOrigin() {

    QUrl url(origin);

    if (url.scheme() == "file") {
        return "localhost";
    } else {
        return url.host();
    }

}

// Messages
InternalMessage WebContext::authenticate(const QVariantMap &data) {
    InternalMessage m;
    m.type = Authenticate;
    m.data = data;
    return m;
}

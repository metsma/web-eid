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


WebContext::WebContext(QObject *parent, const QString &origin) {

}

WebContext::WebContext(QObject *parent, QLocalSocket *client) {
    this->ls = client;
    connect(client, &QLocalSocket::readyRead, [this, client] {
        _log("Handling data from socket");
        quint32 msgsize = 0;
        if (client->read((char*)&msgsize, sizeof(msgsize)) == sizeof(msgsize)) {
            _log("Reading  message of %d bytes", msgsize);
            QByteArray msg(int(msgsize), 0);
            if (client->read(msg.data(), msgsize) == msgsize) {
                _log("Read message of %d bytes", msgsize);
                // Make JSON
                QJsonObject jo = QJsonDocument::fromJson(msg).object();
                QVariantMap json = jo.toVariantMap();
                // re-serialize msg
                QByteArray response =  QJsonDocument::fromVariant(json).toJson();
                _log("Read message: %s", response.constData());
                // now call processing
                processMessage(json);
            } else {
                _log("Could not read message");
                client->abort();
            }
        } else {
            _log("Could not read message size");
            client->abort();
        }
    });
    connect(client, &QLocalSocket::disconnected, [this, client] {
        _log("Local client disconnected");
    });
}

WebContext::WebContext(QObject *parent, QWebSocket *client) {
    this->ws = client;
    connect(client, &QWebSocket::textMessageReceived, [this, client] (QString message) {
        _log("Message received from %s", qPrintable(client->origin()));
        QJsonObject jo = QJsonDocument::fromJson(message.toUtf8()).object();
        QVariantMap json = jo.toVariantMap();
        // re-serialize msg
        QByteArray response =  QJsonDocument::fromVariant(json).toJson();
        _log("Read message: %s", response.constData());

        processMessage(json);
    });
    connect(client, &QWebSocket::disconnected, [this, client] {
        _log("%s disconnected", qPrintable(client->origin()));
    });
}


void WebContext::processMessage(const QVariantMap &message) {
    _log("Processing message");
    QVariantMap resp;

    // FIXME: move to server
//    if (json.isEmpty() || !json.contains("id")) {
//        // Do nothing, as we can not reply
//        resp = {{"error", "protocol"}, {"version", VERSION}};
//        write(resp);
//    }

    msgid = message.value("id").toString();

    // Origin. If unset for instance, set
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
//   if (origin != json.value("origin").toString()) {
//       // Otherwise if already set, it must match
//       resp = {{"error", "protocol"}};
//       write(resp);
//       return shutdown(EXIT_FAILURE);
//   }

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
//        emit authenticate(origin, message.value("auth").toMap().value("nonce").toString());
    } else {
        resp = {{"error", "protocol"}};
    }
    if (!resp.empty()) {
        outgoing(resp);
    }
}


void WebContext::outgoing(QVariantMap &message) {

    QByteArray response =  QJsonDocument::fromVariant(message).toJson();
    _log("Sending outgoing message %s", response.constData());
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
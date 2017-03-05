#pragma once

#include "pkcs11module.h"
#include "qt_host.h"

#include <QSslCertificate>

class Authenticate
{
public:
    static QVariantMap authenticate(QtHost *h, const QJsonObject &msg);
    static QByteArray authenticate_dtbs(const QSslCertificate &cert, const QString &origin, const QString &nonce);
};

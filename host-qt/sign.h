#pragma once

#include "pkcs11module.h"
#include "qt_host.h"

class Sign
{
public:
    static QVariantMap sign(QtHost *h, const QJsonObject &msg);
    static QVariantMap select(QtHost *h, const QJsonObject &msg);
};

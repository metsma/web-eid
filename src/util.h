/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once
// silence the warnings for now TODO
#ifdef __clang__
#pragma clang system_header
#endif
#ifdef __GNUC__
#pragma GCC system_header
#endif

#include <vector>
#include <QSslCertificate>
#include <stdexcept>

static const std::vector<unsigned char> ba2v(const QByteArray &data) {
    return std::vector<unsigned char>(data.cbegin(), data.cend());
}

static const QByteArray v2ba(const std::vector<unsigned char> &data) {
    return QByteArray((const char*)data.data(), int(data.size()));
}

static const QSslCertificate v2cert(const std::vector<unsigned char> &data) {
    return QSslCertificate(QByteArray::fromRawData((const char*)data.data(), int(data.size())), QSsl::Der);
}

static const std::string toHex(const std::vector<unsigned char> &data) {
    return QByteArray::fromRawData((const char*)data.data(), int(data.size())).toHex().toStdString();
}

static const std::string x509subject(const std::vector<unsigned char> &c) {
    QSslCertificate cert(QByteArray::fromRawData((const char *)c.data(), int(c.size())), QSsl::Der);
    std::string result;
    QList<QString> cn = cert.subjectInfo(QSslCertificate::CommonName);
    if (cn.size() > 0)
        result.append(cn.at(0).toStdString());

    QList<QString> ou = cert.subjectInfo(QSslCertificate::OrganizationalUnitName);
    if (ou.size() > 0) {
        result.append(" ");
        result.append(ou.at(0).toStdString());
    }
    return result;
}

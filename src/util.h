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

static const std::vector<unsigned char> hex2v(const std::string &hex) {
    if (hex.size() % 2 == 1)
        throw std::invalid_argument("Hex count is odd");

    std::vector<unsigned char> bin(hex.size() / 2, 0);
    unsigned char *c = &bin[0];
    const char *h = hex.c_str();
    while (*h) {
        int x;
        sscanf(h, "%2X", &x);
        *c = x;
        c++;
        h += 2;
    }
    return bin;
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

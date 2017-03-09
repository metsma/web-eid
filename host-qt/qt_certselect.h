/*
 * Chrome Token Signing Native Host
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

#include <QDialog>

#include <vector>

class QTreeWidget;

class QtCertSelect {
public:
    static std::vector<unsigned char> getCert(const std::vector<std::vector<unsigned char>> &certs, const QString &origin, bool signing);
};

class QtCertSelectDialog: public QDialog {
    Q_OBJECT
public:
    QtCertSelectDialog(const QList<QStringList> &certs, const QString &origin, bool signing); // FIXME bool->enum
    QTreeWidget *table;
};

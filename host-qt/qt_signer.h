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

#include "pkcs11module.h"

#include <QDialog>

class QLabel;
class QLineEdit;

class QtSigner {
public:
   static std::vector<unsigned char> sign(const PKCS11Module &m, const std::vector<unsigned char> &hash, const std::vector<unsigned char> &cert);
};

class QtSignerDialog : public QDialog {
public:
   QtSignerDialog(P11Token &token);
   QLineEdit *pin = nullptr;
   QLabel *nameLabel, *pinLabel, *errorLabel;
};

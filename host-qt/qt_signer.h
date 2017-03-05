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

#pragma once

#include <QDialog>

#include <vector>

class QTreeWidget;

class QtCertSelect {
public:
    static std::vector<unsigned char> getCert(const std::vector<std::vector<unsigned char>> &certs);
};

class QtCertSelectDialog: public QDialog {
public:
    QtCertSelectDialog(const QList<QStringList> &certs);
    QTreeWidget *table;
};

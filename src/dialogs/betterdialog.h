#pragma once
#include <QDialog>

#ifdef Q_OS_MAC
void nshideapp();
#endif
class BetterDialog: public QDialog {
public:
    ~BetterDialog() {
#ifdef Q_OS_MAC
        nshideapp();
#endif
    }
};
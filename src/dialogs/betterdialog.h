#pragma once
#include <QDialog>

#ifdef Q_OS_MAC
void nshideapp(bool);
#endif
class BetterDialog: public QDialog {
public:
    ~BetterDialog() {
#ifdef Q_OS_MAC
        nshideapp(false);
#endif
    }
};
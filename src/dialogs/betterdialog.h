#pragma once
#include <QDialog>
#include <QApplication>
#include <QDesktopWidget>

#ifdef Q_OS_MAC
void nshideapp(bool);
void nsshowapp();
#endif
class BetterDialog: public QDialog {
public:
    BetterDialog() {
#ifdef Q_OS_MAC
        nsshowapp();
#endif
    }
    ~BetterDialog() {
#ifdef Q_OS_MAC
        nshideapp(false);
#endif
    }
    void centrify(bool h = true, bool v = true) {
        // Position in center of the screen
        adjustSize();
        QRect screen = QApplication::desktop()->screenGeometry();
        int newx = (screen.width() - width()) / 2;
        int newy = (screen.height() - height()) / 2;
        move(h ? newx : x(), v ? newy : y());
    }
};
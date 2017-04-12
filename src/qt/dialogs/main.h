#pragma once

#include "Logger.h"

#include <QDialog>
#include <QLabel>
#include <QHBoxLayout>
#include <QDialogButtonBox>

class MainDialog: public QDialog {
    Q_OBJECT

public:
    MainDialog():
        layout(new QHBoxLayout(this)),
        message(new QLabel(this))
    {
        layout->addWidget(message);
        setWindowTitle(QStringLiteral("Web eID"));
        message->setText(tr("Web eID app"));
    };

    void showit() {
        show();
    }
private:
    QHBoxLayout *layout;
    QLabel *message;
};

/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "dialogs/betterdialog.h"
#include <QLabel>
#include <QPixmap>
#include <QIcon>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTimer>

// About Web eID
class WinOpNotice: public BetterDialog {
    Q_OBJECT

public:
    WinOpNotice():
        layout(new QVBoxLayout(this)),
        text(new QLabel(this))
    {
        layout->addWidget(text);
        layout->setSizeConstraint(QLayout::SetFixedSize);

        setWindowTitle(tr("Working ..."));
        setWindowFlags(Qt::WindowStaysOnTopHint);
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));

        text->setText(tr("Windows CryptoAPI"));
      
    };

    void display(const QString &message = QString()) {
        if (!message.isEmpty())
            text->setText(message);
        adjustSize();
        show();
        raise();
        activateWindow();
    }
private:
    QVBoxLayout *layout;
    QLabel *text;
};

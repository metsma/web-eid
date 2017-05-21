/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "dialogs/betterdialog.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>

#include "qpcsc.h"

// Insert card to reader dialog
class QtInsertCard: public BetterDialog {
    Q_OBJECT

public:
    QtInsertCard(const QString &origin, QPCSCReader *reader):
        layout(new QVBoxLayout(this)),
        buttons(new QDialogButtonBox(this)),
        message(new QLabel(this)),
        reader(reader)
    {
        layout->addWidget(message);
        layout->addWidget(buttons);

        setWindowTitle(origin);
        message->setText(tr("Insert card into reader %1").arg(reader->name));

        setWindowFlags(Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_DeleteOnClose);

        // remove minimize and maximize buttons
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);

        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        show();
        raise();
        activateWindow();
    };

    void cardInserted(const QString &reader, const QByteArray &atr, const QStringList &flags) {
        if (this->reader->name == reader) {
            if (flags.contains("MUTE")) {
                message->setText(tr("Inserted card can not be used, please check the card"));
            } else {
                accept();
                this->reader->open();
            }
        }
    }

    void cardRemoved(const QString &reader) {
        if (this->reader->name == reader) {
           message->setText(tr("Insert card into reader %1").arg(reader));
        }
    }

private:
    QVBoxLayout *layout;
    QDialogButtonBox *buttons;
    QLabel *message;
    QPCSCReader *reader;
};

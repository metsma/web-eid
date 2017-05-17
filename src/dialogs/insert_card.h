/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "dialogs/betterdialog.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>

// Insert card to reader dialog
class QtInsertCard: public BetterDialog {
    Q_OBJECT

public:
    QtInsertCard(const QString &origin, const QString &reader):
        layout(new QVBoxLayout(this)),
        buttons(new QDialogButtonBox(this)),
        message(new QLabel(this))
    {
        layout->addWidget(message);
        layout->addWidget(buttons);

        setWindowTitle(reader);
        message->setText(tr("Insert card into reader %1 to be used on %2").arg(reader).arg(origin));

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
private:
    QVBoxLayout *layout;
    QDialogButtonBox *buttons;
    QLabel *message;
};

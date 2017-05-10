/*
 * Web eID app, (C) 2017 Web eID team and contributors
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

#include "Logger.h"

#include "qpcsc.h"

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>

// Insert card to reader dialog
// TODO: possibly merge with "reader in use" dialog"
class QtInsertCard: public QDialog {
    Q_OBJECT

public:
    QtInsertCard():
        layout(new QVBoxLayout(this)),
        buttons(new QDialogButtonBox(this)),
        message(new QLabel(this))
    {
        layout->addWidget(message);
        layout->addWidget(buttons);
        setWindowFlags(Qt::WindowStaysOnTopHint);
        // remove minimize and maximize buttons
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        // If the dialog is closed/cancelled, trigger cancel signal with the context in use
        connect(this, &QDialog::rejected, this, &QtInsertCard::trigger_cancel);
    };
    void showit(const QString &origin, const QString &reader, SCARDCONTEXT ctx) {
        this->ctx = ctx;
        setWindowTitle(origin);
        message->setText(tr("Insert card into reader %1 to be used by %2.").arg(reader).arg(origin));

        show();
        // Make sure window is in foreground and focus
        raise();
        activateWindow();
    }
private:
    QVBoxLayout *layout;
    QDialogButtonBox *buttons;
    QLabel *message;
    SCARDCONTEXT ctx;

public slots:
    void trigger_cancel() {
        emit cancel_insert(ctx);
    }
signals:
    void cancel_insert(SCARDCONTEXT ctx);
};

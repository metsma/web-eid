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
class QtInsertCard: public QDialog {
    Q_OBJECT

public:
    QtInsertCard(const QString &reader):
        layout(new QVBoxLayout(this)),
        buttons(new QDialogButtonBox(this)),
        message(new QLabel(this))
    {
        layout->addWidget(message);
        layout->addWidget(buttons);
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowFlags(Qt::WindowStaysOnTopHint);
        // remove minimize and maximize buttons
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        setWindowTitle(reader);
        message->setText(tr("Insert card into reader %1").arg(reader));

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

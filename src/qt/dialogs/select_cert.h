/*
 * Chrome Token Signing Native Host
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

#include "Common.h"
#include "util.h"
#include "pkcs11.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>


class QtCertSelect: public QDialog {
    Q_OBJECT

public:

    void getCert(const std::vector<std::vector<unsigned char>> &certs, const QString &origin, CertificatePurpose type) {
        std::vector<unsigned char> result;
        // Construct the list that is shown to the user.
        for (const std::vector<unsigned char> &c: certs) {
            QSslCertificate cert = v2cert(c);
            // filter out expired certificates
            // TODO: not here
            if (QDateTime::currentDateTime() >= cert.expiryDate())
                continue;
            table->insertTopLevelItem(0, new QTreeWidgetItem(table, QStringList{
                cert.subjectInfo(QSslCertificate::CommonName).at(0),
                cert.subjectInfo(QSslCertificate::Organization).at(0),
                cert.expiryDate().toString("dd.MM.yyyy"),
                QString::number(&c - &certs[0])})); // Index of certs list
        }

        table->setCurrentIndex(table->model()->index(0, 0));

        setWindowTitle(tr("Select certificate for %1 on %2").arg(type == Signing?tr("signing"):tr("authentication")).arg(origin));
        message->setText(tr("Selected certificate will be forwarded to the remote website"));
        show();
        // Make sure window is in foreground and focus
        raise();
        activateWindow();
        if (exec() == 0) {
            return emit cert_selected(CKR_FUNCTION_CANCELED, 0, type);
        }
        emit cert_selected(CKR_OK, v2ba(certs[table->currentItem()->text(3).toUInt()]), type);
    }


    QtCertSelect():
        layout(new QVBoxLayout(this)),
        message(new QLabel(this)),
        table(new QTreeWidget(this)),
        buttons(new QDialogButtonBox(this))
    {
        layout->addWidget(message);
        layout->addWidget(table);
        layout->addWidget(buttons);

        setWindowFlags(Qt::WindowStaysOnTopHint);
        // remove minimize and maximize buttons
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));

        table->setColumnCount(3);
        table->setRootIsDecorated(false);
        table->setHeaderLabels({tr("Certificate"), tr("Type"), tr("Valid to")});
        table->header()->setStretchLastSection(false);
        table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->header()->setSectionResizeMode(0, QHeaderView::Stretch);

        QPushButton *ok = buttons->addButton(tr("Select"), QDialogButtonBox::AcceptRole);
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(table, &QTreeWidget::clicked, [=] {
            ok->setEnabled(true);
        });
    }

signals:
    void cert_selected(CK_RV status, QByteArray cert, CertificatePurpose purpose);

private:
    QVBoxLayout *layout;
    QLabel *message;
    QTreeWidget *table;
    QDialogButtonBox *buttons;

};


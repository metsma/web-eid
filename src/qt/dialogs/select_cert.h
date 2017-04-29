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
#include "Common.h"
#include "util.h"
#include "pkcs11.h"
#include "qt_pki.h"
#include "context.h"

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

    QtCertSelect(WebContext *ctx, CertificatePurpose type, const std::vector<std::vector<unsigned char>> &certs):
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

        setAttribute(Qt::WA_DeleteOnClose);

        setWindowTitle(tr("Select certificate for %1 on %2").arg(type == Signing?tr("signing"):tr("authentication")).arg(ctx->friendlyOrigin()));
        message->setText(tr("Selected certificate will be forwarded to the remote website"));

        table->setColumnCount(3);
        table->setRootIsDecorated(false);
        table->setHeaderLabels({tr("Certificate"), tr("Type"), tr("Valid to")});
        table->header()->setStretchLastSection(false);
        table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->header()->setSectionResizeMode(0, QHeaderView::Stretch);

        QPushButton *ok = buttons->addButton(tr("Select"), QDialogButtonBox::AcceptRole);
        ok->setEnabled(false);

        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(table, &QTreeWidget::clicked, [&] {
            ok->setEnabled(true);
        });

        connect(this, &QDialog::finished, [this, ctx] (int code) {
            _log("Dialog is finished");
            if (code == QDialog::Rejected) {
                _log("Dialog was cancelled");
                // TODO: send message to ... where?
                // TO toPKI
                return emit sendIPC({CertificateSelected, {{"id", ctx->id}, {"error", QtPKI::errorName(CKR_FUNCTION_CANCELED)}}});

            }
//            _log("Selected %d for a status of %d", table->currentItem()->text(3).toUInt(), code);
//            v2ba(certs[table->currentItem()->text(3).toUInt()]);
        });

        show();
        activateWindow(); // to be always topmost and activated, on Linux
        raise(); // to be always topmost, on macOS
    }

public slots:
    // Called from PKI after QtPKI::refresh() when a card has been inserted and certificate list changes.
    void cert_list_updated(const std::vector<std::vector<unsigned char>> &certs) {
        // Change from some to none, interpret as implicit cancel
        // TODO: what about chaning cards?
        if (certs.empty())
            return reject();

        _log("Updating certificate list in window.");
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
    }

    void reject() {
        done(QDialog::Rejected);
    }

signals:
    void sendIPC(InternalMessage msg);

private:
    QVBoxLayout *layout;
    QLabel *message;
    QTreeWidget *table;
    QDialogButtonBox *buttons;
};


/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "Logger.h"
#include "Common.h"
#include "util.h"
#include "pkcs11.h"
#include "context.h"
#include "qpki.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>



class QtSelectCertificate: public QDialog {
    Q_OBJECT

public:

    QtSelectCertificate(WebContext *ctx, CertificatePurpose type):
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

        ok = buttons->addButton(tr("Select"), QDialogButtonBox::AcceptRole);
        ok->setEnabled(false);

        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(table, &QTreeWidget::clicked, [&] {
            ok->setEnabled(true);
        });

        connect(this, &QDialog::accepted, [this, ctx] {
            _log("Dialog is finished");
            return emit certificateSelected(certs[table->currentItem()->text(3).toUInt()]);
        });

    }

public slots:
    // Called from PKI after QtPKI::refresh() when a card has been inserted and certificate list changes.
    void update(const QVector<QByteArray> &certs) {
        // Change from some to none, interpret as implicit cancel
        // TODO: what about chaning cards?
        if (certs.empty()) {
            message->setText("No certificates found, please insert your card!");
        }

        table->clear();
        _log("Updating certificate list in window.");
        for (const auto &c: certs) {
            QSslCertificate cert(c, QSsl::Der);
            if (cert.isNull()) {
                _log("Could not parse certificate");
            }
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

        this->certs = certs;

        show();
        activateWindow(); // to be always topmost and activated, on Linux
        raise(); // to be always topmost, on macOS

    }

signals:
    void certificateSelected(const QByteArray &cert);

private:
    QVector<QByteArray> certs;
    QVBoxLayout *layout;
    QLabel *message;
    QTreeWidget *table;
    QDialogButtonBox *buttons;
    QPushButton *ok;
};


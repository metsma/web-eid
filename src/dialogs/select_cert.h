/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "qpki.h"

#include "Logger.h"
#include "Common.h"
#include "util.h"

#include "dialogs/betterdialog.h"

#include <QVBoxLayout>

#include <QStandardItemModel>
#include <QComboBox>

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>


class QtSelectCertificate: public BetterDialog {
    Q_OBJECT

public:

    QtSelectCertificate(const WebContext *ctx, const CertificatePurpose certtype):
        type(certtype),
        layout(new QVBoxLayout(this)),
        message(new QLabel(this)),
        select(new QComboBox(this)),
        buttons(new QDialogButtonBox(this))
    {
        layout->addWidget(message);
        layout->addWidget(select);
        layout->addWidget(buttons);

        layout->setSizeConstraint(QLayout::SetFixedSize);
        select->setFocusPolicy(Qt::StrongFocus);

        setWindowFlags(Qt::WindowStaysOnTopHint);
        // remove minimize and maximize buttons
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));

        setAttribute(Qt::WA_DeleteOnClose);

        setWindowTitle(ctx->friendlyOrigin());
        message->setText(tr("Selected certificate will be forwarded to the remote website"));

        ok = buttons->addButton(tr("Select"), QDialogButtonBox::AcceptRole);
        ok->setEnabled(false);
        ok->setFocusPolicy(Qt::StrongFocus);

        cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        cancel->setFocusPolicy(Qt::StrongFocus);

        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        connect(this, &QDialog::accepted, [this, ctx] {
            return emit certificateSelected(certs[select->currentIndex()]);
        });

        connect(select, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [=](int index) {
            ok->setEnabled(true);
            ok->setDefault(true);
            ok->setFocus();
        });
    }

public slots:
    // Called from PKI after QtPKI::refresh() when a card has been inserted and certificate list changes.
    void update(const QVector<QByteArray> &newcerts) {
        QVector<QByteArray> certs;
        // Filter by usage.
        for (const auto &c: newcerts) {
            if (QPKI::usageMatches(c, type)) {
                certs.append(c);
            }
        }

        // Change from some to none, interpret as implicit cancel
        // TODO: what about chaning cards?
        if (certs.empty()) {
            message->setText(tr("No certificates found, please insert your card!"));
            select->hide();
            ok->hide();
            cancel->setDefault(true);
            cancel->setFocus();
        } else {
            QStringList crts;
            message->setText(tr("Select certificate for XXX"));

            for (const auto &c: certs) {
                QString crt;

                QSslCertificate cert(c, QSsl::Der);
                if (cert.isNull()) {
                    _log("Could not parse certificate");
                    continue; // FIXME: fail
                }

                if (cert.subjectInfo(QSslCertificate::Organization).size() > 0) {
                    crt += cert.subjectInfo(QSslCertificate::Organization).at(0) + ": ";
                }
                crt += cert.subjectInfo(QSslCertificate::CommonName).at(0) + " ";
                crts << crt;
            }

            select->clear();
            select->show();
            select->addItems(crts);
            ok->show();
            ok->setDefault(true);
            if (select->currentIndex() != -1) {
                ok->setEnabled(true);
                ok->setDefault(true);
                ok->setFocus();
                cancel->setDefault(false);
            }

            ok->setFocus();
            this->certs = certs;
        }
        show();
        activateWindow(); // to be always topmost and activated, on Linux
        raise(); // to be always topmost, on macOS
    }

    void cardInserted(const QString &reader, const QByteArray &atr, const QStringList &flags) {
        (void)atr;
        if (flags.contains("MUTE")) {
            message->setText(tr("Inserted card is not usable. Please check.").arg(reader));
        } else {
            message->setText(tr("Card inserted, looking for certificates ..."));
        }
    }

    void noDriver(const QString &reader, const QByteArray &atr, const QByteArray &extra) {
        message->setText(tr("No driver available. <a href=\"http://smartcard-atr.appspot.com/parse?ATR=%1\">More information</a>").arg(QString(atr.toHex())));
        message->setTextFormat(Qt::RichText);
        message->setTextInteractionFlags(Qt::TextBrowserInteraction);
        message->setOpenExternalLinks(true);
        connect(message, &QLabel::linkActivated, this, [this] {
            reject();
        });
    }

signals:
    void certificateSelected(const QByteArray &cert);

private:
    CertificatePurpose type;
    QVector<QByteArray> certs;
    QVBoxLayout *layout;
    QLabel *message;
    QComboBox *select;
    QDialogButtonBox *buttons;
    QPushButton *ok;
    QPushButton *cancel;
};


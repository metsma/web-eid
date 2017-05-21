/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "qpcsc.h"
#include "Logger.h"
#include "context.h"

#include "dialogs/betterdialog.h"
#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QComboBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>

// Reader selection dialog
class QtSelectReader: public BetterDialog {
    Q_OBJECT

public:
    // FIXME: optional list of wanted ATR-s
    QtSelectReader(WebContext *ctx):
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
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(ctx->friendlyOrigin());

        message->setText("Reader will be made available to remote site!");
        ok = buttons->addButton(tr("Select"), QDialogButtonBox::AcceptRole);
        ok->setFocusPolicy(Qt::StrongFocus);
        ok->setEnabled(false);

        cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        cancel->setFocusPolicy(Qt::StrongFocus);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        connect(select, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), [=](const QString &text) {
            _log("New item is %s", qPrintable(text));
        });
        connect(this, &QDialog::accepted, [=] {
            _log("Selected reader %s", qPrintable(select->currentText()));
            emit readerSelected(select->currentText());
        });
        show();
    }

public slots:
    void update(QMap<QString, QStringList> readers) {
        if (readers.size() == 0) {
            message->setText(tr("Please connect a smart card reader!"));
            select->hide();
            ok->hide();
            cancel->setDefault(true);
            cancel->setFocus();
        } else if (readers.size() == 1) {
            ok->setText(tr("Allow"));
            ok->show();
            select->hide();
            QString reader = readers.keys().at(0);
            if (readers[reader].contains("EXCLUSIVE")) {
                message->setText(tr("%1 can not be used.\nIt is used exclusively by some other application").arg(reader));
                ok->setEnabled(false);
                cancel->setDefault(true);
                cancel->setFocus();
            } else {
                message->setText(tr("Allow access to %1?").arg(reader));
                cancel->setDefault(false);
                ok->setEnabled(true);
                ok->setDefault(true);
                ok->setFocus();
            }
        } else {
            select->clear();
            select->addItems(readers.keys());
            ok->setText(tr("Select"));
            ok->show();
            message->setText(tr("Please select a smart card reader to use"));
            // Disable readers
            QStandardItemModel* model = qobject_cast<QStandardItemModel*>(select->model());
            for (const auto &reader: readers.keys()) {
                _log("REader %s has %s", qPrintable(reader), qPrintable(readers[reader].join(",")));
                QStandardItem *item = model->findItems(reader).at(0);
                // Disable some elements, if necessary
                if (readers[reader].contains("EXCLUSIVE")) {
                    _log("Disabling combo %s", qPrintable(reader));
                    item->setEnabled(false);
                    item->setToolTip(tr("Reader is used in exclusive mode"));
                } else if ((select->currentIndex() == -1) || (selected == reader)) {
                    select->setCurrentText(reader);
                }
            }
            if (select->currentIndex() != -1) {
                ok->setEnabled(true);
                ok->setDefault(true);
                ok->setFocus();
                cancel->setDefault(false);
            }
            select->show();
        }
        activateWindow();
        raise();
    }

    void cardInserted(const QString &reader, const QByteArray &atr, const QStringList &flags) {
        (void)atr;
        // If a card is inserted while the dialog is open, we select the reader by default
        selected = reader;
        select->setCurrentText(selected);

        if (flags.contains("MUTE")) {
            message->setText(tr("Card inserted to %1 can not be used.\nPlease check the card").arg(reader));
        }
    }

    void cardRemoved(const QString &reader) {
        // Reset message after a possibly mute message
        message->setText(tr("Please select a smart card reader"));
        selected = reader;
    }

    void readerAttached(const QString &reader) {
        // If a new reader is attached while the dialog is open, we select it by default
        selected = reader;
    }

signals:
    void readerSelected(const QString &reader);

private:
    QVBoxLayout *layout;
    QLabel *message;
    QComboBox *select;
    QDialogButtonBox *buttons;
    QPushButton *ok;
    QPushButton *cancel;
    QString selected;
};

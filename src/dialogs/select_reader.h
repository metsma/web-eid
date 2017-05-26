/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "qpcsc.h"
#include "Logger.h"
#include "context.h"

#include "dialogs/betterdialog.h"
#include <QVBoxLayout>
#include <QApplication>
#include <QDesktopWidget>
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
    QtSelectReader(WebContext *ctx, QList<QByteArray> atrs):
        layout(new QVBoxLayout(this)),
        message(new QLabel(this)),
        select(new QComboBox(this)),
        buttons(new QDialogButtonBox(this)),
        atrs(atrs)
    {
        layout->addWidget(message);
        layout->addWidget(select);
        layout->addWidget(buttons);
        layout->setSizeConstraint(QLayout::SetFixedSize);
        select->setFocusPolicy(Qt::StrongFocus);
        select->setSizeAdjustPolicy(QComboBox::AdjustToContents);
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

        connect(select, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), [this] (const QString &text) {
            if (!text.isEmpty()) {
                _log("New item is %s", qPrintable(text));
                ok->setEnabled(true);
                ok->setDefault(true);
                ok->setFocus();
            }
        });
        connect(this, &QDialog::accepted, [this] {
            QString reader;
            if (select->count() > 1) {
                reader = select->currentText();
            } else {
                reader = selected;
            }
            _log("Selected reader %s", qPrintable(reader));
            emit readerSelected(reader);
        });
        centrify();
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
            selected.clear();
        } else if (readers.size() == 1) {
            ok->setText(tr("Allow"));
            ok->show();
            select->hide();
            QString reader = readers.keys().at(0);
            selected = reader;
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
            // Remove selected reader is not in list any more
            if (!readers.keys().contains(selected)) {
                selected.clear();
            }

            ok->setText(tr("Select"));
            ok->show();
            message->setText(tr("Please select a smart card reader to use"));
            // Disable readers
            QStandardItemModel* model = qobject_cast<QStandardItemModel*>(select->model());
            for (const auto &reader: readers.keys()) {
                _log("Reader %s has %s", qPrintable(reader), qPrintable(readers[reader].join(",")));
                QStandardItem *item = model->findItems(reader).at(0);
                // Disable some elements, if necessary
                if (readers[reader].contains("EXCLUSIVE")) {
                    _log("Disabling combo %s", qPrintable(reader));
                    item->setEnabled(false);
                    item->setToolTip(tr("Reader is in exclusive use by some other application"));
                } else if ((select->currentIndex()) == -1 || (selected == reader)) {
                    select->setCurrentText(reader);
                }
            }
            select->show();
        }

        centrify();

        // make focused
        activateWindow();
        raise();
    }

    void cardInserted(const QString &reader, const QByteArray &atr, const QStringList &flags) {
        (void)atr;
        // If a card is inserted while the dialog is open, we select the reader by default
        selected = reader;
        select->setCurrentText(reader);

        if (flags.contains("MUTE")) {
            message->setText(tr("Inserted card is not working.\nPlease check the card.").arg(reader));
        }
    }

    void cardRemoved(const QString &reader) {
        // Reset message after a possibly mute message
        message->setText(tr("Please select a smart card reader"));
        selected = reader;
        select->setCurrentText(reader);
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
    QList<QByteArray> atrs;
};

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
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>
#include <QTimeLine>

// Reader selection dialog
class QtSelectReader: public BetterDialog {
    Q_OBJECT

public:
    // FIXME: optional list of wanted ATR-s
    QtSelectReader(WebContext *ctx, QList<QByteArray> atrs):
        context(ctx),
        layout(new QVBoxLayout(this)),
        message(new QLabel(this)),
        select(new QComboBox(this)),
        remember(new QCheckBox(this)),
        buttons(new QDialogButtonBox(this)),
        atrs(atrs),
        autoaccept(new QTimeLine(3000, this))
    {
        remembered = settings.value(context->friendlyOrigin() + "/" + "reader").toString();
        oktext = tr("OK");
        layout->addWidget(message);
        layout->addWidget(select);
        remember->setText(tr("Always use this reader"));
        connect(remember, &QCheckBox::stateChanged, [this] (int state) {
            if (state == Qt::Unchecked) {
                autoaccept->stop();
                // FIXME: keep the OK text as a separate variable
                ok->setText(oktext);
            }
        });
        layout->addWidget(remember);
        layout->addWidget(buttons);
        layout->setSizeConstraint(QLayout::SetFixedSize);
        select->setFocusPolicy(Qt::StrongFocus);
        select->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        setWindowFlags(Qt::WindowStaysOnTopHint);
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(ctx->friendlyOrigin());

        ok = buttons->addButton(oktext, QDialogButtonBox::AcceptRole);
        ok->setFocusPolicy(Qt::StrongFocus);
        ok->setEnabled(false);

        cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        cancel->setFocusPolicy(Qt::StrongFocus);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        connect(autoaccept, &QTimeLine::finished, this, &QDialog::accept);

        connect(select, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), [this] (const QString &text) {
            if (!text.isEmpty()) {
                _log("New item is %s", qPrintable(text));
                ok->setEnabled(true);
                ok->setDefault(true);
                ok->setFocus();
                remember->setChecked(text == remembered);
                if (!this->atrs.isEmpty() && !readers[text].first.isEmpty()) {
                    if  (this->atrs.contains(readers[text].first)) {
                        message->setText(tr("The card in this reader is the expected card"));
                    } else {
                        message->setText(tr("The card in this reader is not the expected card"));
                    }
                } else {
                    message->setText(defaultmessage);
                }
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

            if (remember->isChecked()) {
                settings.beginGroup(context->friendlyOrigin());
                settings.setValue("reader", reader);
                settings.endGroup();
            } else if (!remembered.isEmpty() && !remember->isChecked()) {
                settings.remove(QStringLiteral("%1/reader").arg(context->friendlyOrigin()));
            }
            emit readerSelected(reader);
        });
        centrify();
        show();
    }

public slots:
    void update(QMap<QString, QPair<QByteArray, QStringList>> readers) {
        _log("Update reader in dialog");
        message->clear();
        select->clear();

        if (readers.size() == 0) {
            defaultmessage = tr("Please connect a smart card reader!");
            select->hide();
            remember->hide();
            ok->hide();
            cancel->setDefault(true);
            cancel->setFocus();
            selected.clear();
        } else if (readers.size() == 1) {
            oktext = tr("Allow");
            ok->show();
            select->hide();
            remember->show();
            QString reader = readers.keys().at(0);
            selected = reader;
            remember->setChecked(remembered == selected);

            if (readers[reader].second.contains("EXCLUSIVE")) {
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
            oktext = tr("Select");
            defaultmessage = tr("Please select a smart card reader to use");
            select->addItems(readers.keys());
            // Remove selected reader is not in list any more
            if (!readers.keys().contains(selected)) {
                selected.clear();
            }
            // Set reader if remembered
            if (readers.keys().contains(remembered)) {
                select->setCurrentText(remembered);
            }
            ok->show();
            // Disable readers
            QStandardItemModel* model = qobject_cast<QStandardItemModel*>(select->model());
            for (const auto &reader: readers.keys()) {
                _log("Reader %s has %s", qPrintable(reader), qPrintable(readers[reader].second.join(",")));
                QStandardItem *item = model->findItems(reader).at(0);
                // Disable some elements, if necessary
                if (readers[reader].second.contains("EXCLUSIVE")) {
                    _log("Disabling combo %s", qPrintable(reader));
                    item->setEnabled(false);
                    item->setToolTip(tr("Reader is in exclusive use by some other application"));
                } else {
                    if (!atrs.isEmpty() && atrs.contains(readers[reader].first)) {
                        message->setText(tr("This reader has the expected card"));
                        select->setCurrentText(reader);
                    }
                    if ((select->currentIndex()) == -1 || (selected == reader)) {
                        select->setCurrentText(reader);
                    }
                }
            }
            remember->setChecked(remembered == select->currentText());
            select->show();
        }
        ok->setText(oktext);
        if (message->text().isEmpty())
            message->setText(defaultmessage);
        if (remember->isChecked()) {
            ok->setText(QStringLiteral("%1 (3s)").arg(oktext));
            autoaccept->setCurveShape(QTimeLine::LinearCurve);
            autoaccept->setFrameRange(3, 0);
            connect(autoaccept, &QTimeLine::frameChanged, this, [this] (int frame) {
                ok->setText(QStringLiteral("%1 (%2s)").arg(oktext).arg(frame));
            });
            autoaccept->start();
        } else {
            autoaccept->stop();
        }
        // Use manual centrify to reduce "hopping"
        centrify();
        // make focused
        activateWindow();
        raise();
        // So that change event would know the ATR
        this->readers = readers;
    }

    void cardInserted(const QString &reader, const QByteArray &atr, const QStringList &flags) {
        // Update our view
        readers[reader].first = atr;
        readers[reader].second = flags;

        // Disable a reader as needed
        if (select->count() > 1) {
            QStandardItemModel* model = qobject_cast<QStandardItemModel*>(select->model());
            for (const auto &reader: readers.keys()) {
                _log("Reader %s has %s", qPrintable(reader), qPrintable(readers[reader].second.join(",")));
                QStandardItem *item = model->findItems(reader).at(0);
                // Disable some elements, if necessary
                if (readers[reader].second.contains("EXCLUSIVE")) {
                    _log("Disabling combo %s", qPrintable(reader));
                    item->setEnabled(false);
                    item->setToolTip(tr("Reader is in exclusive use by some other application"));
                } else {
                    item->setEnabled(true);
                    item->setToolTip(QString());
                }
            }
        } else {
            if (flags.contains("EXCLUSIVE")) {
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
        }
        // If it was a "exclusive" signal, do nothing
        if (flags.contains("EXCLUSIVE"))
            return;

        // If a card is inserted while the dialog is open, we select the reader by default
        selected = reader;
        select->setCurrentText(reader);
        if (!atrs.isEmpty()) {
            if (atrs.contains(atr)) {
                message->setText(tr("Inserted card is the expected card"));
            } else {
                message->setText(tr("Inserted card is not the expected card"));
            }
        } else {
            message->setText(defaultmessage);
        }

        if (flags.contains("MUTE")) {
            message->setText(tr("Inserted card is not working, please check the card."));
        }
    }

    void cardRemoved(const QString &reader) {
        // Update our view
        readers[reader].first.clear();
        readers[reader].second.clear();

        // Reset message after a possibly mute message
        message->setText(defaultmessage);
        // By default select the reader where a card was removed
        selected = reader;
        select->setCurrentText(reader);

        // But override if a usable card with the wanted ATR is present
        for (const auto &r: readers.keys()) {
            _log("Reader %s has %s", qPrintable(r), qPrintable(readers[r].second.join(",")));
            if (!atrs.isEmpty() && atrs.contains(readers[r].first) && !readers[r].second.contains("EXCLUSIVE")) {
                select->setCurrentText(r);
            }
        }
    }

    void readerAttached(const QString &reader) {
        // If a new reader is attached while the dialog is open, we select it by default
        selected = reader;
    }

signals:
    void readerSelected(const QString &reader);

private:
    WebContext *context;
    QVBoxLayout *layout;
    QLabel *message;
    QComboBox *select;
    QCheckBox *remember;
    QDialogButtonBox *buttons;
    QPushButton *ok;
    QPushButton *cancel;
    QString selected;
    QList<QByteArray> atrs;
    QSettings settings;
    QString remembered;
    QTimeLine *autoaccept;
    QString oktext;
    QString defaultmessage;
    QMap<QString, QPair<QByteArray, QStringList>> readers;
};

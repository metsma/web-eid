/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "qpcsc.h"
#include "Logger.h"
#include "context.h"

#include "dialogs/betterdialog.h"
#include <QTreeWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPushButton>

// Reader selection dialog
class QtSelectReader: public BetterDialog {
    Q_OBJECT

public:
    QtSelectReader(WebContext *ctx):
        layout(new QVBoxLayout(this)),
        message(new QLabel(this)),
        table(new QTreeWidget(this)),
        buttons(new QDialogButtonBox(this))
    {
        layout->addWidget(message);
        layout->addWidget(table);
        layout->addWidget(buttons);

        setWindowFlags(Qt::WindowStaysOnTopHint);
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(tr("Select reader for %1").arg(ctx->friendlyOrigin()));

        message->setText("Reader will be made available to remote site!");
        table->setColumnCount(1);
        table->setRootIsDecorated(false);
        table->setHeaderLabels({tr("Reader")});
        table->header()->setStretchLastSection(true);
        table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);


        ok = buttons->addButton(tr("Select"), QDialogButtonBox::AcceptRole);
        ok->setEnabled(false);

        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(table, &QTreeWidget::currentItemChanged, [=] (QTreeWidgetItem *item, QTreeWidgetItem *previous) {
            (void)previous;
            // TODO: only if reader can be used
            if (item) {
                selected = item->text(0);
                ok->setEnabled(true);
            }
        });

        connect(this, &QDialog::accepted, [=] {
            _log("Selected reader %s", qPrintable(table->currentItem()->text(0)));
            emit readerSelected(table->currentItem()->text(0));
        });
        show();
    }

public slots:
    void update(QMap<QString, QStringList> readers) {
        if (readers.size() == 0) {
            // No readers FIXME UX
            // FIXME: if started with empty list
            if (table->topLevelItemCount() > 0) {
                // if previous list had some
                return reject();
            } else {
                message->setText("Please connect a smart card reader!");
            }
        }
        table->clear();
        // set ok enabled only if previously selected reader is still in list.
        if (!readers.keys().contains(selected)) {
            selected.clear();
            ok->setEnabled(false);
        }
        for (const auto &r: readers.keys()) {
            QTreeWidgetItem *item = new QTreeWidgetItem(table, QStringList {r});
            if (readers[r].contains("EXCLUSIVE")) {
                item->setFlags(Qt::NoItemFlags);
                item->setForeground(0, QBrush(Qt::darkRed));
            } else if (readers[r].contains("EMPTY")) {
                item->setForeground(0, QBrush(Qt::darkGreen));
            }
            table->insertTopLevelItem(0, item);
            if (selected == r) {
                table->setCurrentItem(item);
            }
        }
        activateWindow();
        raise();
    }

    void cardInserted(const QString &reader, const QByteArray &atr) {
        (void)atr;
        // If a card is inserted while the dialog is open, we select the reader by default
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
    QTreeWidget *table;
    QDialogButtonBox *buttons;
    QPushButton *ok;
    QString selected;
};

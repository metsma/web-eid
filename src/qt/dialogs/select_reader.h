#pragma once

#include "pcsc.h"

#include <QDialog>
#include <QTreeWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPushButton>

// Reader selection dialog
// Used from main thread
class QtSelectReader: public QDialog {
    Q_OBJECT

public:
    // Called from main thread. Show the reader selection window
    void showit(const QString &origin, const QString &protocol, std::vector<PCSCReader> readers)
    {
        table->clear();
        setWindowTitle(tr("Select a reader for %1").arg(origin));
        // Geometry re-calculation
        // Construct the list that is shown to the user.
        for (const auto &r: readers)
        {
            QTreeWidgetItem *item = new QTreeWidgetItem(table, QStringList{
                QString::fromStdString(r.name),
                QString::number(&r - &readers[0])}); // Index of list
            // TODO: nicer UI
            if (r.exclusive) {
                item->setFlags(Qt::NoItemFlags);
                item->setForeground(0, QBrush(Qt::darkRed));
            }
            if (r.atr.empty()) {
                item->setForeground(0, QBrush(Qt::darkGreen));
            }
            table->insertTopLevelItem(0, item);
        }

        // FIXME: auto-select first usable reader
        table->setCurrentIndex(table->model()->index(0, 0));
        // FIXME: somebody please show how the fsck to work with Qt...

        table->resizeColumnToContents(0); // creates the horizontal scroll
        table->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        table->adjustSize();
        table->updateGeometry();
        //table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        // FIXME: can still be made smaller than the content of the table
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
        adjustSize();
        updateGeometry();

        resize(table->width() + 48, table->height());

        show();
        // Make sure window is in foreground and focus
        raise();
        activateWindow();
        if (exec() == 0) {
            return emit reader_selected(SCARD_E_CANCELLED, 0, 0);
        }
        return emit reader_selected(SCARD_S_SUCCESS, QString::fromStdString(readers[table->currentItem()->text(1).toUInt()].name), protocol);
    }

    // TODO: make this dialog react to reader state changes
    QtSelectReader():
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

        message->setText(tr("Selected reader will be made available to remote website"));
        table->setColumnCount(1);
        table->setRootIsDecorated(false);
        table->setHeaderLabels({tr("Reader")});
        table->header()->setStretchLastSection(true);
        table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        // table->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        ok = buttons->addButton(tr("Connect"), QDialogButtonBox::AcceptRole);
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(table, &QTreeWidget::clicked, [=] {
            // TODO: only if reader can be used
            ok->setEnabled(true);
        });
    }

signals:
    void reader_selected(const LONG status, const QString &reader, const QString &protocol);

private:
    QVBoxLayout *layout;
    QLabel *message;
    QTreeWidget *table;
    QDialogButtonBox *buttons;
    QPushButton *ok;
};



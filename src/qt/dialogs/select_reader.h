#pragma once

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
    // TODO: make this dialog react to reader state changes
    QtSelectReader(const QString &origin) {
        QLabel *message = new QLabel(this);
        QTreeWidget *table = new QTreeWidget(this);
        QDialogButtonBox *buttons = new QDialogButtonBox(this);
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(message);
        layout->addWidget(table);
        layout->addWidget(buttons);
        setWindowFlags(Qt::WindowStaysOnTopHint);
        // remove minimize and maximize buttons
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        setWindowTitle(tr("Select a reader for %1").arg(origin));
        message->setText(tr("Selected reader will be made available to remote website"));
        table->setColumnCount(1);
        table->setRootIsDecorated(false);
        table->setHeaderLabels({tr("Reader")});
        table->header()->setStretchLastSection(true);
        table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        // table->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        QPushButton *ok = buttons->addButton(tr("Select"), QDialogButtonBox::AcceptRole);
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(table, &QTreeWidget::clicked, [=]{
            // TODO: only if reader can be used
            ok->setEnabled(true);
        });
    }
};



#pragma once

#include "Logger.h"

#include "pcsc.h"

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>

// Insert card to reader dialog
// TODO: possibly merge with "reader in use" dialog"
class QtInsertCard: public QDialog {
    Q_OBJECT

public:
    QtInsertCard():
        layout(new QVBoxLayout(this)),
        buttons(new QDialogButtonBox(this)),
        message(new QLabel(this))
    {
        layout->addWidget(message);
        layout->addWidget(buttons);
        setWindowFlags(Qt::WindowStaysOnTopHint);
        // remove minimize and maximize buttons
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        // If the dialog is closed/cancelled, trigger cancel signal with the context in use
        connect(this, &QDialog::rejected, this, &QtInsertCard::trigger_cancel);
    };
    void showit(const QString &origin, const QString &reader, SCARDCONTEXT ctx) {
        this->ctx = ctx;
        setWindowTitle(origin);
        message->setText(tr("Insert card into reader %1 to be used by %2.").arg(reader).arg(origin));

        show();
        // Make sure window is in foreground and focus
        raise();
        activateWindow();
    }
private:
    QVBoxLayout *layout;
    QDialogButtonBox *buttons;
    QLabel *message;
    SCARDCONTEXT ctx;

public slots:
    void trigger_cancel() {
        emit cancel_insert(ctx);
    }
signals:
    void cancel_insert(SCARDCONTEXT ctx);
};

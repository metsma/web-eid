#pragma once

#include "Logger.h"

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>

class QtReaderInUse: public QDialog {
    Q_OBJECT

public:
    QtReaderInUse():
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
    };

    void showit(const QString &origin, const QString &reader) {
        setWindowTitle(origin);
        message->setText(tr("Reader %1 is used by %2.\nPress cancel to end access").arg(reader).arg(origin));

        show();
        // Make sure window is in foreground and focus
        raise();
        activateWindow();
    }
private:
        QVBoxLayout *layout;
        QDialogButtonBox *buttons;
        QLabel *message;
};

/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "dialogs/betterdialog.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QTextEdit>

class QtHost;

// Insert card to reader dialog
class QtDebugDialog: public BetterDialog {
    Q_OBJECT

public:
    QtDebugDialog(QtHost *mainapp):
        layout(new QVBoxLayout(this)),
        text(new QTextEdit(this)),
        buttons(new QDialogButtonBox(this))
    {
        layout->addWidget(text);
        layout->addWidget(buttons);

        setWindowTitle("Debug information");
        setWindowFlags(Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_DeleteOnClose);
        // remove minimize and maximize buttons
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        text->insertPlainText("Hello World!");
        show();
        raise();
        activateWindow();
    };
private:
    QVBoxLayout *layout;
    QTextEdit *text;
    QDialogButtonBox *buttons;
};

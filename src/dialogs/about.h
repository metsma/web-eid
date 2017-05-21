/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "dialogs/betterdialog.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>

class QtHost;


// About Web eID
class AboutDialog: public BetterDialog {
    Q_OBJECT

public:
    AboutDialog():
        layout(new QVBoxLayout(this)),
        text(new QLabel(this)),
        buttons(new QDialogButtonBox(this))
    {
        layout->addWidget(text);
        layout->addWidget(buttons);

        setWindowTitle("About Web eID app");
        setWindowFlags(Qt::WindowStaysOnTopHint);
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        setAttribute(Qt::WA_DeleteOnClose);
        // remove minimize and maximize buttons
        buttons->addButton(tr("OK"), QDialogButtonBox::AcceptRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        text->setText(tr("<h3>Web eID v%1</h3><div>Use your eID on the Web!</div><p>&copy; 2017 <a href=\"mailto:martin@martinpaljak.net\">Martin Paljak</a></p><p>More information on <a href=\"https://web-eid.com\">web-eid.com</a></p>").arg(VERSION));
        text->setAlignment(Qt::AlignCenter);
        text->setTextFormat(Qt::RichText);
        //text->setTextInteractionFlags(Qt::TextBrowserInteraction);
        text->setOpenExternalLinks(true);
        show();
        raise();
        activateWindow();
    };
private:
    QVBoxLayout *layout;
    QLabel *text;
    QDialogButtonBox *buttons;
};

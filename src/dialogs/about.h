/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "dialogs/betterdialog.h"
#include <QLabel>
#include <QSvgWidget>
#include <QPixmap>
#include <QIcon>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTimer>
#include <QSettings>
#include <QMouseEvent>

class QtHost;

// Clickable label
class SurpriseLabel: public QSvgWidget {
    Q_OBJECT
public:
    SurpriseLabel(const QString &file, QWidget *parent = Q_NULLPTR) : QSvgWidget(file, parent) {};
    ~SurpriseLabel() {};

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) {
        if(event->button() == Qt::RightButton) {
            emit clicked();
        }
    }
};

// About Web eID
class AboutDialog: public BetterDialog {
    Q_OBJECT

public:
    AboutDialog():
        layout(new QVBoxLayout(this)),
        img(new SurpriseLabel(":/web-eid.svg", this)),
        text(new QLabel(this)),
        revision(new QLabel(this)),
        buttons(new QDialogButtonBox(this))
    {
        layout->addWidget(img);
        img->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
        layout->setAlignment(img, Qt::AlignHCenter);
        layout->setAlignment(text, Qt::AlignTop);

        connect(img, &SurpriseLabel::clicked, [this] {
            counter++;
            if (counter == 3) {
                setWindowTitle(tr("Debug mode unlocked"));
                QSettings settings;
                settings.setValue("debug", true);
                revision->show();
            } else if (counter == 5) {
                setWindowTitle(tr("Almost there ..."));
            } else if (counter == 8) {
                setWindowTitle(tr("Supplies!"));
                text->setText(text->text() + "<p>Send me an e-mail with the window title<br>to get a free JavaCard for smart card development!</p>");
                centrify(true, false);
                QTimer::singleShot(3000, this, &QDialog::accept);
            }
        });

        layout->addWidget(text);
        layout->addWidget(revision);
        layout->addWidget(buttons);

        setWindowTitle(tr("About"));
#ifndef Q_OS_LINUX
        // We get a stock app icon otherwise
        setWindowIcon(QIcon());
#endif
        setSizeGripEnabled(false);
        setWindowFlags(Qt::WindowStaysOnTopHint);
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) &
                       ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        setAttribute(Qt::WA_DeleteOnClose);

        buttons->addButton(tr("OK"), QDialogButtonBox::AcceptRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        text->setText(tr("<h3>Web eID v%1</h3><div>Use your eID smart card on the Web!</div>").arg(VERSION));
        text->setText(text->text() + tr("<p>&copy; 2017 <a href=\"mailto:martin@martinpaljak.net\">Martin Paljak</a> & contributors</p>"));
        text->setText(text->text() + tr("<p>More information on <a href=\"https://web-eid.com\">web-eid.com</a></p>"));
        text->setAlignment(Qt::AlignCenter);
        text->setTextFormat(Qt::RichText);
        text->setOpenExternalLinks(true);

        revision->setText(tr("Built from %1").arg(GIT_REVISION));
        revision->setTextInteractionFlags(Qt::TextSelectableByMouse);
        revision->setAlignment(Qt::AlignCenter);
        revision->hide();
        QSettings settings;
        if (settings.value("debug", false).toBool())
            revision->show();
        centrify(true, true);
        show();
        raise();
        activateWindow();
    };
private:
    int counter = 0;
    QVBoxLayout *layout;
    SurpriseLabel *img;
    QLabel *text;
    QLabel *revision;
    QDialogButtonBox *buttons;
};

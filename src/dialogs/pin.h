/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include "pkcs11module.h"
#include "util.h" // helpers
#include "debuglog.h"

#include "context.h"

#include "dialogs/betterdialog.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTimeLine>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QUrl>

class QtPINDialog : public BetterDialog
{
    Q_OBJECT

public:
    QtPINDialog(const WebContext *context, const QByteArray &cert, const P11Token p11token, const CK_RV last, CertificatePurpose type):
        layout(new QVBoxLayout(this)),
        nameLabel(new QLabel(this)),
        pinLabel(new QLabel(this)),
        errorLabel(new QLabel(this)),
        // for pinpad
        progress(new QProgressBar(this)),
        buttons(new QDialogButtonBox(this)),
        pinlayout(new QHBoxLayout()),
        pin(new QLineEdit(this))

    {

        setAttribute(Qt::WA_DeleteOnClose);
        setMinimumWidth(400); // TODO: static sizing sucks
        // Force window to be topmost
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        // Remove minimize and maximize buttons from window chrome
        setWindowFlags((windowFlags() | Qt::CustomizeWindowHint) & ~(Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint));

        // Create buttons for PIN entry subdialog
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        ok = buttons->addButton(tr("OK"), QDialogButtonBox::ActionRole); // FIXME
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        // TODO: UX use a gray "eye" icon if possible, instead
        showpwd = new QPushButton(tr("Show"));
        connect(showpwd, &QPushButton::pressed, [=] {
            pin->setEchoMode(QLineEdit::Normal);
        });

        connect(showpwd, &QPushButton::released, [=] {
            pin->setEchoMode(QLineEdit::Password);
        });

        pinlayout->addWidget(showpwd);
        pinlayout->addWidget(pin);

        // Create elements for pinpad  subdialog
        progress->setTextVisible(false);

        statusTimer = new QTimeLine(progress->maximum() * 1000, this);
        statusTimer->setCurveShape(QTimeLine::LinearCurve);
        statusTimer->setFrameRange(progress->maximum(), progress->minimum());
        connect(statusTimer, &QTimeLine::frameChanged, progress, &QProgressBar::setValue);

        // Construct layout
        layout->addWidget(errorLabel);
        layout->addWidget(nameLabel);
        layout->addWidget(pinLabel);

        layout->addLayout(pinlayout);
        layout->addWidget(buttons);

        layout->addWidget(progress);

        // Set title
        if (type == Signing) {
            setWindowTitle(tr("Signing at %1").arg(context->friendlyOrigin()));
        } else {
            setWindowTitle(tr("Authenticating to %1").arg(context->friendlyOrigin()));
        }

        //nameLabel->setText(x509subject(cert).c_str());
        nameLabel->setText("serdi subjekt");
        pinLabel->setText(tr("Enter PIN for \"%1\"").arg(QString::fromStdString(p11token.label)));
        ok->setEnabled(false);
        ok->setText(type == Signing ? tr("Sign") : tr("Authenticate"));

        errorLabel->setTextFormat(Qt::RichText);
        errorLabel->hide();
        auto triesLeft = PKCS11Module::getPINRetryCount(p11token);
        if (triesLeft != 3 || last != CKR_OK) {
            errorLabel->setText(QString("<font color='red'><b>%1 tries left: %2</b></font>").arg(triesLeft).arg(PKCS11Module::errorName(last)));
            errorLabel->show();
        }
        if (p11token.has_pinpad) {
            showpwd->hide();
            pin->hide();
            buttons->hide();
            progress->show();

            // Show the spinner
            // TODO: restart spinner
            // 30 seconds of time.
            progress->setRange(0, 30);
            progress->setValue(progress->maximum());
            // Timeline for counting from 30 to 0
            statusTimer->setFrameRange(progress->maximum(), progress->minimum());
            statusTimer->start();
            show();
            raise();
            activateWindow();
            emit login(cert, nullptr);
        } else {
            // TODO: show/hide things
            progress->hide();
            showpwd->show();
            pin->show();
            buttons->show();

            pin->setEchoMode(QLineEdit::Password);
            pin->setText("");
            pin->setFocus();
            pin->setMaxLength(p11token.pin_max);

            // TODO: use validator
            connect(pin, &QLineEdit::textEdited, [=](const QString &text) {
                ok->setEnabled(text.size() >= p11token.pin_min);
            });
            connect(buttons, &QDialogButtonBox::clicked, [this, cert] (QAbstractButton *button) {
                if (ok == button) {
                    emit login(cert, pin->text());
                }
            });

            connect(pin, &QLineEdit::returnPressed, [this, cert] {
                _log("Currently we are %d", ok->isEnabled());
                if (ok->isEnabled()) {
                    emit login(cert, pin->text());
                } else {
                    pin->selectAll();
                    pin->setFocus();
                }
            });

            show();
            raise();
            activateWindow();
        }
    }

    // Called with result of C_Login
    void update(CK_RV rv) {
        if (rv == CKR_FUNCTION_CANCELED) {
            return reject();
        }
        if (rv == CKR_OK) {
            return accept();
        }
        if (rv == CKR_PIN_INCORRECT) {
            errorLabel->setText(QString("<font color='red'><b>%1</b></font>").arg(PKCS11Module::errorName(rv)));
            errorLabel->show();
            pin->clear();
        } else {
            hide();
            emit failed(rv);
            deleteLater();
        }
    }

signals:
    void login(const QByteArray &cert, const QString &pin);
    void failed(const CK_RV rv);


private:
    QVBoxLayout *layout;
    QLabel *nameLabel;
    QLabel *pinLabel;
    QLabel *errorLabel;

    QProgressBar *progress;
    QTimeLine *statusTimer;

    QDialogButtonBox *buttons;
    QHBoxLayout *pinlayout;
    QPushButton *showpwd;
    QLineEdit *pin = nullptr;
    QPushButton *ok;
};

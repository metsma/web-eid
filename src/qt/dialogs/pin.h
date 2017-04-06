/*
 * Chrome Token Signing Native Host
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "pkcs11module.h"
#include "util.h" // helpers
#include "Logger.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTimeLine>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QUrl>

#include <thread>

enum
{
    UserCancel = 0, // same as QDialog::Rejected
    TechnicalError = -1,
    AuthError = -2,
};

// TODO: rewrite dialogs for longrunning process

class QtPINDialog : public QDialog
{
    Q_OBJECT

public:
    // Called from main thread, thus we use exec() here
    void showit(CK_RV last, const P11Token &p11token, const std::vector<unsigned char> &cert, const QString &origin, CertificatePurpose type)
    {
        // Set title
        if (type == Signing) {
            setWindowTitle(tr("Signing at %1").arg(origin));
        } else {
            setWindowTitle(tr("Authenticating to %1").arg(origin));
        }

        nameLabel->setText(x509subject(cert).c_str());
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
            emit login(CKR_OK, nullptr, type);
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

            connect(pin, &QLineEdit::textEdited, [=](const QString &text) {
                ok->setEnabled(text.size() >= p11token.pin_min);
            });
            // Exec
            int dlg = exec();
            if (dlg == QDialog::Rejected) {
                _log("Rejected");
                emit login(CKR_FUNCTION_CANCELED, 0, type);
            } else if (dlg == QDialog::Accepted) {
                _log("PIN is %s", pin->text().toUtf8().toStdString().c_str());
                emit login(CKR_OK, pin->text(), type);
            }
        }
    }

    QtPINDialog() : layout(new QVBoxLayout(this)),
        nameLabel(new QLabel(this)),
        pinLabel(new QLabel(this)),
        errorLabel(new QLabel(this)),
        // for pinpad
        progress(new QProgressBar(this)),
        buttons(new QDialogButtonBox(this)),
        pinlayout(new QHBoxLayout()),
        pin(new QLineEdit(this))

    {
        // Create buttons for PIN entry subdialog
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
        ok = buttons->addButton(tr("OK"), QDialogButtonBox::AcceptRole); // FIXME
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
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
        setMinimumWidth(400); // TODO: static sizing sucks
        // Force window to be topmost
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        // Remove minimize and maximize buttons from window chrome
        setWindowFlags((windowFlags() | Qt::CustomizeWindowHint) & ~(Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint));
    }

signals:
    void login(CK_RV status, const QString &pin, CertificatePurpose purpose);

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

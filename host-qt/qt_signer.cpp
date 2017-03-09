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

#include "qt_signer.h"

#include "Common.h" // exceptions
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

enum {
    UserCancel = 0, // same as QDialog::Rejected
    TechnicalError = -1,
    AuthError = -2,
};

std::vector<unsigned char> QtSigner::sign(const PKCS11Module &m, const std::vector<unsigned char> &hash, const std::vector<unsigned char> &cert, const QString &origin, bool signing) {
        switch(hash.size())
        {
        case BINARY_SHA1_LENGTH:
        case BINARY_SHA224_LENGTH:
        case BINARY_SHA256_LENGTH:
        case BINARY_SHA384_LENGTH:
        case BINARY_SHA512_LENGTH: break;
        default:
            throw std::invalid_argument ("illegal hash size invalid_argument");
        }

        _log("signing %s with cert=%s", toHex(hash).c_str(), x509subject(cert).c_str());

        bool isInitialCheck = true;
        for (int retriesLeft = m.getPINRetryCount(cert); retriesLeft > 0; ) {
            P11Token token(m.getP11Token(cert));
            QtSignerDialog dialog(token, origin, signing);
            if (retriesLeft < 3) {
                dialog.errorLabel->show();
                dialog.errorLabel->setText(QString("<font color='red'><b>%1%2 %3</b></font>")
                     .arg(!isInitialCheck ? tr("Incorrect PIN ") : "")
                     .arg(tr("tries left:"))
                     .arg(retriesLeft));
            }
            isInitialCheck = false;
            dialog.nameLabel->setText(x509subject(cert).c_str());
            std::vector<unsigned char> signature;
            // This needs to be fixed. If user cancels the dialog, nothing happens until
            // the PKCS#11 call times out.

            if (m.isPinpad(cert)) {
                std::thread([&]{
                    try {
                        signature = m.sign(cert, hash, nullptr);
                        dialog.accept();
                    } catch (const AuthenticationError &) {
                        --retriesLeft;
                        dialog.done(AuthError);
                    } catch (const AuthenticationBadInput &) {
                        dialog.done(AuthError);
                    } catch (const UserCanceledError &) {
                        dialog.done(UserCancel);
                    } catch (const std::runtime_error &e) {
                        _log("Pinpad signing failed %s", e.what());
                        dialog.done(TechnicalError);
                    }
                }).detach();
            }

            int dialogresult = dialog.exec();
            switch (dialogresult)
            {
            case QDialog::Rejected:
                throw UserCanceledError();
            case AuthError:
                continue;
            case TechnicalError:
                throw std::runtime_error ("technical error in PIN dialog");
            default:
                if (m.isPinpad(cert)) {
                    return signature;
                }
            }
            try {
                if (!m.isPinpad(cert)) {
                    std::vector<unsigned char> result = m.sign(cert, hash, dialog.pin->text().toUtf8().constData());
                    return result;
                }
            } catch (const AuthenticationBadInput &) {
            } catch (const AuthenticationError &) {
                --retriesLeft;
            }
        }
        // FIXME: exception
        throw std::runtime_error("pin_blocked oh crap");
    }


    QtSignerDialog::QtSignerDialog(P11Token &p11token, const QString &origin, bool signing)
        : nameLabel(new QLabel(this))
        , pinLabel(new QLabel(this))
        , errorLabel(new QLabel(this))
    {
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(errorLabel);
        layout->addWidget(nameLabel);
        layout->addWidget(pinLabel);

        setMinimumWidth(400);
        // Force window to be topmost
        setWindowFlags(windowFlags()|Qt::WindowStaysOnTopHint);
        // Remove minimize and maximize buttons from window chrome
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) & ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));

        if (signing) {
            setWindowTitle(tr("Signing at %1").arg(origin));
        } else {
            setWindowTitle(tr("Authenticating to %1").arg(origin));
        }
        pinLabel->setText(tr("Enter PIN for token \"%1\"").arg(QString::fromStdString(p11token.label)));
        errorLabel->setTextFormat(Qt::RichText);
        errorLabel->hide();

        if(p11token.has_pinpad) {
            QProgressBar *progress = new QProgressBar(this);
            // 30 seconds of time.
            progress->setRange(0, 30);
            progress->setValue(progress->maximum());
            progress->setTextVisible(false);

            // Timeline for counting from 30 to 0
            QTimeLine *statusTimer = new QTimeLine(progress->maximum() * 1000, this);
            statusTimer->setCurveShape(QTimeLine::LinearCurve);
            statusTimer->setFrameRange(progress->maximum(), progress->minimum());
            connect(statusTimer, &QTimeLine::frameChanged, progress, &QProgressBar::setValue);
            statusTimer->start();
            // Show progress bar
            layout->addWidget(progress);
        } else {
            QDialogButtonBox *buttons = new QDialogButtonBox(this);
            connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
            buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
            QPushButton *ok = buttons->addButton(signing ? tr("Sign") : tr("Authenticate"), QDialogButtonBox::AcceptRole); // FiXME
            ok->setEnabled(false);

            pin = new QLineEdit(this);
            pin->setEchoMode(QLineEdit::Password);
            pin->setFocus();
            pin->setMaxLength(p11token.pin_max);
            connect(pin, &QLineEdit::textEdited, [=](const QString &text){
                ok->setEnabled(text.size() >= p11token.pin_min);
            });

            // TODO: UX use a gray "eye" icon if possible, instead
            QPushButton *showpwd = new QPushButton(tr("Show"));
            connect(showpwd, &QPushButton::pressed, [=]{
                pin->setEchoMode(QLineEdit::Normal);
            });

            connect(showpwd, &QPushButton::released, [=]{
                pin->setEchoMode(QLineEdit::Password);
            });

            QHBoxLayout *pinlayout = new QHBoxLayout();
            pinlayout->addWidget(showpwd);
            pinlayout->addWidget(pin);
            layout->addLayout(pinlayout);
            layout->addWidget(buttons);
        }
        show();
        // Make sure window is in foreground and focus
        raise();
        activateWindow();
    }
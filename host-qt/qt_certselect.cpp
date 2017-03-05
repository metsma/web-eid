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

#include "qt_certselect.h"

#include "Common.h"
#include "Labels.h"
#include "util.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

std::vector<unsigned char> QtCertSelect::getCert(const std::vector<std::vector<unsigned char>> &certs) {
        // what is returned.
        std::vector<unsigned char> result;

        // What is shown to user
        QList<QSslCertificate> usable;
        for (auto c: certs) {
            usable << v2cert(c);
        }

        // Construct the list that is shown to the user.
        QList<QStringList> uicerts;
        for (auto &cert: usable) {
            // filter out expired certificates and add to the funky list
            if (QDateTime::currentDateTime() < cert.expiryDate()) {
                uicerts << (QStringList()
                          << cert.subjectInfo(QSslCertificate::CommonName)
                          << cert.subjectInfo(QSslCertificate::Organization)
                          << cert.expiryDate().toString("dd.MM.yyyy")
                          << cert.toDer().toHex());
            }
        }
        QtCertSelectDialog dialog(uicerts);

        if (dialog.exec() == 0) {
            throw UserCanceledError();
        }
        // FIXME: this whole thing is ugly
        result = ba2v(QByteArray::fromHex(uicerts.at(dialog.table->currentIndex().row())[3].toUtf8()));
        return result;
    }


QtCertSelectDialog::QtCertSelectDialog(const QList<QStringList> &certs)
    : table(new QTreeWidget(this))
    {
        QLabel *message = new QLabel(this);
        QDialogButtonBox *buttons = new QDialogButtonBox(this);
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(message);
        layout->addWidget(table);
        layout->addWidget(buttons);

        setWindowFlags(Qt::WindowStaysOnTopHint);
        setWindowTitle(Labels::l10n.get("select certificate").c_str());
        message->setText(Labels::l10n.get("cert info").c_str());
        //remove minimize and maximize
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) & ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        table->setColumnCount(3);
        table->setRootIsDecorated(false);
        table->setHeaderLabels(QStringList()
                               << Labels::l10n.get("certificate").c_str()
                               << Labels::l10n.get("type").c_str()
                               << Labels::l10n.get("valid to").c_str());
        table->header()->setStretchLastSection(false);
        table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        for(const QStringList &row: certs)
            table->insertTopLevelItem(0, new QTreeWidgetItem(table, row));
        table->setCurrentIndex(table->model()->index(0, 0));

        QPushButton *ok = buttons->addButton(Labels::l10n.get("select").c_str(), QDialogButtonBox::AcceptRole);
        buttons->addButton(Labels::l10n.get("cancel").c_str(), QDialogButtonBox::RejectRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(table, &QTreeWidget::clicked, [=]{
            ok->setEnabled(true);
        });

        show();
        // Make sure window is in foreground and focus
        raise();
        activateWindow();
    }

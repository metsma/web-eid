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
#include "util.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

std::vector<unsigned char> QtCertSelect::getCert(const std::vector<std::vector<unsigned char>> &certs, const QString &origin, bool signing) {
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
        QtCertSelectDialog dialog(uicerts, origin, signing);

        if (dialog.exec() == 0) {
            throw UserCanceledError();
        }
        // FIXME: this whole thing is ugly
        result = ba2v(QByteArray::fromHex(uicerts.at(dialog.table->currentIndex().row())[3].toUtf8()));
        return result;
    }


QtCertSelectDialog::QtCertSelectDialog(const QList<QStringList> &certs, const QString &origin, bool signing)
    : table(new QTreeWidget(this))
    {
        QLabel *message = new QLabel(this);
        QDialogButtonBox *buttons = new QDialogButtonBox(this);
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(message);
        layout->addWidget(table);
        layout->addWidget(buttons);

        setWindowFlags(Qt::WindowStaysOnTopHint);
        // remove minimize and maximize buttons
        setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) & ~(Qt::WindowMaximizeButtonHint|Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint));
        setWindowTitle(tr("Select certificate for %1 on %2").arg(signing?tr("signing"):tr("authentication")).arg(origin));
        message->setText(tr("Selected certificate will be forwarded to the remote website"));
        table->setColumnCount(3);
        table->setRootIsDecorated(false);
        table->setHeaderLabels(QStringList()
                               << tr("Certificate")
                               << tr("Type")
                               << tr("Valid to"));
        table->header()->setStretchLastSection(false);
        table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        for(const QStringList &row: certs)
            table->insertTopLevelItem(0, new QTreeWidgetItem(table, row));
        table->setCurrentIndex(table->model()->index(0, 0));

        QPushButton *ok = buttons->addButton(tr("Select"), QDialogButtonBox::AcceptRole);
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
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
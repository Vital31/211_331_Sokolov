#include "dialog.h"
#include "ui_dialog.h"

#include <QRegularExpression>
#include <QMessageBox>

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &Dialog::onAccept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &Dialog::reject);
}

Dialog::~Dialog()
{
    delete ui;
}

QString Dialog::surname() const
{
    return ui->lineEditSurname->text().trimmed();
}

QString Dialog::name() const
{
    return ui->lineEditName->text().trimmed();
}

QString Dialog::passport() const
{
    return ui->lineEditPassport->text().trimmed();
}

void Dialog::onAccept()
{
    QRegularExpression passportRe("^\\d{10}$");

    if (surname().isEmpty() || name().isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Заполните фамилию и имя."));
        return;
    }

    if (!passportRe.match(passport()).hasMatch()) {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Номер паспорта должен содержать ровно 10 цифр."));
        return;
    }

    accept();
}

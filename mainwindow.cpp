#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QListWidgetItem>

#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Кнопка "Открыть..."
    connect(ui->pushButtonOpen, &QPushButton::clicked,
            this, &MainWindow::onOpenClicked);

    ui->labelStatus->setText(tr("Файл не загружен"));

    // Ищем persons.json в удобных местах
    QDir appDir(QCoreApplication::applicationDirPath());

    QStringList candidates = {
        appDir.filePath("persons.json"),        // рядом с .exe
        appDir.filePath("../persons.json"),     // на уровень выше
        appDir.filePath("../../persons.json")   // ещё на уровень выше (часто это корень проекта)
    };

    QString defaultPath;

    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            defaultPath = path;
            break;
        }
    }

    // если нигде не нашли, всё равно используем первый вариант — просто покажем "файл не найден"
    if (defaultPath.isEmpty())
        defaultPath = candidates.first();

    loadFromFile(defaultPath);
}

MainWindow::~MainWindow()
{
    delete ui;
}

//  СЛОТ "Открыть..."

void MainWindow::onOpenClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Открыть файл данных"),
        QString(),
        tr("JSON files (*.json);;All files (*.*)"));

    if (path.isEmpty())
        return;

    loadFromFile(path);
}

// ЗАГРУЗКА ФАЙЛА

void MainWindow::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.exists()) {
        if (currentFilePath_.isEmpty()) {
            ui->labelStatus->setText(tr("Файл не найден: %1").arg(filePath));
        }
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Не удалось открыть файл: %1").arg(filePath));
        return;
    }

    const QByteArray rawData = file.readAll();
    file.close();

    std::vector<PersonRecord> records;
    QString error;
    if (!parseJson(rawData, records, error)) {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Ошибка разбора JSON: %1").arg(error));
        return;
    }

    currentFilePath_ = filePath;
    processRecords(std::move(records));
}

// РАЗБОР JSON

bool MainWindow::parseJson(const QByteArray &data,
                           std::vector<PersonRecord> &records,
                           QString &errorMessage)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMessage = parseError.errorString();
        return false;
    }

    if (!doc.isArray()) {
        errorMessage = tr("Корневой элемент должен быть массивом");
        return false;
    }

    const QJsonArray arr = doc.array();
    if (arr.isEmpty()) {
        errorMessage = tr("Массив записей пуст");
        return false;
    }

    QRegularExpression passportRe("^\\d{10}$");

    records.clear();
    records.reserve(arr.size());

    for (const QJsonValue &val : arr) {
        if (!val.isObject()) {
            errorMessage = tr("Ожидался объект записи");
            return false;
        }

        const QJsonObject obj = val.toObject();

        PersonRecord rec;
        rec.surname   = obj.value("surname").toString();
        rec.name      = obj.value("name").toString();
        rec.passport  = obj.value("passport").toString();
        rec.createdAt = obj.value("createdAt").toString();
        rec.hash      = obj.value("hash").toString();

        if (rec.surname.isEmpty() ||
            rec.name.isEmpty() ||
            rec.passport.isEmpty() ||
            rec.createdAt.isEmpty() ||
            rec.hash.isEmpty())
        {
            errorMessage = tr("Не все поля записи заполнены");
            return false;
        }

        if (!passportRe.match(rec.passport).hasMatch()) {
            errorMessage = tr("Номер паспорта должен содержать ровно 10 цифр");
            return false;
        }

        records.push_back(rec);
    }

    return true;
}

// ВЫЧИСЛЕНИЕ ХЕША

QString MainWindow::computeHash(const PersonRecord &record,
                                const QString &previousHash) const
{
    // Формула: фамилия + имя + паспорт + дата + hash_(i-1)
    const QString payload =
        record.surname +
        record.name +
        record.passport +
        record.createdAt +
        previousHash;

    const QByteArray digest =
        QCryptographicHash::hash(payload.toUtf8(),
                                 QCryptographicHash::Sha256);

    return digest.toBase64(); // base64, как требует вариант
}

// ПРОВЕРКА ЦЕПОЧКИ
void MainWindow::processRecords(std::vector<PersonRecord> records)
{
    QString previousHash;
    bool chainIntact = true;
    int firstInvalid = -1;

    for (int i = 0; i < static_cast<int>(records.size()); ++i) {
        PersonRecord &rec = records[static_cast<size_t>(i)];
        const QString expected = computeHash(rec, previousHash);

        if (chainIntact && rec.hash == expected) {
            rec.valid = true;
            previousHash = rec.hash;
        } else {
            if (chainIntact) {
                firstInvalid = i;
            }
            chainIntact = false;
            rec.valid = false;
        }
    }

    records_ = std::move(records);
    chainValid_ = chainIntact;
    firstInvalidIndex_ = firstInvalid;
    displayRecords();
}

//  ОТОБРАЖЕНИЕ В СПИСКЕ

void MainWindow::displayRecords()
{
    ui->listWidgetRecords->clear();

    for (const PersonRecord &rec : records_) {
        QString text;
        text += tr("Фамилия: %1\n").arg(rec.surname);
        text += tr("Имя: %1\n").arg(rec.name);
        text += tr("Паспорт: %1\n").arg(rec.passport);
        text += tr("Дата/время: %1\n").arg(rec.createdAt);
        text += tr("Хеш: %1").arg(rec.hash);

        auto *item = new QListWidgetItem(text, ui->listWidgetRecords);

        if (!rec.valid) {
            // неправильная и все последующие — красным
            item->setBackground(QColor(255, 204, 204));
        }
    }

    if (currentFilePath_.isEmpty()) {
        ui->labelStatus->setText(tr("Файл не загружен"));
    } else if (chainValid_) {
        ui->labelStatus->setText(
            tr("Файл: %1 — цепочка целая").arg(currentFilePath_));
    } else {
        ui->labelStatus->setText(
            tr("Файл: %1 — цепочка повреждена, первая ошибка в записи %2")
                .arg(currentFilePath_)
                .arg(firstInvalidIndex_ + 1)
            );
    }
}

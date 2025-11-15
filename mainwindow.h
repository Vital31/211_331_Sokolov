#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Одна запись персональных данных
struct PersonRecord {
    QString surname;
    QString name;
    QString passport;
    QString createdAt;
    QString hash;
    bool valid = false;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenClicked();
    void onAddRecordClicked();

private:
    void loadFromFile(const QString &filePath);
    bool parseJson(const QByteArray &data,
                   std::vector<PersonRecord> &records,
                   QString &errorMessage);
    QString computeHash(const PersonRecord &record,
                        const QString &previousHash) const;
    void processRecords(std::vector<PersonRecord> records);
    void displayRecords();

    Ui::MainWindow *ui;

    std::vector<PersonRecord> records_;
    bool chainValid_ = false;
    int firstInvalidIndex_ = -1;
    QString currentFilePath_;
};

#endif // MAINWINDOW_H

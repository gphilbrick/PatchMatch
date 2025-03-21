#ifndef LOADIMAGESDIALOG_H
#define LOADIMAGESDIALOG_H

#include <QDialog>

namespace view {

namespace Ui {
class LoadImagesDialog;
} // Ui

class LoadImagesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoadImagesDialog(QWidget *parent = 0, const QString& lastSourcePath=".", const QString& lastTargetPath=".");
    ~LoadImagesDialog();
    QString sourceImagePath() const;
    QString targetImagePath() const;
private slots:
    void browseSourceImageSlot();
    void browseTargetImageSlot();
    void okSlot();
    void cancelSlot();
private:
    QString _lastTargetFilePath;
    QString _lastSourceFilePath;
    QString getImageFilePath(const QString& initialPath);
    Ui::LoadImagesDialog *ui;
    std::vector<QString> _sourceImageFilePaths;
};

} // view

#endif // #include

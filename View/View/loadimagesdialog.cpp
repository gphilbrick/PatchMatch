#include <QFileDialog>

#include <loadimagesdialog.h>

#include <ui_loadimagesdialog.h>

namespace view {

LoadImagesDialog::LoadImagesDialog(QWidget *parent, const QString& lastSourcePath, const QString& lastTargetPath) :
    QDialog(parent),
    ui(new Ui::LoadImagesDialog)
{
    ui->setupUi(this);

    _lastSourceFilePath=lastSourcePath;
    _lastTargetFilePath = lastTargetPath;

    connect(ui->targetImageBrowseButton,SIGNAL(clicked()),this,SLOT(browseTargetImageSlot()));
    connect(ui->sourceImageBrowseButton,SIGNAL(clicked()),this,SLOT(browseSourceImageSlot()));
    connect(ui->cancelPushButton,SIGNAL(clicked()),this,SLOT(cancelSlot()));
    connect(ui->okPushButton,SIGNAL(clicked()),this,SLOT(okSlot()));

    ui->targetImageLineEdit->setText(lastTargetPath);
    ui->sourceImageLineEdit->setText(lastSourcePath);

}

LoadImagesDialog::~LoadImagesDialog()
{
    delete ui;
}

void LoadImagesDialog::okSlot()
{
    accept();
}

void LoadImagesDialog::cancelSlot()
{
    reject();
}

QString LoadImagesDialog::targetImagePath() const
{
    return ui->targetImageLineEdit->text();
}

QString LoadImagesDialog::sourceImagePath() const
{
    return ui->sourceImageLineEdit->text();
}

void LoadImagesDialog::browseTargetImageSlot()
{
    QString imageFilePath = getImageFilePath(_lastTargetFilePath);
    if(imageFilePath.isEmpty()) return;
    ui->targetImageLineEdit->setText(imageFilePath);
    _lastTargetFilePath = imageFilePath;
}

void LoadImagesDialog::browseSourceImageSlot()
{
    QString imageFilePath = getImageFilePath(_lastSourceFilePath);
    if(imageFilePath.isEmpty()) return;
    ui->sourceImageLineEdit->setText(imageFilePath);
    _lastSourceFilePath = imageFilePath;
}

QString LoadImagesDialog::getImageFilePath(const QString& initialPath)
{
    QString filePath =  QFileDialog::getOpenFileName(this,"Choose Image File",initialPath,"Image File (*.jpg *.bmp *.png)");
    return filePath;
}

} // view


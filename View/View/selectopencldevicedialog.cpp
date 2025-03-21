#include <View/selectopencldevicedialog.h>

#include <ui_selectopencldevicedialog.h>

#include <View/opencldeviceselection.h>

namespace view {

SelectOpenCLDeviceDialog::SelectOpenCLDeviceDialog(
    const Devices& devices,
    const OpenCLDeviceSelection& selection,
    QWidget *parent )
    : QDialog(parent)
    , _devices( devices )
    , ui(new Ui::SelectOpenCLDeviceDialog)
{
    ui->setupUi(this);    

    // Build out all the options in the combo box
    for (const auto& d : devices) {
        ui->deviceComboBox->addItem(QString::fromStdString(d.name()));
    }

    // Select one of the options, possibly using 'selection'
    int index = 0;
    for (int i = 0; i < _devices.size(); i++) {
        if (selection.matches(_devices[i])) {
            index = i;
        }
    }
    ui->deviceComboBox->setCurrentIndex( index );
    connect(ui->deviceComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(indexChangedSlot()));
    indexChangedSlot();
}

SelectOpenCLDeviceDialog::~SelectOpenCLDeviceDialog()
{
    delete ui;
}

boost::optional< openCL::Device > SelectOpenCLDeviceDialog::device() const
{
    const auto index = ui->deviceComboBox->currentIndex();
    if (index > -1 ) {
        return _devices[index];
    } else {
        return boost::none;
    }
}

void SelectOpenCLDeviceDialog::indexChangedSlot()
{
    const char* const errorString = "N/A";
    const auto dev = device();
    ui->typeLabel->setText( dev 
        ? QString::fromStdString( openCL::Device::typeString( dev->type() ) )
        : errorString );
    ui->vendorLabel->setText(dev
        ? QString::fromStdString(dev->vendor())
        : errorString);
    ui->versionLabel->setText(dev
        ? QString::fromStdString(dev->version())
        : errorString);
}

} // view

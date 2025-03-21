#ifndef VIEW_SELECTOPENCLDEVICEDIALOG_H
#define VIEW_SELECTOPENCLDEVICEDIALOG_H

#include <OpenCL/device.h>

#include <QDialog>

#include <boost/optional.hpp>

#include <vector>

namespace openCL {
class Device;
} // openCL

namespace view {

class OpenCLDeviceSelection;

namespace Ui {
class SelectOpenCLDeviceDialog;
}

class SelectOpenCLDeviceDialog : public QDialog
{
    Q_OBJECT
public:
    using Devices = std::vector< openCL::Device >;
    /// sizeof( 'devices' ) > 0
    explicit SelectOpenCLDeviceDialog(
        const Devices& devices,
        const OpenCLDeviceSelection&,
        QWidget *parent = nullptr );
    ~SelectOpenCLDeviceDialog();
    boost::optional< openCL::Device > device() const;
private slots:
    void indexChangedSlot();
private:
    const Devices _devices;
    Ui::SelectOpenCLDeviceDialog *ui;
};

} // view

#endif // #include

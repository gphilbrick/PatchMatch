#include <opencldeviceselection.h>

#include <OpenCL/device.h>

#include <QSettings>

namespace view {

const char* const key_deviceName = "openCLDeviceName";
const char* const key_deviceVendor = "openCLDeviceVendor";
const char* const key_deviceVersion = "openCLDeviceVersion";

OpenCLDeviceSelection::OpenCLDeviceSelection( const QSettings& loadFrom )
{
	if (loadFrom.contains(key_deviceName)
	&& loadFrom.contains(key_deviceVendor) 
	&& loadFrom.contains(key_deviceVersion ) ) {
		_deviceName = loadFrom.value(key_deviceName).toString().toStdString();
		_deviceVendor = loadFrom.value(key_deviceVendor).toString().toStdString();
		_deviceVersion = loadFrom.value(key_deviceVersion).toString().toStdString();
	}
}

OpenCLDeviceSelection::OpenCLDeviceSelection( const openCL::Device& device )
{
	_deviceName = device.name();
	_deviceVendor = device.vendor();
	_deviceVersion = device.version();
}

bool OpenCLDeviceSelection::matches(const openCL::Device& d) const
{
	if (_deviceName && _deviceVendor && _deviceVersion) {
		return d.name() == _deviceName.value()
			&& d.vendor() == _deviceVendor.value()
			&& d.version() == _deviceVersion.value();
	} else {
		return false;
	}
}

boost::optional< openCL::Device > OpenCLDeviceSelection::selectDevice( const openCL::Platforms& platforms )
{
	for (const auto& p : platforms) {
		for (const auto& d : p.devices() ) {
			if ( matches(d ) ) {
				return d;
			}
		}
	}
	return boost::none;
}

void OpenCLDeviceSelection::save( QSettings& saveTo ) const
{
	if (_deviceName && _deviceVendor && _deviceVersion) {
		saveTo.setValue(key_deviceName, QString::fromStdString(_deviceName.value()));
		saveTo.setValue(key_deviceVendor, QString::fromStdString(_deviceVendor.value()));
		saveTo.setValue(key_deviceVersion, QString::fromStdString(_deviceVersion.value()));
	}
}

} // view
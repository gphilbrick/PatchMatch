#ifndef VIEW_OPENCLVDEVICESELECTION_H
#define VIEW_OPENCLVDEVICESELECTION_H

#include <boost/optional.hpp>

#include <OpenCL/platform.h>

#include <string>

class QSettings;

namespace view {

/// Loadable/savable information about which OpenCL device has been selected for 
/// use, possibly indicating no selection at all. Purpose is to persist device
/// selection between multiple runs of an application, possibly with OpenCL 
/// installation changes between runs.
class OpenCLDeviceSelection
{
public:
	/// Construct a selection indicating no OpenCL device selected.
	OpenCLDeviceSelection() = default;
	/// If 'loadFrom' does not contain information about an OpenCL
	/// device selection, construct an instance indicating no selection.
	/// The resulting selection may not correspond to a valid OpenCL device.
	OpenCLDeviceSelection( const QSettings& loadFrom );
	OpenCLDeviceSelection( const openCL::Device& );
	boost::optional< openCL::Device > selectDevice( const openCL::Platforms& );
	void save( QSettings& saveTo ) const;
	bool matches( const openCL::Device& ) const;
private:
	boost::optional< std::string > _deviceName;
	boost::optional< std::string > _deviceVendor;
	boost::optional< std::string > _deviceVersion;
};

} // view

#endif // #include
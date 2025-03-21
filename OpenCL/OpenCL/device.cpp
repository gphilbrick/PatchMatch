#include <OpenCL/device.h>

#include <map>
#include <sstream>

namespace openCL {

Device::Device( cl::Device device ) : _device( device ), _supportsImages( false )
{
	_device.getInfo( CL_DEVICE_NAME, &_name );
	_device.getInfo( CL_DEVICE_VENDOR, &_vendor );
	_device.getInfo( CL_DEVICE_PROFILE, &_profile );
	_device.getInfo( CL_DEVICE_VERSION, &_version );
	_device.getInfo( CL_DEVICE_OPENCL_C_VERSION, &_openCL_CVersion );
	_device.getInfo( CL_DEVICE_TYPE, &_type );

	cl_bool images = CL_FALSE;
    _device.getInfo( CL_DEVICE_IMAGE_SUPPORT, &images );	
	_supportsImages = images == CL_TRUE;
}

Device& Device::operator=(const Device& d)
{
	_device = d._device;
	_name = d._name;
	_vendor = d._vendor;
	_profile = d._profile;
	_version = d._version;
	_openCL_CVersion = d._openCL_CVersion;
	_type = d._type;
	_supportsImages = d._supportsImages;
	return *this;
}

std::string Device::typeString( cl_device_type type )
{
    const std::map< cl_device_type, const char* > typeToString = {
        { CL_DEVICE_TYPE_CPU, "CPU" },
        { CL_DEVICE_TYPE_GPU, "GPU" },
        { CL_DEVICE_TYPE_ACCELERATOR, "ACCELERATOR" },
        { CL_DEVICE_TYPE_CUSTOM, "CUSTOM" }
    };	
	
	size_t numMatches = 0;
	std::stringstream stream;
	for( const auto& pair : typeToString ) {
		if( type & pair.first ) {
			if( numMatches ) {
				stream << "_";
			}
			stream << pair.second;
			numMatches++;
		}
	}
	if( numMatches == 0 ) {
		stream << "UNKNOWN";
	}
	return stream.str();
}	

const cl::Device& Device::device() const
{
	return _device;
}

cl_device_type Device::type() const 
{
	return _type;
}

const std::string& Device::name() const
{
	return _name;
}

const std::string& Device::vendor() const
{
	return _vendor;
}

const std::string& Device::profile() const
{
	return _profile;
}

const std::string& Device::version() const
{
	return _version;
}

const std::string& Device::openCL_CVersion() const
{
	return _openCL_CVersion;
}

bool Device::supportsImages() const
{
	return _supportsImages;
}	
	
} // openCL
#include <OpenCL/platform.h>

namespace openCL {
	
Platform::Platform( cl::Platform platform ) : _platform( platform )
{
    _platform.getInfo( CL_PLATFORM_PROFILE, &_profile );
    _platform.getInfo( CL_PLATFORM_VERSION, &_version );
    _platform.getInfo( CL_PLATFORM_NAME, &_name );
    _platform.getInfo( CL_PLATFORM_VENDOR, &_vendor );
    _platform.getInfo( CL_PLATFORM_EXTENSIONS, &_extensions );	
	
	// devices
	cl_int error = 0;
    std::vector< cl::Device > devices;	
    error = _platform.getDevices( CL_DEVICE_TYPE_ALL, &devices );
    if( error == CL_SUCCESS )
    {
		for( const auto& d : devices ) {
			_devices.push_back( Device{ d } );
		}
    }	
}

Platform::Platforms Platform::platforms()
{
    cl_int error = 0;
    std::vector< cl::Platform > handles;
    error = cl::Platform::get( &handles );
	Platforms ret;
    if( error == CL_SUCCESS )
    {
        for( const auto& handle : handles ) {
			ret.push_back( Platform{ handle } );
		}
	}	
	return ret;
}

const Platform::Devices& Platform::devices() const
{
	return _devices;
}

const std::string& Platform::profile() const
{
	return _profile;
}

const std::string& Platform::version() const
{
	return _version;
}

const std::string& Platform::name() const
{
	return _name;
}

const std::string& Platform::vendor() const
{
	return _vendor;
}

const std::string& Platform::extensions() const
{
	return _extensions;
}
	
} // openCL
#ifndef OPENCL_PLATFORM_H
#define OPENCL_PLATFORM_H

#include <OpenCL/device.h>

#include <CL/cl.hpp>

#include <string>
#include <vector>

namespace openCL {
	
class Platform 
{
public:
	using Platforms = std::vector< Platform >;
	using Devices = std::vector< Device >;
	/// 'p' must be a real OpenCL platform on this system.
	Platform( cl::Platform p );	
	const Devices& devices() const;
	const std::string& profile() const;
	const std::string& version() const;
	const std::string& name() const;
	const std::string& vendor() const;
	const std::string& extensions() const;
	
	static Platforms platforms();
private:
	const cl::Platform _platform;
    std::string _profile;
	std::string _version;
	std::string _name;
	std::string _vendor;
	std::string _extensions;	
	Devices _devices;
};

using Platforms = Platform::Platforms;

} // openCL

#endif // #include
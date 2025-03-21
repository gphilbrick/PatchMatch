#ifndef OPENCL_DEVICE_H
#define OPENCL_DEVICE_H

#include <CL/cl.hpp>

#include <string>

namespace openCL {

/// An OpenCL device handle along with metadata.
class Device 
{
public:
	/// 'device' must be a real OpenCL device associated with
	/// one of the OpenCL platforms on this system.
	Device( cl::Device device );
	Device& operator=(const Device&);
	const cl::Device& device() const;
	const std::string& name() const;
	const std::string& vendor() const;
	const std::string& profile() const;
	const std::string& version() const;
	const std::string& openCL_CVersion() const;
	bool supportsImages() const;
	cl_device_type type() const;
	
	static std::string typeString( cl_device_type );
private:
	cl::Device _device;
    std::string _name; 
	std::string _vendor;
	std::string _profile;
	std::string _version;
	std::string _openCL_CVersion;	
	cl_device_type _type;
	bool _supportsImages;
};
	
} // openCL

#endif // #include
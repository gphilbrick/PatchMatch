#ifndef IEC_OPENCLGPUHOST_H
#define IEC_OPENCLGPUHOST_H

#include <Core/image/imagetypes.h>

#include <OpenCL/opencltypes.h>

#include <memory>
#include <vector>

namespace core {
template<typename T>
class TwoDArray;

class IntCoord;	
class Vector3;
} // core

namespace openCL {

class Device;

/// A single-device OpenCL host program.
class OpenCLGPUHost
{
public:
    ///  'device' must refer to a valid OpenCL device on this machine.
    explicit OpenCLGPUHost( const Device& device );
    OpenCLGPUHost( const OpenCLGPUHost& ) = delete;
    OpenCLGPUHost& operator = (const OpenCLGPUHost&) = delete;
    virtual ~OpenCLGPUHost();
protected:
    //fileName is a path relative to directory containing all our kernel files.
    //for instance, it might be "patchMatchPrograms/holeFill.cl"
    //If success=true, buildLog is "".
    //Note that we are _not_ returning a cl::Program object - this would mean that the
    //cl::Program object created inside  the method had already been destroyed by the time
    //the caller got a copy of it.  Remember that cl::X is a _wrapper_ around an X handle.  It is
    //not itself an X handle.
    void buildProgramFromFile(const std::string& fileName, cl::Program& program, bool& success, std::string& buildLog);

    //utilities for getting our image types into
    //and out of OpenCL domain.  Note use of cl_float4 instead of cl_float3.  Internally,
    //we only use CL_RGBA, never CL_RGB.  This is because CL_RGB cannot be used with CL_FLOAT
    // (see http://www.khronos.org/registry/cl/sdk/1.0/docs/man/xhtml/cl_image_format.html).
    static std::unique_ptr< cl_float4[] > arrayFromRGBImage(const core::ImageRGB& rgbImage);
    static void rgbImageFromArray(core::ImageRGB& rgbImage, const core::IntCoord& dims, cl_float4* array);
    static std::unique_ptr< cl_int[] > arrayFromBoolImage(const core::ImageBinary& boolImage);    
    static CLSizeCoords3 getImageReadWriteCoord(int x, int y, int z);

    std::vector< cl::Device > _devices;
    cl::Context _context;
    cl::CommandQueue _commandQueue;
};

} // openCL

#endif // #include guard

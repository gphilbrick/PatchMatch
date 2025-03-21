#include <OpenCL/openclgpuhost.h>

#include <OpenCL/device.h>

#include <Core/exceptions/runtimeerror.h>
#include <Core/utility/twodarray.h>
#include <Core/utility/vector3.h>

#include <sstream>
#include <fstream>

namespace openCL {

OpenCLGPUHost::OpenCLGPUHost( const Device& d )
    : _devices{ d.device() }
{
    cl_int error = 0;
    _context = cl::Context( _devices, 0, 0, 0, &error );
    if (error != CL_SUCCESS)
    {
        THROW_RUNTIME( "Failed to create OpenCL context" );
    }
    _commandQueue = cl::CommandQueue( _context, _devices.front(), 0, &error);
    if (error != CL_SUCCESS)
    {
        THROW_RUNTIME( "Failed to make OpenCL command queue" );
    }
}

OpenCLGPUHost::~OpenCLGPUHost()
{    
}

CLSizeCoords3 OpenCLGPUHost::getImageReadWriteCoord(int x, int y, int z)
{
    CLSizeCoords3 coord;
    coord[ 0 ] = x;
    coord[ 1 ] = y;
    coord[ 2 ] = z;
    return coord;
}

void OpenCLGPUHost::buildProgramFromFile( const std::string& fileName, cl::Program& program, bool& success, std::string& buildLog )
{
    success=false;
    buildLog="";

    std::ostringstream stringStream;
    stringStream << "runtimeResources/openCLPrograms/"; 
    stringStream << fileName;
    std::string fullPath = stringStream.str();

    std::ifstream file(fullPath);
    std::string sourceCode(std::istreambuf_iterator<char>(file),(std::istreambuf_iterator<char>()));
    if(sourceCode.empty())
    {
        THROW_RUNTIME( "OpenCL source file was empty" );
    }
    const cl::Program::Sources source{ sourceCode };
    file.close();

    cl_int error=0;
    program = cl::Program(_context,source,&error);
    if(error!=CL_SUCCESS)
    {
        buildLog = "Failed to construct the program object";
        return;
    }
    error = program.build(_devices); //returns CL_BUILD_PROGRAM_FAILURE
    if(error!=CL_SUCCESS)
    {
        buildLog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(_devices[0],&error);
    }
    else
    {
        success=true;
    }

}

std::unique_ptr< cl_int[] > OpenCLGPUHost::arrayFromBoolImage( const core::ImageBinary& boolImage ) 
{
    int width=boolImage.width(), height=boolImage.height();
    auto array = std::make_unique< cl_int[] >( width * height );
    for(int y=0; y<height; y++)
    {
        for(int x=0; x<width; x++)
        {
            int idx = y*width+x;
            array[idx] = boolImage.get(x,y);
        }
    }
    return array;
}

std::unique_ptr< cl_float4[] > OpenCLGPUHost::arrayFromRGBImage( const core::ImageRGB& rgbImage )
{
    //row major
    const auto width = rgbImage.width();
    const auto height = rgbImage.height();
    auto array = std::make_unique< cl_float4[] >( width * height );
    for(int y=0; y<height; y++)
    {
        for(int x=0; x<width; x++)
        {
            core::Vector3 rgb = rgbImage.get(x,y);
            const auto idx=y*width+x;
            array[ idx ].s[0] = (float)rgb.r();
            array[ idx ].s[1] = (float)rgb.g();
            array[ idx ].s[2] = (float)rgb.b();
            array[ idx ].s[3] = 0.0;
        }
    }
    return array;
}

void OpenCLGPUHost::rgbImageFromArray( core::ImageRGB& rgbImage, const core::IntCoord& dims, cl_float4* array)
{
    if(rgbImage.size()!=dims)
    {
        rgbImage.recreate(dims.x(),dims.y());
    }

    int width=dims.x(), height=dims.y();

    //row major
    for(int y=0; y<height; y++)
    {
        for(int x=0; x<width; x++)
        {
            int idx = y*width+x;
            float r = array[ idx ].s[0];
            float g = array[ idx ].s[1];
            float b = array[ idx ].s[2];
            rgbImage.set(x,y,core::Vector3((double)r,(double)g,(double)b));
        }
    }
}

} // openCL

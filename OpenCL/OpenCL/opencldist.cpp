#include <OpenCL/opencldist.h>

#include <OpenCL/opencltypes.h>

#include <Core/exceptions/runtimeerror.h>
#include <Core/utility/twodarray.h>
#include <Core/utility/mathutility.h>

#include <array>
#include <algorithm>

namespace openCL {

OpenCLDist::OpenCLDist( const Device& device ) : OpenCLGPUHost( device )
{	
    // Create the OpenCL program we need
    bool success = false;
    std::string buildLog;
    buildProgramFromFile( "utility/utility.cl", _program, success, buildLog );
    if(!success)
    {
        _commandQueue.finish();
        THROW_RUNTIME("Failed to build program");
    }

    cl_int error=CL_SUCCESS;

    _initKernel = cl::Kernel(_program,"externalDistanceMapInit",&error);
    const char* const msg = "Failed to get kernel.";
    if (error != CL_SUCCESS) {
        THROW_RUNTIME( msg );
    }
    _stepKernel = cl::Kernel(_program,"externalDistanceMapStep",&error);
    if (error != CL_SUCCESS) {
        THROW_RUNTIME(msg);
    }
}

void OpenCLDist::distMap(const core::ImageBinary& distTo, core::ImageScalar& store, double inShapeDist)
{
    cl_int error=CL_SUCCESS;
    int width = distTo.width(), height = distTo.height();
    bool readIndex =false;

    const CLSizeCoords3 origin{ 0, 0, 0 };
    const CLSizeCoords3 region{ width, height, 1 };

    //Create the shape map in OpenCL memory
    const cl::Buffer shapeMap(
        _context,
        CL_MEM_READ_WRITE,
        sizeof(cl_int) * width * height,
        0,
        &error);
    auto shapeInputArray = OpenCLGPUHost::arrayFromBoolImage(distTo);
    error = _commandQueue.enqueueWriteBuffer(
        shapeMap,
        CL_FALSE,
        0,
        ( sizeof(cl_int) ) * width * height,
        shapeInputArray.get(),
        0,
        0);

    // Create the dist maps: two float arrays (for double buffering) and one int array to represent
    // our binary "dist to" image.
    std::array< cl::Buffer, 2 > distMaps;
    std::fill(
        distMaps.begin(),
        distMaps.end(),
        cl::Buffer(_context, CL_MEM_READ_WRITE, sizeof(cl_float) * width * height, 0, &error));

    //initial fill the distance map
    error = _initKernel.setArg(0,shapeMap);
    error = _initKernel.setArg(1,distMaps[readIndex]);
    error = _initKernel.setArg(2,(cl_float)inShapeDist);
    error = _commandQueue.enqueueNDRangeKernel(_initKernel,
                                               cl::NullRange,
                                               cl::NDRange(width,height),
                                               cl::NullRange);

    //now compute the distance map with jumpflood
    int k = core::mathUtility::jumpfloodInitialK(width,height);

    while(k>0)
    {
        error = _stepKernel.setArg(0,shapeMap);
        error = _stepKernel.setArg(1,distMaps[readIndex]);
        error = _stepKernel.setArg(2,distMaps[!readIndex]);
        error = _stepKernel.setArg(3,(cl_int)k);
        error = _commandQueue.enqueueNDRangeKernel(_stepKernel,
                                                   cl::NullRange,
                                                   cl::NDRange(width,height),
                                                   cl::NullRange);
        //swap buffers
        readIndex=!readIndex;
        k/=2;
    }

    auto distOutputArray = std::make_unique< cl_float[] >(width * height);
    error = _commandQueue.enqueueReadBuffer(
        distMaps[readIndex],
        CL_FALSE,
        0,
        width * height * sizeof(cl_float),
        distOutputArray.get() );
    error = _commandQueue.finish();

    if(store.size()!=distTo.size())
    {
        store.recreate(width,height);
    }
    for(int x=0; x<width; x++)
    {
        for(int y=0; y<height; y++)
        {
            store.set(x,y,(double)distOutputArray[x+y*width]);
        }
    }
}

} // openCL

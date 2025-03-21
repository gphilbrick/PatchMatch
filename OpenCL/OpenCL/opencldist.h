#ifndef OPENCL_OPENCLDIST_H
#define OPENCL_OPENCLDIST_H

#include <OpenCL/openclgpuhost.h>

#include <Core/image/imagetypes.h>

namespace core {
template<typename T>
class TwoDArray;
} // core

namespace openCL {

/// Computes distance maps (unidirectional). Allows "staggered" distances 
/// where the distance to shape S starts out at some nonzero k (at/within
/// S's borders) and goes up from there.
class OpenCLDist : public OpenCLGPUHost
{
public:
    OpenCLDist( const Device& device );
    void distMap( 
		const core::ImageBinary& distTo, 
		core::ImageScalar& store, 
		double inShapeDist = 0 );
private:
    cl::Program _program;
    cl::Kernel _initKernel;
    cl::Kernel _stepKernel;
};

} // openCL

#endif // #include 

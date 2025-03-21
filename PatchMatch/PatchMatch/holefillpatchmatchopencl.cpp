#include <holefillpatchmatchopencl.h>
#include <patchmatchutility.h>

#include <OpenCL/opencltypes.h>

#include <Core/exceptions/runtimeerror.h>
#include <Core/utility/mathutility.h>

#include <iostream>

namespace patchMatch {

HoleFillPatchMatchOpenCL::HoleFillPatchMatchOpenCL( const openCL::Device& device ) 
    : OpenCLGPUHost( device )    
{
    const auto getKernel = [&](
        const cl::Program& program,
        cl::Kernel& store,
        const char* const kernelName)
        {
            cl_int error = CL_INVALID_VALUE;
            store = cl::Kernel(program, kernelName, &error);
            if (error != CL_SUCCESS) {
                const auto msg = "Failed to get kernel " + std::string{ kernelName };
                THROW_RUNTIME(msg.c_str());
            }
        };

    /// 'relPath' is relative to the directory containing OpenCL programs
    const auto buildProgram = [&](cl::Program& store, const char* const relPath)
    {
        bool success = false;
        std::string buildLog;
        buildProgramFromFile( relPath, store, success, buildLog );
        if ( !success )
        {
            std::cerr << "Failed to build OpenCL program at " << relPath << std::endl;
            std::cerr << "\tBuild log: " << buildLog;
            _commandQueue.finish();
            THROW_RUNTIME("Failed to build utility program");
        }
    };

    buildProgram(_utilityProgram, "utility/utility.cl");
    getKernel(_utilityProgram, _downsampleRGBImageKernel, "downsampleRGBImage");
    getKernel(_utilityProgram, _downsampleBooleanImageKernel, "downsampleBooleanImage");
    getKernel(_utilityProgram, _internalDistanceMapInitKernel, "internalDistanceMapInit");
    getKernel(_utilityProgram, _distanceMapStepKernel, "internalDistanceMapStep");

    buildProgram(_holeFillProgram, "patches/holeFillPatchMatch.cl");
    getKernel(_holeFillProgram, _blendKernel, "blend");
    getKernel(_holeFillProgram, _sourceMaskFromTargetMaskKernel, "sourceMaskFromTargetMask");
    getKernel(_holeFillProgram, _anchorWeightsFromInternalDistMapKernel, "anchorWeightsFromInternalDistMap");
    getKernel(_holeFillProgram, _nnfInitialFillKernel, "nnfInitialFill");
    getKernel(_holeFillProgram, _nnfUpsampleCoordsKernel, "nnfUpsampleCoords");
    getKernel(_holeFillProgram, _nnfCostsKernel, "nnfCosts");
    getKernel(_holeFillProgram, _searchKernel, "search");
    getKernel(_holeFillProgram, _blackOutMaskedAreaKernel, "blackOutMaskedArea");
    getKernel(_holeFillProgram, _initialHoleFillKernel, "initialHoleFill");
    getKernel(_holeFillProgram, _propagateKernel, "propagate");
}

void HoleFillPatchMatchOpenCL::init(
                         const core::ImageRGB& target,
                         const core::ImageBinary& targetMask,
                         int numPyramidLevels,
                         int patchWidth)
{
    cleanupMemObjects();

    cl_int error = CL_SUCCESS;

    //validation
    if (patchWidth < 3) {
        THROW_RUNTIME( "Too small patch size" );
    }
    if (target.size() != targetMask.size()) {
        THROW_RUNTIME("Mask must be same size as target.");
    }
    if (target.width() < patchWidth || target.height() < patchWidth) {
        THROW_RUNTIME("Patch size too big.");
    }
    if(numPyramidLevels<1) {
        THROW_RUNTIME("Illegal numPyramidLevels");
    }
    _numPyramidLevels=numPyramidLevels;
    _patchWidth= patchWidth;
    _targetOriginalDims = target.size();
    _sourceOriginalDims = target.size();

    //OpenCL stuff to make and put online:
    // target original size
    // source original size
    // targetMask original size

    //Get original size target image, source image, and target mask into OpenCL.
    _targetOriginalSize = std::make_unique< cl::Image2D >(
        _context,
        CL_MEM_READ_ONLY,
        cl::ImageFormat(CL_RGBA, CL_FLOAT),
        target.width(), 
        target.height(), 
        0, 
        nullptr,
        &error );
    _sourceOriginalSize = std::make_unique< cl::Image2D >(
        _context,
        CL_MEM_READ_ONLY,
        cl::ImageFormat(CL_RGBA, CL_FLOAT),
        target.width(), 
        target.height(), 
        0, 
        nullptr,
        &error );
    _targetMaskOriginalSize = std::make_unique< cl::Buffer >(
        _context,
        CL_MEM_READ_ONLY,
        sizeof(cl_int) * target.width() * target.height(),
        nullptr,
        &error );
    
    const auto origin = getImageReadWriteCoord(0,0,0);
    const auto region = getImageReadWriteCoord(target.width(),target.height(),1);
    auto targetInputArray = OpenCLGPUHost::arrayFromRGBImage(target);

    auto targetMaskInputArray = OpenCLGPUHost::arrayFromBoolImage(targetMask);
    error = _commandQueue.enqueueWriteImage(
        *_targetOriginalSize,
        CL_FALSE,
        origin,
        region,
        0,
        0,
        targetInputArray.get(),
        0,
        0);
    error = _commandQueue.enqueueWriteImage(
        *_sourceOriginalSize,
        CL_FALSE,
        origin,
        region,
        0,
        0,
        targetInputArray.get(),
        0,
        0);
    error = _commandQueue.enqueueWriteBuffer(
        *_targetMaskOriginalSize,
        CL_FALSE,
        0,
        (sizeof(cl_int)) * target.width() * target.height(),
        targetMaskInputArray.get(),
        0,
        0);

    // Get our random seeds onto GPU. 
    // Seed with constant to ensure determinism.
    srand(42);
    cl_ulong* randomSeedsArray = new cl_ulong[target.width()*target.height()];
    for(int i=0; i<target.width()*target.height(); i++) {
        randomSeedsArray[i] = rand();
    }
    _randomBuffer = std::make_unique< cl::Buffer >(
        _context,
        CL_MEM_READ_WRITE,
        sizeof(cl_ulong)*target.width()*target.height(),
        nullptr,
        &error );
    error = _commandQueue.enqueueWriteBuffer(*_randomBuffer,CL_FALSE,0,
                                             sizeof(cl_ulong)*target.width()*target.height(),randomSeedsArray,0,0);

    error =_commandQueue.finish();
    // At this point, our target and targetMask original size are inside OpenCL, 
    // though nothing else has been created yet.

    // Make our first planned step to be filling out the first pyramid level
    _currentPyramidLevel=-1;
    planStep(NextPyramid);

    // Assign default values to OpenCL stuff that will not 
    // be initialized until our first executeQueue() call.
    _nnfReadIndex=true;
    _anchorWeightsReadIndex=true;
}

void HoleFillPatchMatchOpenCL::cleanupMemObjects()
{
    _targetOriginalSize = nullptr;
    _sourceOriginalSize = nullptr;
    _targetMaskOriginalSize = nullptr;
    _targetPyramidSize = nullptr;
    _targetMaskPyramidSize = nullptr;
    _sourcePyramidSize = nullptr;
    _sourceMaskPyramidSize = nullptr;
    _randomBuffer = nullptr;
    for(int i=0; i<2; i++)
    {
        _nnfCoords[i] = nullptr;
        _nnfCosts[i] = nullptr;
        _anchorWeights[ i ] = nullptr;
    }
}

void HoleFillPatchMatchOpenCL::planStep(Step step)
{
    //do not allow enqueueing bogus procedures, like doing two blend steps in a row
    if (step == Blend && _steps.back() == Blend) {
        THROW_RUNTIME("Makes no sense to do two blends back to back.");
    }
    _steps.push(step);
}

void HoleFillPatchMatchOpenCL::executeSteps(core::ImageRGB& blendResult)
{
    if (!stepsValidForExecution()) {
        THROW_RUNTIME("Queue is invalid");
    }

    cl_int error = CL_SUCCESS;

    //go through queue
    while(_steps.size())
    {
        Step nextStep = _steps.front();
        _steps.pop();
        switch(nextStep)
        {
        case NextPyramid:
        {
            enqueueSetupNextPyramidLevel();
            break;
        }
        case Blend:
        {
            enqueueBlend();
            break;
        }
        case Search:
        {
            enqueueSearch();
            break;
        }
        case Propagate:
        {
            enqueuePropagate();
            break;
        }
        };
    }

    //read back the target pyramid size image - the one that was just
    //written to in the blend step at the end of the queue, the one that
    //is now the read image
    auto outputArray = std::make_unique< cl_float4[] >( _targetPyramidDims.x() * _targetPyramidDims.y() );
    error = _commandQueue.enqueueReadImage(
        *_targetPyramidSize,
        CL_FALSE,
        getImageReadWriteCoord(0,0,0),
        getImageReadWriteCoord(_targetPyramidDims.x(),_targetPyramidDims.y(),1),
        0,
        0,
        outputArray.get(),
        0,
        0 );
    error = _commandQueue.finish();

    OpenCLGPUHost::rgbImageFromArray(blendResult,_targetPyramidDims,outputArray.get());
}

bool HoleFillPatchMatchOpenCL::stepsValidForExecution()
{
    if(_steps.back()!=Blend) return false;
    return true;
}

void HoleFillPatchMatchOpenCL::enqueueSetupNextPyramidLevel()
{
    cl_int error=CL_SUCCESS;

    //Need to:
    // -recreate randomBufferIndices
    // -populate randomBufferIndices
    // -recreate targetPyramidSize and sourcePyramidSize
    // -fill targetPyramidSize and sourcePyramidSize by downsampling/copying from _originalSize images
    // -recreate targetMaskPyramidSize object
    // -fill targetMaskPyramidSize by downsampling/copying from targetMaskOriginalSize
    // -recreate sourceMaskPyramidSize object
    // -fill sourceMaskPyramidSize from targetMaskPyramidSize using hole fill-specific code
    // -recreate anchorWeights object
    // -fill anchorWeights object based on targetMaskPyramidSize (this is a two step process that might require an intermediate
    //  object since a cl::Buffer/cl::Image2D cannot be simultaneously a read and write object within a kernel.
    // -get new NNF
    //      if first pyramid level:
    //          -recreate all four NNF OpenCL objects (2 for each buffer)
    //          -random fill write buffer NNF coords
    //          -fill write buffer NNF costs based on write buffer NNF coords
    //      if not first pyramid level:
    //          -upsample write buffer NNF coords from read buffer NNF coords
    //          -fill write buffer NNF costs based on write buffer NNF coords
    //      -swap NNF buffers

    //absolutely must finish all pending OpenCL operations since
    //we will be deleting and recreating buffers/images
    error=_commandQueue.finish();

    //this is first pyramid level
    if(_currentPyramidLevel<0) {
        _currentPyramidLevel=_numPyramidLevels-1;
    } else {
        if(_currentPyramidLevel==0) {
            THROW_RUNTIME("Cannot advance to next pyramid level; we are already on the last one." );
        }
        _currentPyramidLevel--;
    }

    core::IntCoord prevTargetDims = _targetPyramidDims;
    core::IntCoord prevSourceDims = _sourcePyramidDims;
    utility::pyramidLevelSizes(_currentPyramidLevel,_numPyramidLevels,_patchWidth,_targetOriginalDims,
                                         _sourceOriginalDims,_targetPyramidDims,_sourcePyramidDims);

    _targetPyramidSize = std::make_unique< cl::Image2D >(
        _context,
        CL_MEM_READ_WRITE,
        cl::ImageFormat(CL_RGBA, CL_FLOAT),
        _targetPyramidDims.x(), 
        _targetPyramidDims.y(), 
        0, 
        nullptr,
        &error );
    _sourcePyramidSize = std::make_unique< cl::Image2D >(
        _context,
        CL_MEM_READ_WRITE,
        cl::ImageFormat(CL_RGBA, CL_FLOAT),
        _sourcePyramidDims.x(), 
        _sourcePyramidDims.y(), 
        0, 
        nullptr,
        &error );
    if(_currentPyramidLevel==0)
    {
        error = _commandQueue.enqueueCopyImage(*_targetOriginalSize,*_targetPyramidSize,
                                               getImageReadWriteCoord(0,0,0),
                                               getImageReadWriteCoord(0,0,0),
                                               getImageReadWriteCoord(_targetPyramidDims.x(),_targetPyramidDims.y(),1));
        error = _commandQueue.enqueueCopyImage(*_sourceOriginalSize,*_sourcePyramidSize,
                                               getImageReadWriteCoord(0,0,0),
                                               getImageReadWriteCoord(0,0,0),
                                               getImageReadWriteCoord(_sourcePyramidDims.x(),_sourcePyramidDims.y(),1));
    }
    else
    {
        error = _downsampleRGBImageKernel.setArg(0,*_targetOriginalSize);
        error = _downsampleRGBImageKernel.setArg(1,*_targetPyramidSize);
        error = _commandQueue.enqueueNDRangeKernel(_downsampleRGBImageKernel,
                                           cl::NullRange,
                                           cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                           cl::NullRange);
        error = _downsampleRGBImageKernel.setArg(0,*_sourceOriginalSize);
        error = _downsampleRGBImageKernel.setArg(1,*_sourcePyramidSize);
        error = _commandQueue.enqueueNDRangeKernel(_downsampleRGBImageKernel,
                                           cl::NullRange,
                                           cl::NDRange(_sourcePyramidDims.x(),_sourcePyramidDims.y()),
                                           cl::NullRange);
    }

    _targetMaskPyramidSize = std::make_unique< cl::Buffer >(
        _context,
        CL_MEM_READ_WRITE,
        sizeof(int)*_targetPyramidDims.x()*_targetPyramidDims.y(),
        nullptr,
        &error );
    _sourceMaskPyramidSize = std::make_unique< cl::Buffer >( 
        _context,
        CL_MEM_READ_WRITE,
        sizeof(int)*_sourcePyramidDims.x()*_sourcePyramidDims.y(),
        nullptr,
        &error );
    if(_currentPyramidLevel==0)
    {
        error = _commandQueue.enqueueCopyBuffer(*_targetMaskOriginalSize,*_targetMaskPyramidSize,
                                                0,0,sizeof(int)*_targetPyramidDims.x()*_targetPyramidDims.y());
    }
    else
    {
        error = _downsampleBooleanImageKernel.setArg(0,*_targetMaskOriginalSize);
        error = _downsampleBooleanImageKernel.setArg(1,*_targetMaskPyramidSize);
        error = _downsampleBooleanImageKernel.setArg(2,_targetOriginalDims.x());
        error = _downsampleBooleanImageKernel.setArg(3,_targetOriginalDims.y());
        error = _downsampleBooleanImageKernel.setArg(4,(int)1);
        error = _commandQueue.enqueueNDRangeKernel(_downsampleBooleanImageKernel,
                                                   cl::NullRange,
                                                   cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                                   cl::NullRange);
    }
    error = _sourceMaskFromTargetMaskKernel.setArg(0,*_targetMaskPyramidSize);
    error = _sourceMaskFromTargetMaskKernel.setArg(1,*_sourceMaskPyramidSize);
    error = _sourceMaskFromTargetMaskKernel.setArg(2,_patchWidth);
    error = _commandQueue.enqueueNDRangeKernel(_sourceMaskFromTargetMaskKernel,
                                               cl::NullRange,
                                               cl::NDRange(_sourcePyramidDims.x(),_sourcePyramidDims.y()),
                                               cl::NullRange);

    //anchorWeights
    enqueueSetupAnchorWeights();


    //nnf and target initial fill
    if(_currentPyramidLevel==_numPyramidLevels-1)
    {
        //Is this the first pyramid level?  If so, we need to do an initial fill of the
        //hole in targetPyramidSizeImage.  Right now, the hole is filled with the original contents,
        //which is illegal, since those are meant not to exist.
        enqueueInitialHoleFill();

        enqueueSetupFirstNNF();
    }
    else
    {
        enqueueSetupNextNNF(prevTargetDims,prevSourceDims);
    }


}

void HoleFillPatchMatchOpenCL::enqueueInitialHoleFill()
{
    cl_int error = CL_SUCCESS;

    //black out the target image where masked
    error = _blackOutMaskedAreaKernel.setArg(0,*_targetMaskPyramidSize);
    error = _blackOutMaskedAreaKernel.setArg(1,*_targetPyramidSize);
    error = _commandQueue.enqueueNDRangeKernel(_blackOutMaskedAreaKernel,
                                       cl::NullRange,
                                       cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                       cl::NullRange);

    //finish the queue because we are going to make a new entity
    error = _commandQueue.finish();

    //make a new image buffer which will automatically be deleted when this function exits
    cl::Image2D targetBuffer = cl::Image2D(_context,
                                      CL_MEM_READ_WRITE,
                                      cl::ImageFormat(CL_RGBA, CL_FLOAT),
                                      _targetPyramidDims.x(), _targetPyramidDims.y(), 0, 0,&error);
    error = _commandQueue.enqueueCopyImage(*_targetPyramidSize,targetBuffer,
                                           getImageReadWriteCoord(0,0,0),
                                           getImageReadWriteCoord(0,0,0),
                                           getImageReadWriteCoord(_targetPyramidDims.x(),_targetPyramidDims.y(),1));
    cl::Image2D* readBuffer = _targetPyramidSize.get();
    cl::Image2D* writeBuffer = &targetBuffer;

    //MODFLAG
    const int numBlurs=100;

    error = _initialHoleFillKernel.setArg(0,*_targetMaskPyramidSize);
    for(int i=0; i<numBlurs; i++)
    {
        error = _initialHoleFillKernel.setArg(1,*readBuffer);
        error = _initialHoleFillKernel.setArg(2,*writeBuffer);
        error = _commandQueue.enqueueNDRangeKernel(_initialHoleFillKernel,
                                           cl::NullRange,
                                           cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                           cl::NullRange);
        std::swap(readBuffer, writeBuffer);
    }

    //if our last buffer is our temp buffer, we need to copy targetBuffer back to targetPyramidSize
    if(readBuffer == &targetBuffer)
    {
        error = _commandQueue.enqueueCopyImage(targetBuffer,*_targetPyramidSize,
                                               getImageReadWriteCoord(0,0,0),
                                               getImageReadWriteCoord(0,0,0),
                                               getImageReadWriteCoord(_targetPyramidDims.x(),_targetPyramidDims.y(),1));

    }

    //finish the queue because we are going to (implicitly) destroy a new entity
    error = _commandQueue.finish();
}

void HoleFillPatchMatchOpenCL::enqueueSetupFirstNNF()
{
    cl_int error = CL_SUCCESS;

    //recreate all four objects
    for(int i=0; i<2; i++)
    {
        _nnfCosts[i] = std::make_unique< cl::Buffer >( 
            _context,
            CL_MEM_READ_WRITE,
            sizeof(float)*_targetPyramidDims.x()*_targetPyramidDims.y(),
            nullptr,
            &error );
        _nnfCoords[ i ] = std::make_unique< cl::Buffer >(
            _context,
            CL_MEM_READ_WRITE,
            2*sizeof(int)*_targetPyramidDims.x()*_targetPyramidDims.y(),
            nullptr,
            &error );
    }

    //prepare the initial fill kernel
    error = _nnfInitialFillKernel.setArg(0,*_targetMaskPyramidSize);
    error = _nnfInitialFillKernel.setArg(1,*_sourceMaskPyramidSize);
    error = _nnfInitialFillKernel.setArg(2,_sourcePyramidDims.x());
    error = _nnfInitialFillKernel.setArg(3,_sourcePyramidDims.y());
    error = _nnfInitialFillKernel.setArg(4,_patchWidth);
    error = _nnfInitialFillKernel.setArg(5,*_randomBuffer);
    error = _nnfInitialFillKernel.setArg(6,*_targetPyramidSize);
    error = _nnfInitialFillKernel.setArg(7,*_sourcePyramidSize);
    error = _nnfInitialFillKernel.setArg(8,*(_anchorWeights[_anchorWeightsReadIndex]));
    error = _nnfInitialFillKernel.setArg(9,*(_nnfCoords[!_nnfReadIndex]));
    error = _nnfInitialFillKernel.setArg(10,*(_nnfCosts[!_nnfReadIndex]));

    //swap buffers
    _nnfReadIndex = !_nnfReadIndex;

    error = _commandQueue.enqueueNDRangeKernel(_nnfInitialFillKernel,
                                               cl::NullRange,
                                               cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                               cl::NullRange);

}

void HoleFillPatchMatchOpenCL::enqueueSetupNextNNF(const core::IntCoord& prevTargetDims, const core::IntCoord& prevSourceDims)
{
    //Take the following steps:
    //-Recreate nnfCoords/Costs (write buffer only!)
    //-Upsample nnf coords (not costs)
    //      For each (newX,newY), need to
    //-Blend to get new targetPyramidSize (which already exists and has mask=false values set correctly, unlike in
    // CPU implementation, where we would be required to do something like initMaskedOutPartsOfTargetPyramidSize
    //-Compute nnf costs
    //-Finish OpenCL queue (because we're about to do buffer deletion/recreation on nnf read buffer).
    //-Recreate nnfCoords/Costs read buffer (so they are correct size, in anticipation of search/prop)
    //-Swap nnf buffers
    cl_int error = CL_SUCCESS;

    //recreate the write buffer with new size
    _nnfCosts[!_nnfReadIndex] = std::make_unique< cl::Buffer >(
        _context,
        CL_MEM_READ_WRITE,
        sizeof(float)*_targetPyramidDims.x()*_targetPyramidDims.y(),
        nullptr,
        &error );
    _nnfCoords[!_nnfReadIndex]= std::make_unique< cl::Buffer >( 
        _context,
        CL_MEM_READ_WRITE,
        2*sizeof(int)*_targetPyramidDims.x()*_targetPyramidDims.y(),
        nullptr,
        &error );

    //    int prevTargetWidth,
    //    int prevTargetHeight,
    //    global int* nextTargetMask,
    //    global int* nextSourceMask,
    //    int nextSourceWidth,
    //    int nextSourceHeight,
    //    int prevSourceWidth,
    //    int prevSourceHeight,
    //    int patchWidth,
    //    global ulong* randomBuffer,
    //    global int* prevNNFCoords,
    //    global int* nextNNFCoords)
    //upsample coords
    error = _nnfUpsampleCoordsKernel.setArg(0,prevTargetDims.x());
    error = _nnfUpsampleCoordsKernel.setArg(1,prevTargetDims.y());
    error = _nnfUpsampleCoordsKernel.setArg(2,*_targetMaskPyramidSize);
    error = _nnfUpsampleCoordsKernel.setArg(3,*_sourceMaskPyramidSize);
    error = _nnfUpsampleCoordsKernel.setArg(4,_sourcePyramidDims.x());
    error = _nnfUpsampleCoordsKernel.setArg(5,_sourcePyramidDims.y());
    error = _nnfUpsampleCoordsKernel.setArg(6,prevSourceDims.x());
    error = _nnfUpsampleCoordsKernel.setArg(7,prevSourceDims.y());
    error = _nnfUpsampleCoordsKernel.setArg(8,_patchWidth);
    error = _nnfUpsampleCoordsKernel.setArg(9,*_randomBuffer);
    error = _nnfUpsampleCoordsKernel.setArg(10,*(_nnfCoords[_nnfReadIndex]));
    error = _nnfUpsampleCoordsKernel.setArg(11,*(_nnfCoords[!_nnfReadIndex]));
    error = _commandQueue.enqueueNDRangeKernel(_nnfUpsampleCoordsKernel,
                                               cl::NullRange,
                                               cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                               cl::NullRange);

    //blend to get new targetImagePyramidSize from coords
    error = _blendKernel.setArg(0,*(_nnfCoords[!_nnfReadIndex]));
    error = _blendKernel.setArg(1,*_targetMaskPyramidSize);
    error = _blendKernel.setArg(2,*_sourceMaskPyramidSize);
    error = _blendKernel.setArg(3,*(_anchorWeights[_anchorWeightsReadIndex]));
    error = _blendKernel.setArg(4,*_sourcePyramidSize);
    error = _blendKernel.setArg(5,*_targetPyramidSize);
    error = _blendKernel.setArg(6,_patchWidth);
    error = _commandQueue.enqueueNDRangeKernel(_blendKernel,
                                               cl::NullRange,
                                               cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                               cl::NullRange);

    //now find costs
     //       __read_only image2d_t targetImage,
     //       __read_only image2d_t sourceImage,
     //       global float* anchorWeights,
     //       global int* targetMask,
     //       global int* sourceMask,
     //       int sourceWidth,
     //       int patchWidth,
     //       global int* nnfCoords, //read only
     //       global float* nnfCosts //write only
    error = _nnfCostsKernel.setArg(0,*_targetPyramidSize);
    error = _nnfCostsKernel.setArg(1,*_sourcePyramidSize);
    error = _nnfCostsKernel.setArg(2,*(_anchorWeights[_anchorWeightsReadIndex]));
    error = _nnfCostsKernel.setArg(3,*_targetMaskPyramidSize);
    error = _nnfCostsKernel.setArg(4,*_sourceMaskPyramidSize);
    error = _nnfCostsKernel.setArg(5,_sourcePyramidDims.x());
    error = _nnfCostsKernel.setArg(6,_patchWidth);
    error = _nnfCostsKernel.setArg(7,*(_nnfCoords[!_nnfReadIndex]));
    error = _nnfCostsKernel.setArg(8,*(_nnfCosts[!_nnfReadIndex]));
    error = _commandQueue.enqueueNDRangeKernel(_nnfCostsKernel,
                                               cl::NullRange,
                                               cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                               cl::NullRange);

    //we need to finish queue at this point because we are about to recreate nnf objects
    error = _commandQueue.finish();

    //now recreate nnf read buffer to be the correct size
    _nnfCosts[_nnfReadIndex] = std::make_unique< cl::Buffer >(
        _context,
        CL_MEM_READ_WRITE,
        sizeof(float)*_targetPyramidDims.x()*_targetPyramidDims.y(),
        nullptr,
        &error );
    _nnfCoords[_nnfReadIndex]= std::make_unique< cl::Buffer >(
        _context,
        CL_MEM_READ_WRITE,
        2*sizeof(int)*_targetPyramidDims.x()*_targetPyramidDims.y(),
        nullptr,
        &error );

    //swap buffers (because the write buffer now has upsampled, valid coords/costs, whereas we just recreated
    //the read buffer with garbage data
    _nnfReadIndex = !_nnfReadIndex;

}

//PRECONDITION:  _targetMaskPyramidSize must already exist, have the correct size,
//and be queued to be filled with the correct data.
void HoleFillPatchMatchOpenCL::enqueueSetupAnchorWeights()
{
    cl_int error = CL_SUCCESS;

    //remember that this is a double buffered structure
    for(int i=0; i<2; i++)
    {
        _anchorWeights[i] = std::make_unique< cl::Buffer >(
            _context,
            CL_MEM_READ_WRITE,
            sizeof(float)*_targetPyramidDims.x()*_targetPyramidDims.y(),
            nullptr,
            &error );
    }

    //temporarily turn _anchorWeights[writeIndex] into an internal distance map based on
    //_targetMaskPyramidSize.
    error = _internalDistanceMapInitKernel.setArg(0,*_targetMaskPyramidSize);
    error = _internalDistanceMapInitKernel.setArg(1,*(_anchorWeights[!_anchorWeightsReadIndex]));
    error = _commandQueue.enqueueNDRangeKernel(_internalDistanceMapInitKernel,
                                               cl::NullRange,
                                               cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                               cl::NullRange);
    //swap buffers
    _anchorWeightsReadIndex = !_anchorWeightsReadIndex;

    //now compute the distance map with jumpflood
    int k = core::mathUtility::jumpfloodInitialK(_targetPyramidDims.x(),_targetPyramidDims.y());

    while(k>0)
    {
        error = _distanceMapStepKernel.setArg(0,*(_anchorWeights[_anchorWeightsReadIndex]));
        error = _distanceMapStepKernel.setArg(1,*(_anchorWeights[!_anchorWeightsReadIndex]));
        error = _distanceMapStepKernel.setArg(2,k);
        error = _commandQueue.enqueueNDRangeKernel(_distanceMapStepKernel,
                                                   cl::NullRange,
                                                   cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                                   cl::NullRange);
        //swap buffers
        _anchorWeightsReadIndex = !_anchorWeightsReadIndex;

        k/=2;
    }

    //Now turn a distmap into actual anchor weights
    error = _anchorWeightsFromInternalDistMapKernel.setArg(0,*(_anchorWeights[_anchorWeightsReadIndex]));
    error = _anchorWeightsFromInternalDistMapKernel.setArg(1,*(_anchorWeights[!_anchorWeightsReadIndex]));
    error = _anchorWeightsFromInternalDistMapKernel.setArg(2,_patchWidth);
    error = _commandQueue.enqueueNDRangeKernel(_anchorWeightsFromInternalDistMapKernel,
                                               cl::NullRange,
                                               cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                               cl::NullRange);
    //swap buffers
    _anchorWeightsReadIndex = !_anchorWeightsReadIndex;

}

void HoleFillPatchMatchOpenCL::enqueueBlend()
{
    cl_int error;

    error = _blendKernel.setArg(0,*(_nnfCoords[_nnfReadIndex]));
    error = _blendKernel.setArg(1,*_targetMaskPyramidSize);
    error = _blendKernel.setArg(2,*_sourceMaskPyramidSize);
    error = _blendKernel.setArg(3,*(_anchorWeights[_anchorWeightsReadIndex]));
    error = _blendKernel.setArg(4,*_sourcePyramidSize);
    error = _blendKernel.setArg(5,*_targetPyramidSize);
    error = _blendKernel.setArg(6,_patchWidth);
    error = _commandQueue.enqueueNDRangeKernel(_blendKernel,
                                               cl::NullRange,
                                               cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                               cl::NullRange);

    //Now need to update the nnf costs, since targetImage may be completely different now (costs may no longer be
    //valid).
      //      __read_only image2d_t targetImage,
      //      __read_only image2d_t sourceImage,
      //      global float* anchorWeights,
      //      global int* targetMask,
      //      global int* sourceMask,
      //      int sourceWidth,
      //      int patchWidth,
      //      global int* nnfCoords, //read only
      //      global float* nnfCosts //write only
    error = _nnfCostsKernel.setArg(0,*_targetPyramidSize);
    error = _nnfCostsKernel.setArg(1,*_sourcePyramidSize);
    error = _nnfCostsKernel.setArg(2,*(_anchorWeights[_anchorWeightsReadIndex]));
    error = _nnfCostsKernel.setArg(3,*_targetMaskPyramidSize);
    error = _nnfCostsKernel.setArg(4,*_sourceMaskPyramidSize);
    error = _nnfCostsKernel.setArg(5,_sourcePyramidDims.x());
    error = _nnfCostsKernel.setArg(6,_patchWidth);
    error = _nnfCostsKernel.setArg(7,*(_nnfCoords[_nnfReadIndex]));
    error = _nnfCostsKernel.setArg(8,*(_nnfCosts[_nnfReadIndex]));
    error = _commandQueue.enqueueNDRangeKernel(_nnfCostsKernel,
                                               cl::NullRange,
                                               cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                               cl::NullRange);

}

void HoleFillPatchMatchOpenCL::enqueueSearch()
{
    //    global ulong* randomSeeds,
    //    global float* anchorWeights,
    //    __read_only image2d_t targetImage,
    //    __read_only image2d_t sourceImage,
    //    global int* targetMask,
    //    global int* sourceMask,
    //    int patchWidth,
    //    global int* nnfCoords, //readandwrite
    //    global float* nnfCosts //readandwrite
    cl_int error;

    error = _searchKernel.setArg(0,*_randomBuffer);
    error = _searchKernel.setArg(1,*(_anchorWeights[_anchorWeightsReadIndex]));
    error = _searchKernel.setArg(2,*_targetPyramidSize);
    error = _searchKernel.setArg(3,*_sourcePyramidSize);
    error = _searchKernel.setArg(4,*_targetMaskPyramidSize);
    error = _searchKernel.setArg(5,*_sourceMaskPyramidSize);
    error = _searchKernel.setArg(6,_patchWidth);
    //Note that, even though this kernel _does_ write to the passed nnf buffers, we
    //are passing the read buffers, not the write buffers.  This is because the kernel
    //does not _need_ to use both buffers.  Might as well just write to the active buffer and
    //save a buffer swap (not that that would cost anything, necessarily).
    error = _searchKernel.setArg(7,*(_nnfCoords[_nnfReadIndex]));
    error = _searchKernel.setArg(8,*(_nnfCosts[_nnfReadIndex]));
    error = _commandQueue.enqueueNDRangeKernel(_searchKernel,
                                               cl::NullRange,
                                               cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                               cl::NullRange);
}

void HoleFillPatchMatchOpenCL::enqueuePropagate()
{
    //now compute the distance map with jumpflood
    int k = core::mathUtility::jumpfloodInitialK(_targetPyramidDims.x(),_targetPyramidDims.y());

    cl_int error = CL_SUCCESS;

    while(k>0)
    {

        //global float* anchorWeights,
        //__read_only image2d_t targetImage,
        //__read_only image2d_t sourceImage,
        //global int* targetMask,
        //global int* sourceMask,
        //int patchWidth,
        //int k,
        //global int* nnfCoordsRead, //readonly.
        //global int* nnfCoordsWrite, //writeonly
        //global float* nnfCostsRead //readonly
        //global float* nnfCostsWrite //writeonly
        error = _propagateKernel.setArg(0,*(_anchorWeights[_anchorWeightsReadIndex]));
        error = _propagateKernel.setArg(1,*_targetPyramidSize);
        error = _propagateKernel.setArg(2,*_sourcePyramidSize);
        error = _propagateKernel.setArg(3,*_targetMaskPyramidSize);
        error = _propagateKernel.setArg(4,*_sourceMaskPyramidSize);
        error = _propagateKernel.setArg(5,_patchWidth);
        error = _propagateKernel.setArg(6,k);
        error = _propagateKernel.setArg(7,*(_nnfCoords[_nnfReadIndex]));
        error = _propagateKernel.setArg(8,*(_nnfCoords[!_nnfReadIndex]));
        error = _propagateKernel.setArg(9,*(_nnfCosts[_nnfReadIndex]));
        error = _propagateKernel.setArg(10,*(_nnfCosts[!_nnfReadIndex]));
        error = _commandQueue.enqueueNDRangeKernel(_propagateKernel,
                                                   cl::NullRange,
                                                   cl::NDRange(_targetPyramidDims.x(),_targetPyramidDims.y()),
                                                   cl::NullRange);

        //swap buffers
        _nnfReadIndex = !_nnfReadIndex;

        k/=2;
    }
}

} // patchMatch

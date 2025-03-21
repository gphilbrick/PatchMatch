#ifndef IEC_HOLEFILLPATCHMATCH_H
#define IEC_HOLEFILLPATCHMATCH_H

#include <Core/image/imagetypes.h>

#include <PatchMatch/patchmatch.h>

namespace patchMatch {

/// Applies the PatchMatch algorithm specifically to the hole-filling problem,
/// where the hole to be filled in some image X is the "target image" and the 
/// rest of X is the "source image."
class HoleFillPatchMatch : public PatchMatch
{
public:
    HoleFillPatchMatch(
        int patchWidth,
        const core::ImageRGB& sourceImage,
        const core::ImageRGB& targetImage,
        const core::ImageBinary& targetMask,
        int numPyramidLevels );
protected:
    void makeTargetWeightsAndSourceMaskAtPyramidLevel( 
        core::ImageScalar& weightsDest,
        core::ImageBinary& sourceMaskDest,
        const core::ImageBinary& targetMaskPyramidSize,
        const core::IntCoord& sourcePyramidSize );
    void initMaskedOutPartsOfTargetPyramidSize(
        core::ImageRGB& targetPyramidSizeRgb );
    void makeFirstTargetPyramidSize(
        const core::ImageRGB& targetOriginalSize,
        const core::ImageBinary& targetMaskPyramidSize,
        core::ImageRGB& targetPyramidSize );
};

} // patchMatch

#endif // #include 

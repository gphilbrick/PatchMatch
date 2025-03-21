#include <holefillpatchmatch.h>
#include <patchmatchutility.h>

#include <Core/exceptions/runtimeerror.h>
#include <Core/image/imageutility.h>

namespace patchMatch {

HoleFillPatchMatch::HoleFillPatchMatch(
    int patchWidth,
    const core::ImageRGB& sourceImage,
    const core::ImageRGB& targetImage,
    const core::ImageBinary& targetMask,
    int numPyramidLevels)
    : PatchMatch(
        patchWidth,
        sourceImage,
        targetImage,
        targetMask,
        numPyramidLevels )
{
}

void HoleFillPatchMatch::makeFirstTargetPyramidSize(
    const core::ImageRGB& targetOriginalSize,
    const core::ImageBinary& targetMaskPyramidSize,
    core::ImageRGB& targetPyramidSize )
{
    core::ImageRGB downsampled;
    core::imageUtility::downsample< core::Vector3 >(
        targetOriginalSize,
        downsampled,
        targetMaskPyramidSize.size() );
    utility::holeFillingInitialFill(
        downsampled,
        targetMaskPyramidSize,
        targetPyramidSize );
}

void HoleFillPatchMatch::makeTargetWeightsAndSourceMaskAtPyramidLevel(
    core::ImageScalar& weightsDest,
    core::ImageBinary& sourceMaskDest,
    const core::ImageBinary& targetMaskPyramidSize,
    const core::IntCoord& sourcePyramidSize)
{
    core::IntCoord targetSize = targetMaskPyramidSize.size();
    if ( targetSize != sourcePyramidSize ) {
        THROW_RUNTIME( "Wrong target size" );
    }
    if( weightsDest.size() != targetSize ) {
        weightsDest.recreate(targetSize.x(),targetSize.y());
    }

    // Make source mask be the complement of (targetMask dilated by structuring element the size of a patch).
    const auto pWidth = patchWidth();
    core::ImageBinary structure( pWidth, pWidth, true );
    core::imageUtility::dilate(
        structure,
        core::IntCoord( pWidth / 2, pWidth / 2 ),
        targetMaskPyramidSize,
        sourceMaskDest );
    //Now reverse the source mask
    for( int x = 0; x < sourceMaskDest.width(); x++ ) {
        for( int y = 0; y < sourceMaskDest.height(); y++ ) {
            sourceMaskDest.set( x, y, !sourceMaskDest.get( x, y ) );
        }
    }

    // Get anchor weights. The deeper a point is in the hole, the lower its weight.
    core::imageUtility::getDistanceMapBidirectional( targetMaskPyramidSize, weightsDest );
    const double outsideHoleWeight = 100. ;
    const double gamma = 2.0;
    double overlapDist = patchWidth() / 2;
    for( int x = 0; x < weightsDest.width(); x++ ) {
        for( int y = 0; y < weightsDest.height(); y++ ) {
            const auto dist = weightsDest.get(x,y);

            if( dist < 0 ) {
                // This is outside the hole.  We are still assigning it
                // a weight because we want to weight outside the hole
                // participants in the patch cost step.
                weightsDest.set( x, y, outsideHoleWeight );
                continue;
            }

            double weight = pow( gamma,-dist );
            if( dist >= 0 && dist <= overlapDist ) {
                weight*=2.0;
            }
            weightsDest.set( x, y, weight );
        }
    }
}

void HoleFillPatchMatch::initMaskedOutPartsOfTargetPyramidSize(
    core::ImageRGB& targetPyramidSizeRgb )
{
    // The mask=false parts of target image should be equal to the source image values at same location
    getSourceImagePyramidSize( targetPyramidSizeRgb );
}

} // patchMatch
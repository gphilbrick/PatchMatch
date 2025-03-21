#ifndef IEC_PATCHMATCHUTILITY_H
#define IEC_PATCHMATCHUTILITY_H

#include <Core/image/imagetypes.h>

#include <algorithm>

namespace core {
class IntCoord;
class Vector3;
template<typename T>
class TwoDArray;
class NNF;
} // core

namespace patchMatch {
namespace utility {

bool patchWidthValid( int patchWidth );

// Return the value k such that
// smallest pyramid-level target size = originalTargetSize * k
// smallest pyramid-level source size = originalSouceSize * k
double smallestPyramidLevelRatio(
    const core::IntCoord& targetOriginalSize, 
    const core::IntCoord& sourceOriginalSize, 
    int patchWidth );

/// Store in 'targetSize' and 'sourceSize' what the target and source images' sizes
/// should be at pyramid level 'pyramidLevel'.
void pyramidLevelSizes(
    int pyramidLevel, 
    int numPyramidLevels, 
    int patchWidth,
    const core::IntCoord& targetOriginalSize,
    const core::IntCoord& sourceOriginalSize,
    core::IntCoord& targetSize, 
    core::IntCoord& sourceSize );

/// Return the cost associated with mapping 'targetAnchor' in the target image to 'sourceAnchor'
/// in the source image. 'sourceAnchor' and 'targetAnchor' must be valid anchor positions: far enough 
/// away from the edges of their images (at least 'patchWidth'/2). 'patchWidth' is odd number >=3,
/// smaller than the dimensions of 'source'/'target'.
double patchCost(
    const core::IntCoord& sourceAnchor,
    const core::IntCoord& targetAnchor,
    const int patchWidth,
    const core::ImageRGB& source,
    const core::ImageRGB& target,
    const core::ImageScalar& anchorWeights,
    const double costNotToExceed=std::numeric_limits<double>::max());

/// Return whether (x,y) is far enough away from the border of the implied image that
/// a patch of width 'patchWidth' placed there would not extend out of the image.
bool isPossibleAnchorPosition( int x, int y, int patchWidth, const core::IntCoord& imageSize );
bool isPossibleAnchorPosition( const core::IntCoord& coord, int patchWidth, const core::IntCoord& imageSize );

/// 'targetMask' - true means hole-to-be-filled (an actual part of the "target image").
void holeFillingInitialFill(
    const core::ImageRGB& target,
    const core::ImageBinary& targetMask, 
    core::ImageRGB& targetInitialFill );

} // utility
} // patchMatch

#endif // #include 

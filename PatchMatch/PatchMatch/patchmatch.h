#ifndef IEC_PATCHMATCH_H
#define IEC_PATCHMATCH_H

#include <Core/image/imagetypes.h>

#include <memory>

namespace core {
class IntCoord;
} // core

namespace patchMatch {

/// Allows performing the PatchMatch algorithm one step at a time. The PatchMatch 
/// algorithm entails building a target image by blending together patches taken from some source
/// image, with a nearest-neighbor field (NNF) indicating which source-image patch is to be pasted
/// at each target-image location. The algorithm iteratively improves the NNF at each level of 
/// a "pyramid" where the first level (highest-numbered pyramid level) operates on very low-resolution
/// versions of the target/source images and the final level (pyramid level 0) involves the full-
/// resolution versions of the images. 
class PatchMatch
{
public:
    /// Set up the first, smallest-resolution pyramid level (numbered 'numPyramidLevels'-1) with
    /// a randomized NNF. 'patchWidth' must be greater than 1. 'sourceImage' is the full-size
    /// (pyramid level 0) source image from which patches will be taken for blending the target
    /// image. The source image must not have a dimension smaller than 'patchWidth'. The target image 
    /// is represented by both 'targetImage' and 'targetMask', which have the same dimensions, i.e., 
    /// the target image's full resolution (width and height must be >= 'patchWidth'). The target image
    /// encompasses those pixels of 'targetImage' where 'targetMask' is true; other pixels
    /// in 'targetImage' are left out of the process (the NNF does not have entries for them).
    /// 'numPyramidLevels' must be at least 1.
    PatchMatch(
        int patchWidth,
        const core::ImageRGB& sourceImage,
        const core::ImageRGB& targetImage,
        const core::ImageBinary& targetMask,
        int numPyramidLevels );
    virtual ~PatchMatch();

    /// Update the internally stored current-pyramid-size target image as per the current NNF.
    void blend();

    /// Improve the NNF by propagating better-match information between neighbors.
    void propagate();
    // Improve the NNF by considering random new source positions for each target position.
    void search();

    /// If 'this' is already at the final pyramid level--level 0--then do nothing and return 0. Otherwise,
    /// move to the next pyramid level and return the updated pyramid level.
    int moveToNextPyramidLevel();
    /// Return the current pyramid level; a return value of 0 means 'this' is on the final level
    /// (operating at th
    int currentPyramidLevel() const;
    int patchWidth() const;

    /// If 'this' has not been initialized, initialize first.
    void getTargetImagePyramidSize( core::ImageRGB& rgbStore );
    /// If 'this' has not been initialized, initialize first.
    void getSourceImagePyramidSize( core::ImageRGB& rgbStore );
protected:
    virtual void makeTargetWeightsAndSourceMaskAtPyramidLevel(
        core::ImageScalar& weightsDest,
        core::ImageBinary& sourceMaskDest,
        const core::ImageBinary& targetMaskPyramidSize,
        const core::IntCoord& sourcePyramidSize ) = 0;
    virtual void makeFirstTargetPyramidSize(
        const core::ImageRGB& targetOriginalSize,
        const core::ImageBinary& targetMaskPyramidSize,
        core::ImageRGB& targetPyramidSize );
    virtual void initMaskedOutPartsOfTargetPyramidSize(
        core::ImageRGB& targetPyramidSizeRgb)=0;
private:
    struct Implementation;

    /// If the first pyramid level has never been begun, begin it; else do nothing.
    void ensureInitialized();
    void setUpNextPyramidLevel();

    std::unique_ptr< Implementation > _imp;
};

} // patchMatch

#endif // #include


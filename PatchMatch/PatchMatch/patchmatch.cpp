#include <patchMatch.h>
#include <patchMatchUtility.h>

#include <nnf.h>

#include <Core/exceptions/runtimeerror.h>
#include <Core/utility/mathutility.h>
#include <Core/utility/twodarray.h>
#include <Core/image/imageutility.h>

#include <omp.h>

#include <boost/optional.hpp>

#include <vector>

namespace patchMatch {

constexpr bool useJumpFloodForPropagation = false;

struct PatchMatch::Implementation
{
    void blend( core::ImageRGB& store ) const;
    void search();
    void propagateLineOrder( bool topToBottom );
    void propagateJumpFlood();

    /// Full-size source image (for pyramid level 0).
    core::ImageRGB _sourceOriginal;
    /// Current pyramid level-sized source image.
    core::ImageRGB _sourcePyramidSize;
    /// Same size as '_sourcePyramidSize'. True-marked pixels are valid potential 
    /// locations for the NNF to refer to; false-marked pixels are excluded from the NNF.
    core::ImageBinary _sourceMaskPyramidSize;

    /// Full-size target image (for pyramid level 0).
    core::ImageRGB _targetOriginal;
    /// Same size as '_targetOriginal' True-marked pixels identify the actual target
    /// image--a subset of '_targetOriginal' that the NNF must store source-image
    /// locations for. False-marked pixels are not part of the actual target image,
    /// are not involved in the NNF or the PatchMatch problem in any sense, e.g.,
    /// those pixels in '_targetOriginal' will not change their color.
    core::ImageBinary _targetMaskOriginal;
    core::ImageBinary _targetMaskPyramidSize;
    /// Current pyramid level-sized target image.
    core::ImageRGB _targetPyramidSize;
    /// Same size as the current pyramid-level target image. 
    core::ImageScalar _anchorWeightsPyramidSize;

    /// For every valid target image position X, identifies the location of a patch in the source image
    /// that should be pasted at X. This spatial correspondence is w.r.t. the current pyramid level.
    std::unique_ptr< NNF > _nnf;

    int _patchWidth = 0 ;
    /// boost::none means first pyramid level hasn't been set up yet.
    int _pyramidLevel = 0;
    int _numPyramidLevels = 0;
    bool _initialized = false;
};

void PatchMatch::Implementation::propagateJumpFlood()
{
    // I am implementing jumpflood as suggested in http://www.comp.nus.edu.sg/~tants/jfa/i3d06.pdf.
    // Recall the simple case where imageWidth=imageHeight and imageWidth is power of 2.  You just
    // start with k=imageWidth/2.  However, imageWidth and imageHeight are not necessarily equal and
    // not necessarily powers of 2

    const int targetDim = std::max(
        _targetPyramidSize.width(), 
        _targetPyramidSize.height() );
    int k = ceil(log((double)targetDim) / log(2.0));

    const auto& target = _targetPyramidSize;
    const auto& targetMask = _targetMaskPyramidSize;
    const auto& source = _sourcePyramidSize;
    const auto& sourceMask = _sourceMaskPyramidSize;
    const auto& anchorWeights = _anchorWeightsPyramidSize;
    const auto targetSize = target.size();
    const auto sourceSize = source.size();

    const auto yMin = _patchWidth / 2;
    const auto yMax = target.height() - _patchWidth / 2 - 1;
    const auto xMin = _patchWidth / 2;
    const auto xMax = target.width() - _patchWidth / 2 - 1;

    auto nnfBuffer = std::make_unique< NNF >();
    nnfBuffer->init(target.width(), target.height());
    NNF* nnfRead = _nnf.get();
    NNF* nnfWrite = nnfBuffer.get();

    while (k > 0) {
#pragma omp parallel
        {
#pragma omp for
            for (int y = yMin; y <= yMax; y++) {
                for (int x = xMin; x <= xMax; x++) {
                    const core::IntCoord targetCoord(x, y);
                    auto bestMatchCost = nnfRead->getStoredMatchCost(x, y);
                    core::IntCoord bestMatchCoord = nnfRead->getStoredSourceCoord(targetCoord);

                    // Check ever NNF neighbor in {x+i,y+j}, where i and j are {-k,0,k}.
                    // See if their propositions are better.
                    for (int i = -k; i <= k; i += k) {
                        for (int j = -k; j <= k; j++) {
                            if (i == 0 && j == 0) {
                                continue;
                            }

                            const core::IntCoord votingNeighbor(x + i, y + j);
                            if (!utility::isPossibleAnchorPosition(votingNeighbor, _patchWidth, targetSize)) {
                                continue;
                            }
                            if (!targetMask.get(votingNeighbor)) {
                                continue;
                            }
                            const core::IntCoord candidateMatch =
                                nnfRead->getStoredSourceCoord(votingNeighbor) - core::IntCoord(i, j);
                            if (!utility::isPossibleAnchorPosition(candidateMatch, _patchWidth, sourceSize)) {
                                continue;
                            }
                            if (!sourceMask.get(candidateMatch)) {
                                continue;
                            }
                            const auto matchCost = utility::patchCost(
                                candidateMatch,
                                targetCoord,
                                _patchWidth,
                                source,
                                target,
                                anchorWeights,
                                bestMatchCost);
                            if (matchCost < bestMatchCost) {
                                bestMatchCost = matchCost;
                                bestMatchCoord = candidateMatch;
                            }
                        }
                    }
                    nnfWrite->set(targetCoord, bestMatchCoord, bestMatchCost);
                }
            }
        } // omp
        k /= 2;
        std::swap(nnfRead, nnfWrite);
    }

    if ( _nnf.get()  ==  nnfWrite ) {
        _nnf = std::move( nnfBuffer );
    }
}

void PatchMatch::Implementation::blend( core::ImageRGB& dest ) const
{
    if (dest.width() != _targetPyramidSize.width()
     || dest.height() != _targetPyramidSize.height()) {
        dest.recreate( _targetPyramidSize.width(), _targetPyramidSize.height() );
    }

    const int yMin = 0;
    const int yMax = dest.height();
    const auto& source = _sourcePyramidSize;
    const auto& currentTarget = _targetPyramidSize;
    const auto& targetMask = _targetMaskPyramidSize;
    const auto& sourceMask = _sourceMaskPyramidSize;
    const auto& anchorWeights = _anchorWeightsPyramidSize;
    const auto& nnf = _nnf;
    const auto width = dest.width();
    const auto patchWidth = _patchWidth;

#pragma omp parallel
    {
#pragma omp for
        for (int y = yMin; y < yMax; y++) {
            for (int x = 0; x < width; x++) {
                // If this is masked out, just set to current target value
                if (!targetMask.get(x, y)) {
                    dest.set(x, y, currentTarget.get(x, y));
                    continue;
                }

                bool noValidContributors = true;
                core::Vector3 sum(0, 0, 0);
                double weightSum = 0.0;

                // Walk around all the patches that cover me.
                for (int patchX = -patchWidth / 2; patchX <= patchWidth / 2; patchX++) {
                    for (int patchY = -patchWidth / 2; patchY <= patchWidth / 2; patchY++) {
                        const auto targetAnchorX = x + patchX;
                        const auto targetAnchorY = y + patchY;
                        if (targetAnchorX < patchWidth / 2
                            || targetAnchorY < patchWidth / 2
                            || targetAnchorX > dest.width() - 1 - patchWidth / 2
                            || targetAnchorY > dest.height() - 1 - patchWidth / 2) {
                            continue;
                        }
                        if (!targetMask.get(targetAnchorX, targetAnchorY)) {
                            continue;
                        }

                        // Neighbor might point to a masked-out source anchor.
                        const auto sourceAnchor = nnf->getStoredSourceCoord(targetAnchorX, targetAnchorY);
                        const auto sourceCoord = sourceAnchor - core::IntCoord(patchX, patchY);
                        if (!sourceMask.get(sourceCoord)) {
                            continue;
                        }

                        const auto color = source.get(sourceCoord);

                        // Measure the local coherence in the NNF around (targetAnchorX,targetAnchorY).
                        double coherenceAmount = 0;
                        for (int i = -1; i <= 1; i++)
                        {
                            for (int j = -1; j <= 1; j++)
                            {
                                if (i == 0 && j == 0) continue;
                                const auto otherSourceAnchor = nnf->getStoredSourceCoord(
                                    targetAnchorX + i, targetAnchorY + i);
                                if (otherSourceAnchor == sourceAnchor + core::IntCoord(i, j))
                                {
                                    coherenceAmount += 1.0;
                                }
                            }
                        }

                        // Give a higher weight to a 'color' associated with (a) a high weight in 'anchorWeights' and/or
                        // (b) a higher "coherence".
                        const auto weight =
                            anchorWeights.get(targetAnchorX, targetAnchorY) + coherenceAmount * coherenceAmount * 0.5;

                        sum += color * weight;
                        weightSum += weight;
                        noValidContributors = false;
                    }
                }

                //Our weight sum can be zero in two cases:
                //  -bad weights are assigned to anchorWeights (ie. zeroes or negative numbers)
                //  -our blend pixel is an an invalid anchor position (on the border of the image) and has
                //   no valid-anchor un-masked contributing neighbors.  In short, noValidContributors=true.
                //
                if (noValidContributors) {
                    //Set to a warning color.
                    dest.set(x, y, core::Vector3(0, 0, 0));
                } else {
                    sum /= weightSum;
                    dest.set(x, y, sum);
                }
            }
        }
    } // omp
}

void PatchMatch::Implementation::search()
{
    const int yMin = _patchWidth / 2;
    const int yMax = _targetPyramidSize.height() - _patchWidth / 2 - 1;
    const int xMin = _patchWidth / 2;
    const int xMax = _targetPyramidSize.width() - _patchWidth / 2 - 1;

    const auto& const dest = _targetPyramidSize;
    const auto& const source = _sourcePyramidSize;
    const auto& targetMask = _targetMaskPyramidSize;
    const auto& sourceMask = _sourceMaskPyramidSize;
    const auto& anchorWeights = _anchorWeightsPyramidSize;
    const auto& nnf = _nnf;
    const auto patchWidth = _patchWidth;

    const double searchInitialRadius = std::max(source.width(), source.height());
    const double alpha = 0.5;

#pragma omp parallel 
    {
#pragma omp for
        for (int y = yMin; y <= yMax; y++) {
            for (int x = xMin; x <= xMax; x++) {
                if (!targetMask.get(x, y)) continue;
                const core::IntCoord targetAnchor(x, y);
                auto sourceAnchor = nnf->getStoredSourceCoord( targetAnchor );
                auto searchRadius = searchInitialRadius;
                while( searchRadius > 1. ) {
                    const auto sourceAnchorX = sourceAnchor.x();
                    const auto sourceAnchorY = sourceAnchor.y();

                    const int minX = std::max< int >(
                        patchWidth / 2,
                        (int)(sourceAnchorX - searchRadius) );
                    const int maxX = std::min< int >(
                        (int)(sourceAnchorX + searchRadius),
                        source.width() - patchWidth / 2 - 1);
                    const int minY = std::max< int >(
                        (int)(sourceAnchorY - searchRadius),
                        patchWidth / 2 );
                    const int maxY = std::min< int >(
                        (int)(sourceAnchorY + searchRadius),
                        source.height() - patchWidth / 2 - 1);

                    const auto candidateSourceX = core::mathUtility::randInt( minX, maxX );
                    const auto candidateSourceY = core::mathUtility::randInt( minY, maxY );

                    if (sourceMask.get(candidateSourceX, candidateSourceY))
                    {
                        const auto currentMatchCost = nnf->getStoredMatchCost(x, y);
                        const core::IntCoord potentialSourceAnchor(candidateSourceX, candidateSourceY);
                        const auto potentialMatchCost = 
                            utility::patchCost(
                                potentialSourceAnchor,
                                targetAnchor, 
                                patchWidth,
                                source, 
                                dest,
                                anchorWeights,
                                currentMatchCost );
                        if ( potentialMatchCost < currentMatchCost )
                        {
                            nnf->set( targetAnchor, potentialSourceAnchor, potentialMatchCost );
                            sourceAnchor = potentialSourceAnchor;
                        }
                    }
                    searchRadius *= alpha;
                }
            }
        }
    } // omp
}

void PatchMatch::Implementation::propagateLineOrder( bool topToBottom )
{
    const int yStart = topToBottom ? _patchWidth / 2 : _targetPyramidSize.height() - _patchWidth / 2 - 1;
    const int yEndExclusive = topToBottom ? _targetPyramidSize.height() - _patchWidth / 2 : _patchWidth / 2 - 1;
    const int inc = topToBottom ? 1 : -1;
    const int xStart = topToBottom ? _patchWidth / 2 : _targetPyramidSize.width() - _patchWidth / 2 - 1;
    const int xEndExclusive = topToBottom ? _targetPyramidSize.width() - _patchWidth / 2 : _patchWidth / 2 - 1;

    constexpr int numNeighbors = 2;
    std::array< core::IntCoord, numNeighbors > offsets;
    offsets[0] = core::IntCoord( -inc, 0 );
    offsets[1] = core::IntCoord( 0, -inc );

    for (int y = yStart; y != yEndExclusive; y += inc) {
        for (int x = xStart; x != xEndExclusive; x += inc) {
            const core::IntCoord anchor(x, y);

            for (int c = 0; c < numNeighbors; c++) {
                if (c == 0 && x == xStart) continue;
                if (c == 1 && y == yStart) continue;

                const auto neighborTargetAnchor = anchor + offsets[c];

                if (!_targetMaskPyramidSize.get(neighborTargetAnchor)) continue;

                //what is the current cost
                const auto currentMatchCost = _nnf->getStoredMatchCost(x, y);
                const auto candidateSourceAnchor = 
                    _nnf->getStoredSourceCoord(neighborTargetAnchor.x(), neighborTargetAnchor.y()) - offsets[c];

                if ( !_sourceMaskPyramidSize.get( candidateSourceAnchor ) ) continue;
                if ( !utility::isPossibleAnchorPosition(
                    candidateSourceAnchor.x(), 
                    candidateSourceAnchor.y(), 
                    _patchWidth, 
                    _sourcePyramidSize.size())) {
                    continue;
                }

                const auto potentialMatchCost = utility::patchCost(
                    candidateSourceAnchor,
                    anchor,
                    _patchWidth,
                    _sourcePyramidSize,
                    _targetPyramidSize,
                    _anchorWeightsPyramidSize,
                    currentMatchCost);
                if (potentialMatchCost < currentMatchCost) {
                    _nnf->set(anchor, candidateSourceAnchor, potentialMatchCost);
                }
            }
        }
    }
}

PatchMatch::PatchMatch(
    int patchWidth,
    const core::ImageRGB& sourceImage,
    const core::ImageRGB& targetImage,
    const core::ImageBinary& targetMask,
    int numPyramidLevels )
    : _imp( std::make_unique< Implementation >() )
{
    if (!utility::patchWidthValid(patchWidth)) {
        THROW_RUNTIME("Illegal patch width.");
    }

    if ( targetImage.width() < patchWidth
      || targetImage.height() < patchWidth
      || sourceImage.width() < patchWidth
      || sourceImage.height() < patchWidth) {
        THROW_RUNTIME( "Source and/or target image too small for the given patch size." );
    }
    if ( targetImage.width() != targetMask.width() || targetImage.height() != targetMask.height()) {
        THROW_RUNTIME("targetMask and targetImage must have same size.");
    }
    if (numPyramidLevels < 1) {
        THROW_RUNTIME("Illegal numPyramidLevels");
    }

    _imp->_numPyramidLevels = numPyramidLevels;
    _imp->_pyramidLevel = numPyramidLevels - 1;
    _imp->_patchWidth = patchWidth;
    core::ImageRGB::clone( targetImage, _imp->_targetOriginal );
    core::ImageRGB::clone( sourceImage, _imp->_sourceOriginal );
    core::ImageBinary::clone( targetMask, _imp->_targetMaskOriginal );
}

PatchMatch::~PatchMatch()
{
}

void PatchMatch::ensureInitialized()
{
    if ( !_imp->_initialized) {
        setUpNextPyramidLevel();
        _imp->_initialized = true;
    }
}

int PatchMatch::patchWidth() const
{ 
    return _imp->_patchWidth; 
}

void PatchMatch::propagate()
{
    ensureInitialized();
    if( useJumpFloodForPropagation ) {
        _imp->propagateJumpFlood();
    } else {
        _imp->propagateLineOrder(true);
        _imp->propagateLineOrder(false);
    }
}

void PatchMatch::getTargetImagePyramidSize(core::ImageRGB& rgbStore)
{
    ensureInitialized();
    core::ImageRGB::clone( _imp->_targetPyramidSize,rgbStore );
}

void PatchMatch::getSourceImagePyramidSize( core::ImageRGB& rgbStore )
{
    ensureInitialized();
    core::ImageRGB::clone( _imp->_sourcePyramidSize, rgbStore );
}

void PatchMatch::makeFirstTargetPyramidSize(
    const core::ImageRGB& targetOriginalSize,
    const core::ImageBinary& targetMaskPyramidSize,
    core::ImageRGB& targetPyramidSize)
{
    //get our first target image by downsampling from original input target image
    core::imageUtility::downsample<core::Vector3>(targetOriginalSize,targetPyramidSize,targetMaskPyramidSize.size());
}

void PatchMatch::setUpNextPyramidLevel()
{
    if (_imp->_initialized ) {
        if (_imp->_pyramidLevel == 0) {
            return;
        }
        _imp->_pyramidLevel--;
    } else {
        // This is the first (highest-numbered) level of the pyramid operating on initial, small images.
        _imp->_pyramidLevel = _imp->_numPyramidLevels - 1;
    }

    core::IntCoord targetSize, sourceSize;
    utility::pyramidLevelSizes(
        _imp->_pyramidLevel,
        _imp->_numPyramidLevels,
        _imp->_patchWidth,
        _imp->_targetOriginal.size(),
        _imp->_sourceOriginal.size(),
        targetSize,
        sourceSize );

    const auto previousSourceSize = _imp->_sourcePyramidSize.size();
    core::ImageBinary prevTargetMask;
    core::ImageBinary::clone( _imp->_targetMaskPyramidSize, prevTargetMask ); 

    core::imageUtility::downsample< core::Vector3 >(
        _imp->_sourceOriginal,
        _imp->_sourcePyramidSize,
        sourceSize);
    core::imageUtility::downsampleBoolean(
        _imp->_targetMaskOriginal,
        _imp->_targetMaskPyramidSize,
        targetSize,
        true );

    makeTargetWeightsAndSourceMaskAtPyramidLevel(
        _imp->_anchorWeightsPyramidSize,
        _imp->_sourceMaskPyramidSize,
        _imp->_targetMaskPyramidSize,
        sourceSize );

    const int numTriesPerTargetPixel = std::max( targetSize.x(), targetSize.y() ) * 10;

    if( !_imp->_initialized ) {
        makeFirstTargetPyramidSize(
            _imp->_targetOriginal,
            _imp->_targetMaskPyramidSize,
            _imp->_targetPyramidSize );

        // Randomly initialize the NNF.
        _imp->_nnf = std::make_unique< NNF >();
        _imp->_nnf->init( targetSize.x(), targetSize.y() );
        for( int x = _imp->_patchWidth/2; x < targetSize.x() - _imp->_patchWidth / 2; x++ ) {
            for( int y = _imp->_patchWidth / 2; y < targetSize.y() - _imp->_patchWidth / 2; y++ ) {
                const core::IntCoord targetCoord(x,y);

                if( !_imp->_targetMaskPyramidSize.get( targetCoord ) ) {
                    continue;
                }

                // Guarantee that (x,y) maps to some valid position in the source image. Also _try_ to ensure 
                // that (x,y) maps to a location which is marked true in the source mask (this cannot
                // be guaranteed).
                for( int attempt = 0; attempt < numTriesPerTargetPixel; attempt++ ) {
                    const core::IntCoord sourceCoord(                         
                        _imp->_patchWidth / 2 
                            + core::mathUtility::randInt( 0, _imp->_sourcePyramidSize.width() - _imp->_patchWidth ),
                        _imp->_patchWidth / 2 
                            + core::mathUtility::randInt( 0, _imp->_sourcePyramidSize.height() - _imp->_patchWidth ) );
                    if( _imp->_sourceMaskPyramidSize.get(sourceCoord) ) {
                        // We have found a source coord that is valid _and_ unmasked. 
                        const auto costThere = utility::patchCost(
                            sourceCoord,
                            targetCoord,
                            _imp->_patchWidth,
                            _imp->_sourcePyramidSize,
                            _imp->_targetPyramidSize,
                            _imp->_anchorWeightsPyramidSize,
                            std::numeric_limits<double>::max() );
                        _imp->_nnf->set( targetCoord, sourceCoord, costThere );
                        break;
                    } else {
                        // This source coord is a valid position but masked.  
                        _imp->_nnf->set( targetCoord, sourceCoord, std::numeric_limits<double>::max() );
                    }
                }
            }
        }
    } else {
        // Upsample the NNF from itself. Note that as we build the increased-size NNF, 
        // we do not set valid patch costs. This is because the new target image does 
        // not exist yet, and can only be constructed from the new NNF we are building 
        // right here.
        auto nextNNF = std::make_unique< NNF >();
        nextNNF->init( targetSize.x(), targetSize.y() );

        const double oldWidth = static_cast< double >( _imp->_nnf->width() );
        const double oldHeight = static_cast< double >( _imp->_nnf->height() );
        for( int x = _imp->_patchWidth / 2; x < targetSize.x() - _imp->_patchWidth / 2; x++ ) {
            for( int y = _imp->_patchWidth / 2; y < targetSize.y() - _imp->_patchWidth / 2; y++ ) {
                const core::IntCoord targetCoord( x , y );

                if( !_imp->_targetMaskPyramidSize.get( targetCoord ) ) {
                    continue;
                }

                // Intuitively, we should map (x,y) to prev-pyramid target image space (oldX,oldY), ask the NNF which
                // prev-pyramid source coords this is connected to, and then convert those source coords
                // to next-pyramid source space and use those coords for (x,y). But there are cases 
                // where this will not work:
                //      - (oldX,oldY) is an invalid or masked position w.r.t. prev-pyramid target image.
                //      - the prev-pyramid NNF maps (oldX,oldY) to a source position which, when converted 
                //        to next-pyramid source image space, is an invalid or masked position w.r.t. the
                //        next-pyramid source image.

                const int oldX = (int)((((double)x)/((double)(targetSize.x()-1)))*((double)(oldWidth-1)));
                const int oldY = (int)((((double)y)/((double)(targetSize.y()-1)))*((double)(oldHeight-1)));
                bool useUpsampledSourceCoord = true;
                core::IntCoord upsampledSourceCoord;
                if( utility::isPossibleAnchorPosition(
                        oldX,
                        oldY,
                        _imp->_patchWidth,
                        previousSourceSize ) 
                 && prevTargetMask.get( oldX, oldY ) ) {
                    // Just do "nearest neighbor" NNF sampling by. Interpolating values does not
                    // work here, because adjacent entries in the NNF may differ greatly.
                    upsampledSourceCoord = _imp->_nnf->getStoredSourceCoord( oldX, oldY );

                    // Convert old source coord from previous pyramid level size to new pyramid leve size
                    upsampledSourceCoord = core::IntCoord(
                        (int)((((double)upsampledSourceCoord.x())/((double)(previousSourceSize.x()-1)))*((double)(sourceSize.x()-1))),
                        (int)((((double)upsampledSourceCoord.y())/((double)(previousSourceSize.y()-1)))*((double)(sourceSize.y()-1)))
                                );
                    if( !utility::isPossibleAnchorPosition(
                            upsampledSourceCoord,
                            _imp->_patchWidth,
                            _imp->_sourcePyramidSize.size() ) 
                     || !_imp->_sourceMaskPyramidSize.get( upsampledSourceCoord ) ) {
                        useUpsampledSourceCoord=false;
                    }
                } else {
                    useUpsampledSourceCoord = false;
                }

                if( useUpsampledSourceCoord ) {
                    nextNNF->set(
                        targetCoord,
                        upsampledSourceCoord,
                        std::numeric_limits<double>::max() );
                } else {
                    // Just assign a random source position to (x,y).

                    // We will guarantee that (x,y) maps to some valid position
                    // in the next-pyramid source image.  We will _try_ to ensure that (x,y) 
                    // maps to a location which is marked true in _sourceMaskPyramidSize, but we cannot guarantee this.
                    for(int attempt=0; attempt<numTriesPerTargetPixel; attempt++) {
                        const core::IntCoord sourceCoord( 
                            _imp->_patchWidth / 2 
                                + core::mathUtility::randInt( 0, _imp->_sourcePyramidSize.width() - _imp->_patchWidth ),
                            _imp->_patchWidth / 2 
                                + core::mathUtility::randInt( 0, _imp->_sourcePyramidSize.height() - _imp->_patchWidth ) );
                        if( _imp->_sourceMaskPyramidSize.get( sourceCoord ) ) {
                            nextNNF->set( 
                                targetCoord,
                                sourceCoord,
                                std::numeric_limits<double>::max() );
                            break;
                        } else {
                            nextNNF->set(
                                targetCoord,
                                sourceCoord,
                                std::numeric_limits<double>::max() );
                        }
                    }
                }
            }
        }
        std::swap( _imp->_nnf, nextNNF );

        _imp->_targetPyramidSize.recreate( _imp->_nnf->width(), _imp->_nnf->height() );
        // This ensures that '_targetPyramidSize' will have correct values for
        // targetMask=false pixels.
        initMaskedOutPartsOfTargetPyramidSize( _imp->_targetPyramidSize );
        // Get our new target image using the new NNF.
        blend();

        // Calculate patch costs across the NNF.
        for(int x = _imp->_patchWidth / 2; x < _imp->_targetPyramidSize.width() - _imp->_patchWidth / 2; x++) {
            for( int y = _imp->_patchWidth / 2; y < _imp->_targetPyramidSize.height() - _imp->_patchWidth / 2; y++) {
                if (!_imp->_targetMaskPyramidSize.get(x, y)) {
                    continue;
                }
                const core::IntCoord targetCoord(x,y);

                //we have found a source coord that is valid _and_ unmasked.  We are done
                core::IntCoord sourceCoord = _imp->_nnf->getStoredSourceCoord( targetCoord );
                const auto costThere = utility::patchCost(
                    sourceCoord,
                    targetCoord,
                    _imp->_patchWidth,
                    _imp->_sourcePyramidSize,
                    _imp->_targetPyramidSize,
                    _imp->_anchorWeightsPyramidSize,
                    std::numeric_limits<double>::max() );
                _imp->_nnf->set( targetCoord, sourceCoord, costThere );
            }
        }
    }
}

int PatchMatch::moveToNextPyramidLevel()
{
    ensureInitialized();
    setUpNextPyramidLevel();
    return _imp->_pyramidLevel;
}

void PatchMatch::search()
{
    ensureInitialized();
    _imp->search();
}

void PatchMatch::blend()
{
    ensureInitialized();
    _imp->blend( _imp->_targetPyramidSize );
}

int PatchMatch::currentPyramidLevel() const
{ 
    return _imp->_pyramidLevel;
}

} // patchMatch


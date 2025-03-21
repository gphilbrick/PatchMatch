#include <patchmatchutility.h>

#include <Core/exceptions/runtimeerror.h>
#include <Core/utility/intcoord.h>
#include <Core/utility/vector3.h>
#include <Core/utility/twodarray.h>
#include <Core/image/imageutility.h>

namespace patchMatch {
namespace utility {

bool patchWidthValid(int patchWidth)
{
    return patchWidth >= 3 && ( patchWidth % 2 == 1);
}

double smallestPyramidLevelRatio(
    const core::IntCoord& targetOriginalSize, 
    const core::IntCoord& sourceOriginalSize, 
    int patchWidth )
{
    if( !patchWidthValid( patchWidth ) ) {
        THROW_RUNTIME("Illegal patch width.");
    }

    if( targetOriginalSize.x() < patchWidth 
     || targetOriginalSize.y() < patchWidth 
     || sourceOriginalSize.x() < patchWidth 
     || sourceOriginalSize.y() < patchWidth ) {
        THROW_RUNTIME("Initial image too small.");
    }

    const int minMaxDim = 50;
    if ( patchWidth > minMaxDim ) {
        THROW_RUNTIME( "Patch size too large." );
    }

    // We seek the smallest possible k in (0,1) such that size of smallest pyramid level target =
    // targetOriginalSize*k.  k cannot be so small that it causes the target image to be smaller
    // than minMaxDim in length or width.  k also cannot be so small that the length or width
    // of source or target ends up being less than patchWidth.

    const double kHardMin = ((double)patchWidth) / ( (double)std::min( std::min(targetOriginalSize.x(),targetOriginalSize.y()),
                                                                 std::min(sourceOriginalSize.x(),sourceOriginalSize.y())) );
    const double kIdeal = ((double)minMaxDim) / ((double)std::max(targetOriginalSize.x(),targetOriginalSize.y()));
    return kHardMin > kIdeal ? kHardMin : kIdeal;
}

void pyramidLevelSizes(
    int pyramidLevel, 
    int numPyramidLevels, 
    int patchWidth,
    const core::IntCoord& targetOriginalSize,
    const core::IntCoord& sourceOriginalSize,
    core::IntCoord& targetSize, 
    core::IntCoord& sourceSize )
{    
    if( pyramidLevel < 0 || pyramidLevel > numPyramidLevels - 1 ) {
        THROW_RUNTIME("Illegal pyramid level.");
    }

    if( numPyramidLevels == 1 ) {
        targetSize = targetOriginalSize;
        sourceSize = sourceOriginalSize;
        return;
    }

    const auto smallestK = smallestPyramidLevelRatio(
        targetOriginalSize,
        sourceOriginalSize,
        patchWidth );

    // These early short circuits should not really be necessary, but I am paranoid
    // about floating point + off-by-one integer errors.
    if( pyramidLevel == numPyramidLevels - 1 ) {
        targetSize = core::IntCoord( (int)(smallestK*((double)targetOriginalSize.x())),
                               (int)(smallestK*((double)targetOriginalSize.y())));
        sourceSize = core::IntCoord( (int)(smallestK*((double)sourceOriginalSize.x())),
                               (int)(smallestK*((double)sourceOriginalSize.y())));
        return;
    }
    if( pyramidLevel == 0 ) {
        targetSize=targetOriginalSize;
        sourceSize =sourceOriginalSize;
        return;
    }

    //The general equation is this:
    // pyramidLevelDim = originalLevelDim* (k^pyramidLevel), where k is some number in (0,1) which
    // we calculate here based on our knowledge that k=smallestK when pyramidLevel = numPyramidLevels-1
    double k = exp(  log(smallestK)/((double)(numPyramidLevels-1)));
    targetSize = core::IntCoord(
                (int)(((double)targetOriginalSize.x())*pow(k,((double)pyramidLevel))),
                (int)(((double)targetOriginalSize.y())*pow(k,((double)pyramidLevel)))
                );
    sourceSize = core::IntCoord(
                (int)(((double)sourceOriginalSize.x())*pow(k,((double)pyramidLevel))),
                (int)(((double)sourceOriginalSize.y())*pow(k,((double)pyramidLevel)))
                );
}

void holeFillingInitialFill(
    const core::ImageRGB& targetOriginal,
    const core::ImageBinary& targetMask,
    core::ImageRGB& targetInitialFill)
{
    if ( targetOriginal.width() < 2 || targetOriginal.height() < 2 ) {
        THROW_RUNTIME("Will not work for image this small.");
    }
    if ( targetOriginal.width() != targetMask.width()
     ||  targetOriginal.height() != targetMask.height() ) {
        THROW_RUNTIME("Invalid input.");
    }

    //do initial fill
    if( targetInitialFill.width() != targetOriginal.width() 
     || targetInitialFill.height()!=targetOriginal.height() ) {
        targetInitialFill.recreate(targetOriginal.width(),targetOriginal.height());
    }

    //start with black in the hole
    double numHolePixels = 0;
    for(int x=0; x<targetInitialFill.width(); x++) {
        for(int y=0; y<targetInitialFill.height(); y++) {
            if(!targetMask.get(x,y)) {
                targetInitialFill.set(x,y,targetOriginal.get(x,y));
            } else {
                targetInitialFill.set(x,y,core::Vector3(0,0,0));
                numHolePixels++;
            }
        }
    }
    if(numHolePixels<0) {
        return;
    }

    //I am doing this in lockstep
    core::ImageRGB buffer1,buffer2;
    core::ImageRGB::clone(targetInitialFill,buffer1);
    core::ImageRGB::clone(targetInitialFill,buffer2);
    core::ImageRGB *writeBuffer=&buffer1, *readBuffer=&buffer2;
    const double pixelChangeThreshold=0.0000001;
    int numBlurs=0;
    while(true) {
        int numChangedThisTime=0;
        for(int x=0; x<targetInitialFill.width(); x++) {
            for(int y=0; y<targetInitialFill.height(); y++) {
                if(targetMask.get(x,y)) {
                    double weightSum=0;
                    core::Vector3 colorSum(0,0,0);
                    if(x>0) {
                        colorSum += readBuffer->get(x-1,y);
                        weightSum+=1.0;
                    } 
                    if(x<targetInitialFill.width()-1) {
                        colorSum += readBuffer->get(x+1,y);
                        weightSum+=1.0;
                    }
                    if(y>0) {
                        colorSum += readBuffer->get(x,y-1);
                        weightSum+=1.0;
                    }
                    if(y<targetInitialFill.height()-1) {
                        colorSum += readBuffer->get(x,y+1);
                        weightSum+=1.0;
                    }
                    colorSum/=weightSum;

                    core::Vector3 current = readBuffer->get(x,y);
                    const auto rDiff = current.r()-colorSum.r();
                    const auto gDiff = current.g()-colorSum.g();
                    const auto bDiff = current.b()-colorSum.b();
                    const auto diff = rDiff*rDiff + gDiff*gDiff + bDiff*bDiff;
                    if(diff>pixelChangeThreshold) {
                        numChangedThisTime++;
                    }
                    writeBuffer->set(x,y,colorSum);
                }
            }
        }

        if(!numChangedThisTime) break;
        numBlurs++;

        if(readBuffer==&buffer1) {
            readBuffer=&buffer2;
            writeBuffer=&buffer1;
        } else {
            readBuffer=&buffer1;
            writeBuffer=&buffer2;
        }
    }

    core::ImageRGB::clone(*writeBuffer,targetInitialFill);
}

bool isPossibleAnchorPosition(
    const core::IntCoord& coord, 
    int patchWidth,
    const core::IntCoord& imageSize )
{
    return isPossibleAnchorPosition(
        coord.x(),
        coord.y(),
        patchWidth,
        imageSize );
}

bool isPossibleAnchorPosition( int x, int y, int patchWidth,const core::IntCoord& imageSize )
{
    return x>=patchWidth/2 
        && x<imageSize.x()-patchWidth/2 
        && y>=patchWidth/2 
        && y<imageSize.y()-patchWidth/2;
}

double patchCost(
    const core::IntCoord& sourceAnchor,
    const core::IntCoord& targetAnchor,
    const int patchWidth,
    const core::ImageRGB& source,
    const core::ImageRGB& target,
    const core::ImageScalar& anchorWeights,
    const double costNotToExceed )
{
    double sumCost = 0;

    //note that this loop structure is built around two assumptions:
    // -patch origin is at center
    // -patch width is odd.
    for(int patchX = -patchWidth/2; patchX<=patchWidth/2; patchX++) {
        for(int patchY=-patchWidth/2; patchY<=patchWidth/2; patchY++) {
            core::IntCoord patchOffset(patchX,patchY);
            core::IntCoord targetCoord = targetAnchor + patchOffset;
            core::IntCoord sourceCoord = sourceAnchor + patchOffset;
            core::Vector3 sourceColor = source.get(sourceCoord);
            core::Vector3 targetColor = target.get(targetCoord);

            //add rgb sum squared difference
            const auto rDiff = sourceColor.x() - targetColor.x();
            const auto gDiff = sourceColor.y() - targetColor.y();
            const auto bDiff = sourceColor.z() - targetColor.z();

            // Some target pixels are more imperative to match than others - the weights
            // image gives us an indication of which target pixels are more "costly."
            auto contribution = rDiff*rDiff + gDiff*gDiff + bDiff*bDiff;
            contribution *= anchorWeights.get( targetCoord );

            sumCost+=contribution;
            if(sumCost>costNotToExceed) {
                return sumCost;
            }
        }
    }
    return sumCost;
}

} // utility
} // patchMatch
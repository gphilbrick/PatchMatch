#ifndef CORE_IMAGEUTILITY_H
#define CORE_IMAGEUTILITY_H

#include <Core/exceptions/runtimeerror.h>
#include <Core/image/imagetypes.h>
#include <Core/utility/vector3.h>
#include <Core/utility/vector4.h>
#include <Core/utility/intcoord.h>
#include <Core/utility/mathutility.h>
#include <Core/utility/twodarray.h>

namespace core {

template<typename T>
class TwoDArray;
class Vector2;

namespace imageUtility
{

/// 'structure' must be n by n, where n is an odd integer >= 1.
void dilate(
    const ImageBinary& structure, 
    const IntCoord& structureAnchor, 
    const ImageBinary& source, 
    ImageBinary& target );

/// Produce negative distances outside the object and positive distances inside
void getDistanceMapBidirectional(const ImageBinary& getDistTo, ImageScalar& storeResult);

/// 'newSize' must represent dimensions no larger than 'source''s dimensions.
/// 'newSize' must have dimensions > 0
template<typename T>
void downsample(const TwoDArray<T> &source, TwoDArray<T> &dest, const IntCoord& newSize)
{
    // Will a simple clone suffice?
    if(source.size()==newSize)
    {
        TwoDArray<T>::clone(source,dest);
        return;
    }

    int newWidth = newSize.x();
    int newHeight = newSize.y();

    if(dest.size()!=newSize)
    {
        dest.recreate(newSize);
    }

    for(int xNew=0; xNew<newWidth; xNew++)
    {
        for(int yNew=0; yNew<newHeight; yNew++)
        {
            //find the topleft corner of the smaller image pixel in the
            //space of the larger (original) image.
            double left = (((double)xNew)/((double)(newWidth)))*((double)(source.width()-1));
            double top = (((double)yNew)/((double)(newHeight)))*((double)(source.height()-1));
            double right = (((double)(xNew+1))/((double)(newWidth)))*((double)(source.width()-1));
            double bottom = (((double)(yNew+1))/((double)(newHeight)))*((double)(source.height()-1));

            T sum=T(); //explicit constructor necessary for primitive types.
            int numEncountered=0;

            for(int xOld = (int)ceil(left); xOld<(int)ceil(right); xOld++)
            {
                for(int yOld = (int)ceil(top); yOld<(int)ceil(bottom); yOld++)
                {
                    sum += source.get(xOld,yOld);
                    numEncountered++;
                }
            }

            dest.set(xNew,yNew,sum/((double)numEncountered));
        }
    }
}

/// 'newSize''s dimensions must be >0 and no larger than the dimensions of 'source'.
void downsampleBoolean(
    const ImageBinary& source, 
    ImageBinary& dest, 
    const IntCoord& newSize, 
    bool truesPrevail);

} // imageUtility
} // core

#endif // #include guard

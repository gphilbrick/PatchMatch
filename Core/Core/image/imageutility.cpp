#include <Core/image/imageutility.h>
#include <Core/utility/vector2.h>
#include <Core/utility/vector3.h>
#include <Core/utility/vector4.h>
#include <Core/utility/mathutility.h>

#include <algorithm>
#include <cmath>

namespace core {
namespace imageUtility {

void dilate(
    const ImageBinary& structure, 
    const IntCoord& structureAnchor, 
    const ImageBinary& source, 
    ImageBinary& target)
{
    //do not allow even-numbered dimensions
    if (structure.width() % 2 == 0 || structure.height() % 2 == 0) {
        THROW_RUNTIME("Even numbered dimensions not allowed!");
    }
    if (target.width() != source.width() || target.height() != source.height())
    {
        target.recreate(source.width(), source.height());
    }

    for (int x = 0; x < target.width(); x++)
    {
        for (int y = 0; y < target.height(); y++)
        {
            IntCoord here(x, y);
            bool mark = false;
            for (int sx = 0; sx < structure.width(); sx++)
            {
                for (int sy = 0; sy < structure.height(); sy++)
                {
                    if (structure.get(sx, sy) == false) continue;
                    IntCoord toCheck(sx, sy);
                    toCheck -= structureAnchor;
                    toCheck = here - toCheck;
                    if (source.isValidCoord(toCheck) == false) continue;
                    if (source.get(toCheck))
                    {
                        mark = true;
                        break;
                    }
                }
                if (mark) break;
            }

            if (mark)
            {
                target.set(x, y, true);
            }
        }
    }
}

void getDistanceMapBidirectional(
    const ImageBinary& getDistTo, 
    ImageScalar& storeResult )
{
    storeResult.recreate(getDistTo.width(),getDistTo.height());

    //first pass
    for (int y = 0; y < storeResult.height(); y++)
    {
        for (int x = 0; x < storeResult.width(); x++)
        {
            IntCoord coord(x,y);
            //is object?
            if (getDistTo.get(coord))
            {
                double above = y == 0 ? 0.0 : std::max(0.0,storeResult.get(x,y-1));
                double left = x == 0 ? 0.0 : std::max(0.0,storeResult.get(x-1,y));
                double setValue = std::min(above,left)+1.0;
                storeResult.set(coord, setValue);
            }
            else
            {
                double left = x == 0 ? (-storeResult.width()) : std::min(0.0,storeResult.get(x-1,y));
                double up = y == 0 ? (-storeResult.height()) : std::min(0.0,storeResult.get(x,y-1));
                double setValue = std::max(left,up)-1.0;
                storeResult.set(coord, setValue);

            }
        }
    }

    //second pass
    for (int y = storeResult.height() - 1; y >= 0; y--)
    {
        for (int x = storeResult.width() - 1; x >= 0; x--)
        {
            IntCoord coord(x,y);
            if (getDistTo.get(coord) == false)
            {
                double valHere = storeResult.get(coord);
                double right = x < storeResult.width() - 1 ? std::min(1.0,storeResult.get(x+1,y)) : valHere - 1.0;
                double down =  y < storeResult.height() - 1 ? std::min(1.0,storeResult.get(x,y+1)) : valHere - 1.0;
                storeResult.set(coord, std::max( std::max(right-1,down-1),storeResult.get(coord)));

            }
            else
            {
                double valHere = storeResult.get(coord);
                double right = x < storeResult.width() - 1 ? std::max(0.0,storeResult.get(x+1,y)) : 0.0;
                double down = y < storeResult.height() - 1 ? std::max(0.0,storeResult.get(x,y+1)) : 0.0;
                double setValue = std::min(valHere, std::min(right + 1, down + 1));
                storeResult.set(coord, setValue);
            }
        }
    }
}

void downsampleBoolean( 
    const ImageBinary& source, 
    ImageBinary& dest, 
    const IntCoord& newSize, 
    bool truesPrevail)
{
    //are we just cloning?
    if(source.size()==newSize)
    {
        ImageBinary::clone(source,dest);
        return;
    }

    int newWidth = newSize.x();
    int newHeight = newSize.y();

    if(dest.width()!=newWidth || dest.height()!=newHeight)
    {
        dest.recreate(newWidth,newHeight);
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

            bool result=!truesPrevail;

            for(int xOld = (int)ceil(left); xOld<(int)ceil(right); xOld++)
            {
                if(result==truesPrevail) break;
                for(int yOld = (int)ceil(top); yOld<(int)ceil(bottom); yOld++)
                {
                    if(source.get(xOld,yOld)==truesPrevail)
                    {
                        result=truesPrevail;
                        break;
                    }
                }
            }
            dest.set(xNew,yNew,result);
        }
    }
}

} // imageUtility
} // core

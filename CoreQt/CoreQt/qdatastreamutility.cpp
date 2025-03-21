#include <CoreQt/qdatastreamutility.h>

#include <Core/utility/twodarray.h>

namespace coreQt {

void writeToDataStream( QDataStream& outStream, const core::TwoDArray< bool >& image )
{
    outStream << image.width();
    outStream << image.height();
    for(int y=0; y<image.height(); y++)
    {
        for(int x=0; x<image.width(); x++)
        {
            outStream << image.get(x,y);
        }
    }
}

void readFromDataStream( QDataStream& inStream, core::TwoDArray<bool>& image)
{
    int width=0, height=0;
    inStream >> width >> height;
    image.recreate(width,height);
    for(int y=0; y<image.height(); y++)
    {
        for(int x=0; x<image.width(); x++)
        {
            bool val(false);
            inStream >> val;
            image.set(x, y, val);
        }
    }
}

} // coreQt

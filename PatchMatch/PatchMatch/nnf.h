#ifndef IEC_NNF_H
#define IEC_NNF_H

#include <Core/image/imagetypes.h>
#include <Core/utility/intcoord.h>
#include <Core/utility/vector2.h>
#include <Core/utility/twodarray.h>

#include <map>

namespace patchMatch {

/// Mapping from locations in a target image to 
/// locations (patch centers) in a source image.
class NNF
{
public:
    NNF();
    void set(const core::IntCoord& targetCoord,
        const core::IntCoord& sourceCoord,
        double matchCost);
    core::IntCoord getStoredSourceCoord( int targetX, int targetY ) const;
    core::IntCoord getStoredSourceCoord( const core::IntCoord& ) const;
    double getStoredMatchCost(int targetX, int targetY) const;

    /// Remove all stored data.  Set targetCoord => (0,0) for all targetCoord.  
    /// Set all _matchCosts to 0.
    void init(int width, int height);
    int width() const { return _sourceCoords.width(); }
    int height() const { return _sourceCoords.height(); }
private:
    core::TwoDArray< core::IntCoord > _sourceCoords;
    core::ImageScalar _matchCosts;
};

} // patchMatch

#endif // #include


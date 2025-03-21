#include <nnf.h>

namespace patchMatch {

NNF::NNF()
{
}

core::IntCoord NNF::getStoredSourceCoord( int targetX, int targetY ) const
{
    return _sourceCoords.get( targetX, targetY );
}

core::IntCoord NNF::getStoredSourceCoord( const core::IntCoord& coord ) const
{
    return _sourceCoords.get( coord.x(), coord.y() );
}

void NNF::init( int width, int height )
{
    _sourceCoords.recreate( width, height );
    _matchCosts.recreate( width, height );
}

double NNF::getStoredMatchCost( int targetX, int targetY ) const
{
    return _matchCosts.get( targetX, targetY );
}

void NNF::set(
    const core::IntCoord& targetCoord, 
    const core::IntCoord& sourceCoord, 
    double matchCost )
{
    _sourceCoords.set(targetCoord,sourceCoord);
    _matchCosts.set(targetCoord,matchCost);
}

} // patchMatch

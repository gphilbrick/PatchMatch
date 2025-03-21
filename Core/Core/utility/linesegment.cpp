#include <utility/linesegment.h>
#include <utility/mathutility.h>

namespace core {

namespace {

IntCoord intCoordForRasterizing( double fX, double fY )
{
    return IntCoord(
        static_cast< int >( std::floor( fX ) ),
        static_cast< int >( std::floor( fY ) ) );
}

IntCoord intCoordForRasterizing( const Vector2& v )
{
    return intCoordForRasterizing( v.x(), v.y() );
}

} // unnamed

LineSegment::LineSegment()
{
}

LineSegment::LineSegment( const Vector2& aArg, const Vector2& bArg ) : a( aArg ), b( bArg )
{
}

LineSegment LineSegment::reverse() const
{
    return { b, a };
}

Vector2 LineSegment::pos( double t ) const
{
    return mathUtility::lerp< Vector2 >( a, b, t );
}

void LineSegment::extend( double moveABackBy, double moveBAheadBy )
{
    Vector2 vec = b - a;
    const auto length = vec.length();
    vec.normalize();

    b = a + vec * ( length + moveBAheadBy );
    a = a - vec * moveABackBy;
}

Vector2 LineSegment::asVec() const
{
    return b - a;
}

double LineSegment::length() const
{
    return ( b - a ).length();
}

Vector2 LineSegment::midpoint() const
{
    return ( a + b ) * 0.5;
}

BoundingBoxd LineSegment::bounds() const
{
    return { a, b };
}

LineSegment LineSegment::fromPosAndDir( const Vector2& pos, const Vector2& dir )
{
    return LineSegment( pos, pos + dir );
}

double LineSegment::t( const Vector2& p ) const
{
    const auto len = length();
    if( mathUtility::closeEnoughToZero( len ) ) {
        return 0.;
    } else {
        return std::clamp( ( p - a ).length() / len, 0., 1. );
    }
}

} // core

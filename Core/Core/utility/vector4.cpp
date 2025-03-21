#include <Core/utility/vector4.h>

namespace core {

Vector4::Vector4(double x, double y, double z, double w)
{
    setX(x);
    setY(y);
    setZ(z);
    setW(w);
}

Vector4::Vector4(const Vector3& xyz, double w)
{
    setX(xyz.x());
    setY(xyz.y());
    setZ(xyz.z());
    setW(w);
}

void Vector4::setX(double x)
{
    _x = x;
}

void Vector4::setY(double y)
{
    _y=y;
}

void Vector4::setZ(double z)
{
    _z=z;
}

void Vector4::setW(double w)
{
    _w=w;
}

//a la glsl
Vector3 Vector4::xyz() const
{
    return Vector3(_x,_y,_z);
}

bool Vector4::operator == ( const Vector4& other ) const
{
    if( _x != other.x() ) {
        return false;
    }
    if( _y != other.y() ) {
        return false;
    }
    if( _z != other.z() ) {
        return false;
    }
    if( _w != other.w() ) {
        return false;
    }
    return true;
}

Vector4& Vector4::operator += (const Vector4& other)
{
    _x += other._x;
    _y += other._y;
    _z += other._z;
    _w += other._w;
    return *this;
}

Vector4& Vector4::operator -= (const Vector4& other)
{
    _x -= other._x;
    _y -= other._y;
    _z -= other._z;
    _w -= other._w;
    return *this;
}

Vector4& Vector4::operator *= (double fac)
{
    _x *= fac;
    _y *=fac;
    _z *=fac;
    _w *= fac;
    return *this;
}

Vector4& Vector4::operator /= (double fac)
{
    _x /= fac;
    _y /= fac;
    _z /= fac;
    _w /= fac;
    return *this;
}

const Vector4 Vector4::operator + (const Vector4& b) const
{
    Vector4 a = *this;
    a += b;
    return a;
}

const Vector4 Vector4::operator - (const Vector4& b) const
{
    Vector4 a = *this;
    a -= b;
    return a;
}

const Vector4 Vector4::operator * (const double fac) const
{
    Vector4 a= *this;
    a*=fac;
    return a;
}

const Vector4 Vector4::operator / (const double fac) const
{
    Vector4 a = *this;
    a/=fac;
    return a;
}

} // core

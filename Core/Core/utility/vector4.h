#ifndef CORE_VECTOR4_H
#define CORE_VECTOR4_H

#include <Core/utility/vector3.h>

namespace core {

class Vector4
{
public:
    explicit Vector4(double x=0, double y=0, double z=0, double w=0.0);
    Vector4(const Vector3& xyz, double w=1.0);
    double x() const { return _x; }
    double y() const { return _y; }
    double z() const { return _z; }
    double w() const { return _w; }
    double alpha() const { return _w; }

    void setX(double x);
    void setY(double y);
    void setZ(double z);
    void setW(double w);

    //a la glsl
    Vector3 xyz() const;

    Vector4& operator += (const Vector4&);
    Vector4& operator -= (const Vector4&);
    Vector4& operator *= (double fac);
    Vector4& operator /= (double fac);
    const Vector4 operator + (const Vector4&) const;
    const Vector4 operator - (const Vector4&) const;
    const Vector4 operator * (const double fac) const;
    const Vector4 operator / (const double fac) const;
    bool operator == ( const Vector4& ) const;

private:

    double _x;
    double _y;
    double _z;
    double _w;
};

} // core

#endif // #include guard

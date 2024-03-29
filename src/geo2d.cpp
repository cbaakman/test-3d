/* Copyright (C) 2015 Coos Baakman

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/


#include "geo2d.h"
#include <math.h>

# define M_PI 3.14159265358979323846

vec2::vec2(){}
vec2::vec2(float _x, float _y)
{
    x=_x; y=_y;
}
bool vec2::operator== (const vec2& other) const
{
    return(other.x==x && other.y==y);
}
vec2 vec2::operator* (const float& f) const
{
    vec2 v(f*x,f*y);
    return v;
}
vec2 vec2::operator/ (const float& f) const
{
    vec2 v(x/f,y/f);
    return v;
}
vec2 vec2::operator+ (const vec2& v2) const
{
    vec2 v(x+v2.x,y+v2.y);
    return v;
}
vec2 vec2::operator- (const vec2& v2) const
{
    vec2 v(x-v2.x,y-v2.y);
    return v;
}
void vec2::operator+= (const vec2& v2)
{
    x += v2.x;
    y += v2.y;
}
void vec2::operator-= (const vec2& v2)
{
    x -= v2.x;
    y -= v2.y;
}
void vec2::operator/= (const float v)
{
    x /= v;
    y /= v;
}
void vec2::operator*= (const float v)
{
    x *= v;
    y *= v;
}
vec2 vec2::operator- () const
{
    vec2 v(-x,-y);
    return v;
}
float vec2::Length2 () const
{
    return (x*x+y*y);
}
float vec2::Length () const
{
    return sqrt (Length2());
}
vec2 vec2::Unit () const
{
    const float l = Length();
    if (l > 0.0f)
        return (*this / l);
    else
        return *this;
}
vec2 vec2::Rotate (const float a) const
{
    vec2 r;
    const float
        c = cos(a),
        s = sin(a);

    r.x = c * x - s * y;
    r.y = s * x + c * y;

    return r;
}
float vec2::Angle () const
{
    return atan (y / x);
}
vec2 operator* (const float& f, const vec2& v)
{
    return (v * f);
}
float Distance2 (const vec2& v1, const vec2& v2)
{
    return (v2 - v1).Length2();
}
float Distance (const vec2& v1, const vec2& v2)
{
    return (v2 - v1).Length();
}
float Dot (const vec2 &v1, const vec2 &v2)
{
    return v1.x * v2.x + v1.y * v2.y;
}
float Angle (const vec2 &v1,const vec2 &v2)
{
    float a = acosf (Dot (v1.Unit (), v2.Unit ()));

    /*
        Return an angle between -PI and PI.
        Adding or subtracting 2PI always results
        in the same angle.
     */
    while (a > M_PI)   { a -= 2*M_PI; }
    while (a <= -M_PI) { a += 2*M_PI; }

    return a;
}
vec2 Projection (const vec2& v, const vec2& on_v)
{
    return on_v * Dot (v, on_v) / on_v.Length2 ();
}
vec2 LineIntersection (const vec2& a1, const vec2& a2, const vec2& b1, const vec2& b2)
{
     float d = (a1.x - a2.x) * (b1.y - b2.y) - (a1.y - a2.y) * (b1.x - b2.x),
           a = a1.x * a2.y - a1.y * a2.x,
           b = b1.x * b2.y - b1.y * b2.x;

     vec2 r ((a*(b1.x-b2.x)-b*(a1.x-a2.x))/d,(a*(b1.y-b2.y)-b*(a1.y-a2.y))/d);
     return r;
}
vec2 PointOnBezierCurve (float ti, const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3)
{
    float   invti = 1.0f - ti;

    return  invti*invti*invti*p0 + 3*invti*invti*ti*p1 + 3*invti*ti*ti*p2 + ti*ti*ti*p3;
}

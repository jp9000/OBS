/********************************************************************************
 Copyright (C) 2001-2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#pragma once

#include <math.h>
#include <float.h>

#pragma intrinsic(fabs)


#ifdef USE_SSE
    #include <xmmintrin.h>
#endif


#undef M_PI
#undef M_INFINITE
#undef RAD
#undef DEG
#undef EPSILON

//defines
#define M_PI            3.1415926535897932384626433832795f
#define RAD(val)        ((val)*0.0174532925199432957692369076848f)
#define DEG(val)        ((val)*57.295779513082320876798154814105f)
#define LARGE_EPSILON   1e-2f
#define EPSILON         1e-4f
#define TINY_EPSILON    1e-5f
#define M_INFINITE      3.4e38f



struct Vect;
struct Vect4;
struct Euler;
struct Quat;
struct AxisAngle;
struct Plane;
struct Matrix;
struct Float16;


union XLARGE_INT
{
    struct {DWORD lowVal; LONG highVal;};
    INT64 largeVal;
};

union XLARGE_UINT
{
    struct {DWORD lowUVal; DWORD highVal;};
    UINT64 largeVal;
};



/*=========================================================
    Optimized math functions
==========================================================*/

inline float fSinCosRecurse (float fRet, float fRad, const float fRad2, float fRadPow, float fFact, const int nPrec, int nCur)
{
    fRadPow *= -fRad2;
    fFact   *= nCur++;
    fFact   *= nCur++;
 
    fRet += fRadPow / fFact;
 
    if(nCur < nPrec)
        return fSinCosRecurse(fRet, fRad, fRad2, fRadPow, fFact, nPrec, nCur);
    else
        return fRet;
}

inline float fSin (float fRad, int nPrec=4)
{
    while(fRad >  M_PI) fRad -= (M_PI*2.0f);
    while(fRad < -M_PI) fRad += (M_PI*2.0f);

    const float fRad2 = (fRad*fRad);
    float fRet = fRad;
 
    return fSinCosRecurse(fRet, fRad, fRad2, fRad, 1.0f, (nPrec << 1) + 1, 2);
}

inline float fCos (float fRad, int nPrec=4)
{
    while(fRad >  M_PI) fRad -= (M_PI*2.0f);
    while(fRad < -M_PI) fRad += (M_PI*2.0f);

    const float fRad2 = (fRad*fRad);
    float fRet = 1.0f;
 
    return fSinCosRecurse(fRet, fRad, fRad2, 1.0f, 1.0f, (nPrec << 1) + 1, 1);
}

inline BOOL CloseFloat(float f1, float f2, float precision=LARGE_EPSILON)
{
    return fabsf(f1-f2) <= precision;
}


/*=========================================================
    2-D Vector
==========================================================*/

struct BASE_EXPORT Vect2
{
    union
    {
        struct
        {
            float x;
            float y;
        };
        float ptr[2];
    };


    inline Vect2()                                  {}
    inline Vect2(const Vect2 &v) : x(v.x), y(v.y)   {}
    explicit inline Vect2(float a) : x(a), y(a)     {}
    inline Vect2(float a, float b) : x(a), y(b)     {}
    explicit inline Vect2(const Vect &v);


    ////////////////////////
    //Arithmetic operators
    inline Vect2 operator+(const Vect2 &v) const    {return Vect2(x+v.x, y+v.y);}
    inline Vect2 operator-(const Vect2 &v) const    {return Vect2(x-v.x, y-v.y);}
    inline Vect2 operator*(const Vect2 &v) const    {return Vect2(x*v.x, y*v.y);}
    inline Vect2 operator/(const Vect2 &v) const    {return Vect2(x/v.x, y/v.y);}

    inline Vect2 operator+(float f) const           {return Vect2(  x+f,   y+f);}
    inline Vect2 operator-(float f) const           {return Vect2(  x-f,   y-f);}
    inline Vect2 operator*(float f) const           {return Vect2(  x*f,   y*f);}
    inline Vect2 operator/(float f) const           {return Vect2(  x/f,   y/f);}

    inline float operator|(const Vect2 &v) const    {return (x*v.x)+(y*v.y);}


    /////////////////////////
    //Assignment operators
    inline Vect2& operator+=(const Vect2 &v)        {return (*this)=(*this)+v;}
    inline Vect2& operator-=(const Vect2 &v)        {return (*this)=(*this)-v;}
    inline Vect2& operator*=(const Vect2 &v)        {return (*this)=(*this)*v;}
    inline Vect2& operator/=(const Vect2 &v)        {return (*this)=(*this)/v;}

    inline Vect2& operator+=(float f)               {return (*this)=(*this)+f;}
    inline Vect2& operator-=(float f)               {return (*this)=(*this)-f;}
    inline Vect2& operator*=(float f)               {return (*this)=(*this)*f;}
    inline Vect2& operator/=(float f)               {return (*this)=(*this)/f;}

    inline Vect2& operator=(const Vect &v);
    inline Vect2& operator=(const Vect2 &v)         {x=v.x; y=v.y; return *this;}
    inline Vect2& operator=(float val)              {x=val; y=val; return *this;}


    ////////////////////////
    //Boolean operators
    inline BOOL operator==(const Vect2 &v) const    {return (v.x==x)&&(v.y==y);}
    inline BOOL operator!=(const Vect2 &v) const    {return (v.x!=x)||(v.y!=y);}

    inline Vect2 operator-() const                  {return Vect2(-x, -y);}
    

    ////////////////////////
    //Type casting
    inline operator float*()                        {return ptr;}


    ////////////////////////
    //Other
    inline Vect2& Set(float a, float b)             {x=a; y=b; return *this;}

    inline float Len() const                        {return (float)sqrt((double)(x*x)+(y*y));}

    inline float Dist(const Vect2 &v) const
    {
        //only do this stuff once
        float a=(x-v.x),b=(y-v.y);
        return (float)sqrt((double)(a*a)+(b*b));
    }

    inline Vect2& ClampMin(float val)      {if(x < val) x = val; if(y < val) y = val; return *this;}
    inline Vect2& ClampMin(const Vect2 &v) {if(x < v.x) x = v.x; if(y < v.y) y = v.y; return *this;}

    inline Vect2& ClampMax(float val)      {if(x > val) x = val; if(y > val) y = val; return *this;}
    inline Vect2& ClampMax(const Vect2 &v) {if(x > v.x) x = v.x; if(y > v.y) y = v.y; return *this;}

    static inline Vect2 Max(const Vect2 &v,  float val)       {return Vect2(MAX(v.x, val),   MAX(v.y, val));}
    static inline Vect2 Max(const Vect2 &v1, const Vect2 &v2) {return Vect2(MAX(v1.x, v2.x), MAX(v1.y, v2.y));}

    static inline Vect2 Min(const Vect2 &v,  float val)       {return Vect2(MIN(v.x, val),   MIN(v.y, val));}
    static inline Vect2 Min(const Vect2 &v1, const Vect2 &v2) {return Vect2(MIN(v1.x, v2.x), MIN(v1.y, v2.y));}

    inline Vect2& Abs()                             {x=fabs(x); y=fabs(y); return *this;}
    inline Vect2 GetAbs() const                     {return Vect2(*this).Abs();}

    inline Vect2& Floor()                           {x=floorf(x); y=floorf(y); return *this;}
    inline Vect2 GetFloor() const                   {return Vect2(*this).Floor();}

    inline Vect2& Ceil()                            {x=ceilf(x); y=ceilf(y); return *this;}
    inline Vect2 GetCeil() const                    {return Vect2(*this).Ceil();}

    inline BOOL CloseTo(const Vect2 &v, float epsilon=EPSILON) const
    {
        return CloseFloat(x, v.x, epsilon) && CloseFloat(y, v.y, epsilon);
    }

    inline Vect2& Norm()
    {
        float len=Len();

        if(len != 0)
        {
            len = 1 / len;
            x *= len; y *= len;
        }

        return *this;
    }

    inline Vect2 GetNorm()
    {
        return Vect2(*this).Norm();
    }

    inline float Dot(const Vect2 &v) const
    {
        return v|(*this);
    }

    inline Vect2& Cross()
    {
        return (*this) = GetCross();
    }

    inline Vect2 GetCross() const
    {
        return Vect2(y, -x);
    }

    inline Vect2& SwapVals() {float newY = x; x = y; y = newY; return *this;}

    ////////////////////////
    // Serialization
    friend inline Serializer& operator<<(Serializer &s, Vect2 &v)
    {
        return s << v.x << v.y;
    }
};

typedef Vect2 UVCoord;

inline Vect2 operator/(float f, const Vect2 &v) {return Vect2(f/v.x, f/v.y);}
inline Vect2 operator*(float f, const Vect2 &v) {return Vect2(f*v.x, f*v.y);}


struct CompactVect
{
    float x, y, z;

    inline CompactVect() {}
    inline CompactVect(const Vect &v);

    inline CompactVect& operator=(const Vect &v);
};


/*=========================================================
    3-D Vector (128bit aligned)
==========================================================*/

struct BASE_EXPORT Vect
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
#ifdef USE_SSE
        __m128 m;
#endif
        float ptr[4];
    };

    explicit inline Vect(const Vect4 &v4);

#ifdef USE_SSE
    inline Vect()                                   {w = 0.0f;}
    explicit inline Vect(const Vect2 &v2)           {m = _mm_setr_ps(v2.x, v2.y, 0.0f, 0.0f);}
    inline Vect(const Vect& v)                      {m = v.m;}
    explicit inline Vect(float f)                   {m = _mm_load1_ps(&f); w = 0.0f;}
    inline Vect(float a, float b, float c)          {m = _mm_setr_ps(a, b, c, 0.0f);}
    inline Vect(const CompactVect &v)               {m = _mm_setr_ps(v.x, v.y, v.z, 0.0f);}
    inline Vect(float a, float b, float c, float d) {m = _mm_setr_ps(a, b, c, d);}
    explicit inline Vect(__m128 val) : m(val)       {}

    ////////////////////////
    //Arithmetic operators
    inline Vect operator+(const Vect &v) const      {return Vect(_mm_add_ps(m, v.m));}
    inline Vect operator-(const Vect &v) const      {return Vect(_mm_sub_ps(m, v.m));}
    inline Vect operator*(const Vect &v) const      {return Vect(_mm_mul_ps(m, v.m));}
    inline Vect operator/(const Vect &v) const      {__m128 tmp = _mm_div_ps(m, v.m); tmp.m128_f32[3] = 0.0f; return Vect(tmp);}

    inline Vect operator+(float f) const            {Vect val(_mm_add_ps(m, _mm_set1_ps(f))); val.w = 0.0f; return val;}
    inline Vect operator-(float f) const            {Vect val(_mm_sub_ps(m, _mm_set1_ps(f))); val.w = 0.0f; return val;}
    inline Vect operator*(float f) const            {return Vect(_mm_mul_ps(m, _mm_set1_ps(f)));}
    inline Vect operator/(float f) const            {__m128 tmp = _mm_div_ps(m, _mm_set1_ps(f)); tmp.m128_f32[3] = 0.0f; return Vect(tmp);}

    inline float operator|(const Vect &v) const     {return Dot(v);}
    inline Vect  operator^(const Vect &v) const     {return Cross(v);}

    inline Vect operator-() const                   {return Vect(-x, -y, -z);}


    /////////////////////////
    //Assignment operators
    inline Vect& operator+=(const Vect &v)          {m = _mm_add_ps(m, v.m); return *this;}
    inline Vect& operator-=(const Vect &v)          {m = _mm_sub_ps(m, v.m); return *this;}
    inline Vect& operator*=(const Vect &v)          {m = _mm_mul_ps(m, v.m); return *this;}
    inline Vect& operator/=(const Vect &v)          {m = _mm_div_ps(m, v.m); w=0.0f; return *this;}

    inline Vect& operator+=(float f)                {m = _mm_add_ps(m, _mm_set1_ps(f)); w=0.0f; return *this;}
    inline Vect& operator-=(float f)                {m = _mm_sub_ps(m, _mm_set1_ps(f)); w=0.0f; return *this;}
    inline Vect& operator*=(float f)                {m = _mm_mul_ps(m, _mm_set1_ps(f)); return *this;}
    inline Vect& operator/=(float f)                {m = _mm_div_ps(m, _mm_set1_ps(f)); w=0.0f; return *this;}

    inline Vect& operator=(const Vect &v)           {
        _mm_store_ps(ptr, v.m);
        m = v.m;
        return *this;
    }
    inline Vect& operator=(const Vect2 &v2)         {x = v2.x; y = v2.y; z = w = 0.0f; return *this;}
    inline Vect& operator=(float f)                 {m = _mm_set1_ps(f); w=0.0f; return *this;}


    ////////////////////////
    //Boolean operators
    inline BOOL operator==(const Vect &v) const     {return CloseTo(v);}
    inline BOOL operator!=(const Vect &v) const     {return !CloseTo(v);}


    ////////////////////////
    //Other
    inline Vect& Set(float a, float b)              {m = _mm_setr_ps(a, b, 0.0f, 0.0f); return *this;}
    inline Vect& Set(float a, float b, float c)     {m = _mm_setr_ps(a, b, c, 0.0f); return *this;}

    //as far as I know an SSE dot is not really any better than an FPU dot
    inline float Dot(const Vect &v) const
    {
        __m128 mulVal = _mm_mul_ps(m, v.m); //mul = xyzw*v.xyzw
        __m128 addVal = _mm_add_ps(_mm_movehl_ps(mulVal, mulVal), mulVal); //add = mul.yzyz+mul.xyzw
        addVal = _mm_add_ps(_mm_shuffle_ps(addVal, addVal, 0x55), addVal); //add = add.yyyy+add.xyxw
        return addVal.m128_f32[0]; //return add.x
    }

    //same with cross.
    inline Vect Cross(const Vect &v) const
    {
        return Vect(_mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(m, m, _MM_SHUFFLE(3, 0, 2, 1)), _mm_shuffle_ps(v.m, v.m, _MM_SHUFFLE(3, 1, 0, 2))),
                               _mm_mul_ps(_mm_shuffle_ps(m, m, _MM_SHUFFLE(3, 1, 0, 2)), _mm_shuffle_ps(v.m, v.m, _MM_SHUFFLE(3, 0, 2, 1)))));
        
    }

    inline float Len() const
    {
        float dotVal = Dot(*this);
        return (dotVal > 0.0f) ? sqrtf(dotVal) : 0.0f;
    }

    inline float Dist(const Vect &v) const
    {
        Vect subVal = *this - v;
        float dotVal = subVal.Dot(subVal);
        return (dotVal > 0.0f) ? sqrtf(dotVal) : 0.0f;
    }

    inline Vect& Norm()
    {
        float dotVal = Dot(*this);
        if(dotVal > 0.0f)
            m = _mm_mul_ps(m, _mm_set1_ps(1.0f/sqrtf(dotVal)));
        else 
            m = _mm_setzero_ps();

        return *this;
    }

    inline BOOL CloseTo(const Vect &v, float epsilon=EPSILON) const
    {
        __m128 subVal = _mm_sub_ps(m, v.m);
        return (fabs(subVal.m128_f32[0]) < epsilon) &&
               (fabs(subVal.m128_f32[1]) < epsilon) &&
               (fabs(subVal.m128_f32[2]) < epsilon);
    }

    inline Vect& SetZero() {m = _mm_setzero_ps(); return *this;}

    inline Vect& ClampMin(float val) {m = _mm_max_ps(m, _mm_set1_ps(val)); w = 0.0f; return *this;}
    inline Vect& ClampMin(const Vect &v) {m = _mm_max_ps(m, v.m); w = 0.0f; return *this;}

    inline Vect& ClampMax(float val) {m = _mm_min_ps(m, _mm_set1_ps(val)); w = 0.0f; return *this;}
    inline Vect& ClampMax(const Vect &v) {m = _mm_min_ps(m, v.m); w = 0.0f; return *this;}

    static inline Vect Max(const Vect &v,  float val)       {Vect out(_mm_max_ps(v.m, _mm_set1_ps(val))); out.w = 0.0f; return out;}
    static inline Vect Max(const Vect &v1, const Vect &v2)  {Vect out(_mm_max_ps(v1.m, v2.m)); out.w = 0.0f; return out;}

    static inline Vect Min(const Vect &v,  float val)       {Vect out(_mm_min_ps(v.m, _mm_set1_ps(val))); out.w = 0.0f; return out;}
    static inline Vect Min(const Vect &v1, const Vect &v2)  {Vect out(_mm_min_ps(v1.m, v2.m)); out.w = 0.0f; return out;}
#else
    inline Vect()                                                               {w=0.0f;}
    explicit inline Vect(float f) : x(f), y(f), z(f), w(0.0f)                   {}
    inline Vect(float a, float b, float c) : x(a), y(b), z(c), w(0.0f)          {}
    inline Vect(float a, float b, float c, float d): x(a), y(b), z(c), w(d)     {} 
    inline Vect(const CompactVect &v)                                           {x = v.x; y = v.y; z = v.z; w = 0.0f;}
    explicit inline Vect(const Vect2 &v2) : x(v2.x), y(v2.y), z(0.0f), w(0.0f)  {}


    ////////////////////////
    //Arithmetic operators
    inline Vect operator+(const Vect &v) const      {return Vect(x+v.x, y+v.y, z+v.z);}
    inline Vect operator-(const Vect &v) const      {return Vect(x-v.x, y-v.y, z-v.z);}
    inline Vect operator*(const Vect &v) const      {return Vect(x*v.x, y*v.y, z*v.z);}
    inline Vect operator/(const Vect &v) const      {return Vect(x/v.x, y/v.y, z/v.z);}

    inline Vect operator+(float f) const            {return Vect(  x+f,   y+f,   z+f);}
    inline Vect operator-(float f) const            {return Vect(  x-f,   y-f,   z-f);}
    inline Vect operator*(float f) const            {return Vect(  x*f,   y*f,   z*f);}
    inline Vect operator/(float f) const
    {
        f = 1 / f;
        return Vect(x*f, y*f, z*f);
    }

    inline float operator|(const Vect &v) const     {return (x*v.x)+(y*v.y)+(z*v.z);}
    inline Vect  operator^(const Vect &v) const     {return Vect((y*v.z)-(z*v.y), (z*v.x)-(x*v.z), (x*v.y)-(y*v.x));}


    /////////////////////////
    //Assignment operators
    inline Vect& operator+=(const Vect &v)          {return (*this)=(*this)+v;}
    inline Vect& operator-=(const Vect &v)          {return (*this)=(*this)-v;}
    inline Vect& operator*=(const Vect &v)          {return (*this)=(*this)*v;}
    inline Vect& operator/=(const Vect &v)          {return (*this)=(*this)/v;}

    inline Vect& operator+=(float f)                {return (*this)=(*this)+f;}
    inline Vect& operator-=(float f)                {return (*this)=(*this)-f;}
    inline Vect& operator*=(float f)                {return (*this)=(*this)*f;}
    inline Vect& operator/=(float f)                {return (*this)=(*this)/f;}

    inline Vect& operator^=(const Vect &v)          {return (*this)=(*this)^v;}

    inline Vect& operator=(const Vect2 &v2)         {x = v2.x; y = v2.y; z = w = 0.0f; return *this;}
    inline Vect& operator=(const Vect &v)           {x=v.x; y=v.y; z=v.z; return *this;}
    inline Vect& operator=(float f)                 {x=f; y=f; z=f; return *this;}

    ////////////////////////
    //Boolean operators
    inline BOOL operator==(const Vect &v) const     {return CloseTo(v);}
    inline BOOL operator!=(const Vect &v) const     {return !CloseTo(v);}

    inline Vect operator-() const                   {return Vect(-x, -y, -z);}


    ////////////////////////
    //Other
    inline Vect& Set(float a, float b) {x=a; y=b; z=0.0f; return *this;}
    inline Vect& Set(float a, float b, float c) {x=a; y=b; z=c; return *this;}

    inline float Len() const                        {return (float)sqrt((double)(x*x)+(y*y)+(z*z));}

    inline float Dist(const Vect &v) const
    {
        //only do this stuff once
        float a=(x-v.x),b=(y-v.y),c=(z-v.z);
        return (float)sqrt((double)(a*a)+(b*b)+(c*c));
    }

    inline Vect& Norm()
    {
        float len=Len();

        if(len != 0.0f)
        {
            len = 1.0f / len;

            x *= len; y *= len; z *= len;
        }

        return *this;
    }

    inline float Dot(const Vect &v)   const         {return (*this)|v;}
    inline Vect  Cross(const Vect &v) const         {return (*this)^v;}

    inline BOOL CloseTo(const Vect &v, float epsilon=EPSILON) const
    {
        return CloseFloat(x, v.x, epsilon) && CloseFloat(y, v.y, epsilon) && CloseFloat(z, v.z, epsilon);
    }

    inline Vect& SetZero() {x = y = z = w = 0.0f; return *this;}

    inline Vect& ClampMin(float val)     {if(x < val) x = val; if(y < val) y = val; if(z < val) z = val; return *this;}
    inline Vect& ClampMin(const Vect &v) {if(x < v.x) x = v.x; if(y < v.y) y = v.y; if(z < v.z) z = v.z; return *this;}

    inline Vect& ClampMax(float val)     {if(x > val) x = val; if(y > val) y = val; if(z > val) z = val; return *this;}
    inline Vect& ClampMax(const Vect &v) {if(x > v.x) x = v.x; if(y > v.y) y = v.y; if(z > v.z) z = v.z; return *this;}

    static inline Vect Max(const Vect &v,  float val)       {return Vect(MAX(v.x, val),   MAX(v.y, val),   MAX(v.z, val));}
    static inline Vect Max(const Vect &v1, const Vect &v2)  {return Vect(MAX(v1.x, v2.x), MAX(v1.y, v2.y), MAX(v1.z, v2.z));}

    static inline Vect Min(const Vect &v,  float val)       {return Vect(MIN(v.x, val),   MIN(v.y, val),   MIN(v.z, val));}
    static inline Vect Min(const Vect &v1, const Vect &v2)  {return Vect(MIN(v1.x, v2.x), MIN(v1.y, v2.y), MIN(v1.z, v2.z));}
#endif

    inline Vect& Abs()                              {x=fabsf(x); y=fabsf(y); z=fabsf(z); return *this;}

    inline Vect& Floor()                            {x=floorf(x); y=floorf(y); z=floorf(z); return *this;}
    inline Vect GetFloor() const                    {return Vect(*this).Floor();}

    inline Vect& Ceil()                             {x=ceilf(x); y=ceilf(y); z=ceilf(z); return *this;}
    inline Vect GetCeil() const                     {return Vect(*this).Ceil();}

    static inline Vect Zero()                       {return Vect().SetZero();}

    inline Vect GetClampMax(float f)                {return Vect(*this).ClampMax(f);}
    inline Vect GetClampMin(float f)                {return Vect(*this).ClampMin(f);}

    float DistFromPlane(const Plane &p) const;

    inline Vect& TransformPoint(const Matrix &m);
    inline Vect& TransformVector(const Matrix &m);

    inline Vect GetTransformedPoint(const Matrix &m) const  {return Vect(*this).TransformPoint(m);}
    inline Vect GetTransformedVector(const Matrix &m) const {return Vect(*this).TransformVector(m);}

    inline void MirrorByPlane(const Plane &p);
    inline void MirrorByVector(const Vect &v);

    inline operator float*()                        {return ptr;}

    inline Vect GetAbs() const                      {return Vect(*this).Abs();}

    inline Vect GetNorm() const
    {
        return Vect(*this).Norm();
    }

    inline Vect &MakeFromRGB(DWORD dwRGB)
    {
        x = RGB_Rf(dwRGB);
        y = RGB_Gf(dwRGB);
        z = RGB_Bf(dwRGB);
        w = 0.0f;

        return *this;
    }

    inline void GetNormalByteValue()
    {
        Norm();
        x = ((x + 1.0f) * 0.5f) * 255.0f;
        y = ((y + 1.0f) * 0.5f) * 255.0f;
        z = ((z + 1.0f) * 0.5f) * 255.0f;
    }

    inline void GetNormalWordValue()
    {
        Norm();
        x = ((x + 1.0f) * 0.5f) * 65535.0f;
        y = ((y + 1.0f) * 0.5f) * 65535.0f;
        z = ((z + 1.0f) * 0.5f) * 65535.0f;
    }

    inline DWORD GetRGB() const {return Vect_to_RGB(*this)|0xFF000000;}

    Vect GetInterpolationTangent(const Vect &prev, const Vect &next) const;

    ////////////////////////
    // Serialization
    friend inline Serializer& operator<<(Serializer &s, Vect &v)
    {
        s.Serialize(v.ptr, sizeof(float)*3);
        return s;
    }

    static inline Serializer& SerializeList(Serializer &s, List<Vect> &list)
    {
        UINT num;
        if(s.IsLoading())
        {
            s << num;
            list.SetSize(num);
        }
        else
        {
            num = list.Num();
            s << num;
        }

        for(unsigned int i=0; i<num; i++)
            s << list[i];

        return s;
    }

    static inline Serializer& SerializeArray(Serializer &s, Vect *array, UINT num)
    {
        for(unsigned int i=0; i<num; i++)
            s << array[i];

        return s;
    }
};

typedef Vect Color3, UVWCoord, Vect3;


/*=========================================================
    4-D Vector
==========================================================*/

struct BASE_EXPORT Vect4
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
#ifdef USE_SSE
        __m128 m;
#endif
        float ptr[4];
    };


    inline Vect4()                                                                  {}

#ifdef USE_SSE
    inline Vect4(float a, float b, float c, float d)                                {m = _mm_setr_ps(a, b, c, d);}
    explicit inline Vect4(const Vect2 &v2)                                          {m = _mm_setr_ps(v2.x, v2.y, 0.0f, 1.0f);}
    explicit inline Vect4(const Vect &v)                                            {m = v.m; w = 1.0f;}
    inline Vect4(const Vect &v, float wIn)                                          {m = v.m; w = wIn;}
    explicit inline Vect4(__m128 m2) : m(m2)                                        {}


    ////////////////////////
    //Arithmetic operators
    inline Vect4 operator+(const Vect4 &v) const      {return Vect4(_mm_add_ps(m, v.m));}
    inline Vect4 operator-(const Vect4 &v) const      {return Vect4(_mm_sub_ps(m, v.m));}
    inline Vect4 operator*(const Vect4 &v) const      {return Vect4(_mm_mul_ps(m, v.m));}
    inline Vect4 operator/(const Vect4 &v) const      {return Vect4(_mm_div_ps(m, v.m));}

    inline Vect4 operator+(float f) const             {return Vect4(_mm_add_ps(m, _mm_set1_ps(f)));}
    inline Vect4 operator-(float f) const             {return Vect4(_mm_sub_ps(m, _mm_set1_ps(f)));}
    inline Vect4 operator*(float f) const             {return Vect4(_mm_mul_ps(m, _mm_set1_ps(f)));}
    inline Vect4 operator/(float f) const             {return Vect4(_mm_div_ps(m, _mm_set1_ps(f)));}

    inline float operator|(const Vect4 &v) const
    {
        __m128 mulVal = _mm_mul_ps(m, v.m); //mul = xyzw*v.xyzw
        __m128 addVal = _mm_add_ps(_mm_movehl_ps(mulVal, mulVal), mulVal); //add = mul.yzyz+mul.xyzw
        addVal = _mm_add_ps(_mm_shuffle_ps(addVal, addVal, 0x55), addVal); //add = add.yyyy+add.xyxw
        return addVal.m128_f32[0]; //return add.x
    }

    /////////////////////////
    //Assignment operators
    inline Vect4& operator+=(const Vect4 &v)          {m = _mm_add_ps(m, v.m); return *this;}
    inline Vect4& operator-=(const Vect4 &v)          {m = _mm_sub_ps(m, v.m); return *this;}
    inline Vect4& operator*=(const Vect4 &v)          {m = _mm_mul_ps(m, v.m); return *this;}
    inline Vect4& operator/=(const Vect4 &v)          {m = _mm_div_ps(m, v.m); return *this;}

    inline Vect4& operator+=(float f)                 {m = _mm_add_ps(m, _mm_set1_ps(f)); return *this;}
    inline Vect4& operator-=(float f)                 {m = _mm_sub_ps(m, _mm_set1_ps(f)); return *this;}
    inline Vect4& operator*=(float f)                 {m = _mm_mul_ps(m, _mm_set1_ps(f)); return *this;}
    inline Vect4& operator/=(float f)                 {m = _mm_div_ps(m, _mm_set1_ps(f)); return *this;}

    inline Vect4& operator=(const Vect4 &v)           {m=v.m; return *this;}
    inline Vect4& operator=(float f)                  {m = _mm_set1_ps(f); return *this;}

    ////////////////////////
    //Boolean operators
    inline BOOL operator==(const Vect4 &v) const      {return CloseTo(v);}
    inline BOOL operator!=(const Vect4 &v) const      {return !CloseTo(v);}


    ////////////////////////
    //Other
    inline Vect4& Set(float a, float b) {x=a; y=b; z=0.0f; w=1.0f; return *this;}
    inline Vect4& Set(float a, float b, float c) {x=a; y=b; z=c; w=1.0f; return *this;}
    inline Vect4& Set(float a, float b, float c, float d) {x=a; y=b; z=c; w=d; return *this;}

    inline BOOL CloseTo(const Vect4 &v, float epsilon=EPSILON) const
    {
        Vect4 subVal = (*this - v);
        return (fabsf(subVal.x) < epsilon) &&
               (fabsf(subVal.y) < epsilon) &&
               (fabsf(subVal.z) < epsilon) &&
               (fabsf(subVal.w) < epsilon);
    }

    inline Vect4& SetZero() {m = _mm_setzero_ps(); return *this;}
#else
    inline Vect4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d)   {}
    explicit inline Vect4(const Vect2 &v2) : x(v2.x), y(v2.y), z(0.0f), w(1.0f) {}
    explicit inline Vect4(const Vect &v)   : x(v.x),  y(v.y),  z(v.z),  w(1.0f) {}
    inline Vect4(const Vect &v, float w) : x(v.x),  y(v.y),  z(v.z),  w(w)      {}


    ////////////////////////
    //Arithmetic operators
    inline Vect4 operator+(const Vect4 &v) const      {return Vect4(x+v.x, y+v.y, z+v.z, w+v.w);}
    inline Vect4 operator-(const Vect4 &v) const      {return Vect4(x-v.x, y-v.y, z-v.z, w-v.w);}
    inline Vect4 operator*(const Vect4 &v) const      {return Vect4(x*v.x, y*v.y, z*v.z, w*v.w);}
    inline Vect4 operator/(const Vect4 &v) const      {return Vect4(x/v.x, y/v.y, z/v.z, w/v.w);}

    inline Vect4 operator+(float f) const             {return Vect4(  x+f,   y+f,   z+f,   w+f);}
    inline Vect4 operator-(float f) const             {return Vect4(  x-f,   y-f,   z-f,   w-f);}
    inline Vect4 operator*(float f) const             {return Vect4(  x*f,   y*f,   z*f,   w*f);}
    inline Vect4 operator/(float f) const
    {
        f = 1 / f;
        return Vect4(x*f, y*f, z*f, w*f);
    }

    inline float operator|(const Vect4 &v) const      {return (x*v.x)+(y*v.y)+(z*v.z)+(w*v.w);}

    /////////////////////////
    //Assignment operators
    inline Vect4& operator+=(const Vect4 &v)          {return (*this)=(*this)+v;}
    inline Vect4& operator-=(const Vect4 &v)          {return (*this)=(*this)-v;}
    inline Vect4& operator*=(const Vect4 &v)          {return (*this)=(*this)*v;}
    inline Vect4& operator/=(const Vect4 &v)          {return (*this)=(*this)/v;}

    inline Vect4& operator+=(float f)                 {return (*this)=(*this)+f;}
    inline Vect4& operator-=(float f)                 {return (*this)=(*this)-f;}
    inline Vect4& operator*=(float f)                 {return (*this)=(*this)*f;}
    inline Vect4& operator/=(float f)                 {return (*this)=(*this)/f;}

    inline Vect4& operator=(const Vect4 &v)           {x=v.x; y=v.y; z=v.z; w=v.w; return *this;}
    inline Vect4& operator=(float f)                  {x=f; y=f; z=f; w=f; return *this;}

    ////////////////////////
    //Boolean operators
    inline BOOL operator==(const Vect4 &v) const      {return (v.x==x)&&(v.y==y)&&(v.z==z);}
    inline BOOL operator!=(const Vect4 &v) const      {return (v.x!=x)||(v.y!=y)||(v.z!=z);}


    ////////////////////////
    //Other
    inline Vect4& Set(float a, float b) {x=a; y=b; z=0.0f; w=1.0f; return *this;}
    inline Vect4& Set(float a, float b, float c) {x=a; y=b; z=c; w=1.0f; return *this;}
    inline Vect4& Set(float a, float b, float c, float d) {x=a; y=b; z=c; w=d; return *this;}

    inline BOOL CloseTo(const Vect4 &v, float epsilon=EPSILON) const
    {
        return CloseFloat(x, v.x, epsilon) && CloseFloat(y, v.y, epsilon) && CloseFloat(z, v.z, epsilon) && CloseFloat(w, v.w, epsilon);
    }

    inline Vect4& SetZero() {x = y = z = w = 0.0f; return *this;}

#endif

    inline Vect4 operator-() const                    {return Vect4(-x, -y, -z, -w);}

    inline Vect4& Abs()                               {x=fabs(x); y=fabs(y); z=fabs(z); w=fabs(w); return *this;}
    inline Vect4 GetAbs()                             {Vect4 temp(*this); return temp.Abs();}

    inline Vect4& Floor()                             {x=floorf(x); y=floorf(y); z=floorf(z); w=floorf(w); return *this;}
    inline Vect4 GetFloor() const                     {return Vect4(*this).Floor();}

    inline Vect4& Ceil()                              {x=ceilf(x); y=ceilf(y); z=ceilf(z); w=ceilf(w); return *this;}
    inline Vect4 GetCeil() const                      {return Vect4(*this).Ceil();}

    static inline Vect4 Zero()                        {return Vect4().SetZero();}

    inline Vect4& NormXYZ()
    {
        float multiplyVal = 1.0f/sqrtf((x*x)+(y*y)+(z*z));

        x *= multiplyVal;
        y *= multiplyVal;
        z *= multiplyVal;

        return *this;
    }

    inline Vect4 GetNormXYZ()                         {return Vect4(*this).NormXYZ();}

    inline Vect4& NormFull()
    {
        float multiplyVal = 1.0f/sqrtf((x*x)+(y*y)+(z*z)+(w*w));

        x *= multiplyVal;
        y *= multiplyVal;
        z *= multiplyVal;
        w *= multiplyVal;

        return *this;
    }

    inline Vect4 GetNormFull()                        {return Vect4(*this).NormFull();}

    //...don't ask.  it's just a name I sort of invented out of thin air
    inline Vect4& SquarifyColor()
    {
        register int highest = (x>y) ? 0 : 1;
        if(z > ptr[highest])
            highest = 2;

        float offset = 1.0f-ptr[highest];

        x += offset;
        y += offset;
        z += offset;

        return (*this).ClampColor();
    }

    inline Vect4 GetSquarifiedColor()   {return Vect4(*this).SquarifyColor();}

    inline Vect4& ClampColor()
    {
        for(register int i=0; i<4; i++)
        {
            if(ptr[i] < 0.0f)
                ptr[i] = 0.0f;
            else if(ptr[i] > 1.0f)
                ptr[i] = 1.0f;
        }

        return *this;
    }

    inline Vect4 GetClampedColor()      {return Vect4(*this).ClampColor();}

    inline Vect4& DesaturateColor()
    {
        float average = (x+y+z)*0.3333333333f;

        x = y = z = average;

        return *this;
    }

    inline Vect4 GetDesaturatedColor()    {return Vect4(*this).DesaturateColor();}

    inline Vect4& MakeFromRGBA(DWORD dwRGBA)
    {
        x = RGB_Rf(dwRGBA);
        y = RGB_Gf(dwRGBA);
        z = RGB_Bf(dwRGBA);
        w = RGB_Af(dwRGBA);
        return *this;
    }

    inline DWORD GetRGBA() const {return Vect4_to_RGBA(*this);}

    ////////////////////////
    // Serialization
    friend inline Serializer& operator<<(Serializer &s, Vect4 &v)
    {
        return s << v.x << v.y << v.z << v.w;
    }
};
typedef Vect4 Color4;


/*=========================================================
    Quaternion
==========================================================*/

struct BASE_EXPORT Quat
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
#ifdef USE_SSE
        __m128 m;
#endif
        float ptr[4];
    };

    inline Quat()                                                               {}
    explicit inline Quat(const AxisAngle &aa)                                   {MakeFromAxisAngle(aa);}
    Quat(const Euler &e);

#ifdef USE_SSE
    inline Quat(float a, float b, float c, float d)                             {m = _mm_setr_ps(a, b, c, d);}
    explicit inline Quat(const Vect &v)            : m(v.m), w(1.0f)            {}
    inline Quat(const Vect &v, float wIn) : m(v.m), w(wIn)                      {}
    explicit inline Quat(__m128 m2) : m(m2)                                     {}


    /////////////////////////
    //Arithmetic operators
    inline Quat operator+(const Quat &v) const      {return Quat(_mm_add_ps(m, v.m));}
    inline Quat operator-(const Quat &v) const      {return Quat(_mm_sub_ps(m, v.m));}

    inline Quat operator+(float f) const            {return Quat(_mm_add_ps(m, _mm_set1_ps(f)));}
    inline Quat operator-(float f) const            {return Quat(_mm_sub_ps(m, _mm_set1_ps(f)));}
    inline Quat operator*(float f) const            {return Quat(_mm_mul_ps(m, _mm_set1_ps(f)));}
    inline Quat operator/(float f) const            {return Quat(_mm_div_ps(m, _mm_set1_ps(f)));}

    inline float operator|(const Quat &v) const
    {
        __m128 mulVal = _mm_mul_ps(m, v.m); //mul = xyzw*v.xyzw
        __m128 addVal = _mm_add_ps(_mm_movehl_ps(mulVal, mulVal), mulVal); //add = mul.yzyz+mul.xyzw
        addVal = _mm_add_ps(_mm_shuffle_ps(addVal, addVal, 0x55), addVal); //add = add.yyyy+add.xyxw
        return addVal.m128_f32[0]; //return add.x
    }

    inline Quat  operator^(const Quat &q) const
    {
        return Quat(
            (y*q.z)-(z*q.y)+(w*q.x)+(x*q.w),
            (z*q.x)-(x*q.z)+(w*q.y)+(y*q.w),
            (x*q.y)-(y*q.x)+(w*q.z)+(z*q.w),
            (w*q.w)-(x*q.x)-(y*q.y)-(z*q.z));
    }


    /////////////////////////
    //Transform operators
    Quat operator*(const Quat &q) const;


    /////////////////////////
    //Assignment operators
    inline Quat& operator+=(const Quat &v)          {m = _mm_add_ps(m, v.m); return *this;}
    inline Quat& operator-=(const Quat &v)          {m = _mm_sub_ps(m, v.m); return *this;}
    Quat& operator*=(const Quat &q);
    inline Quat& operator*=(const Euler &e);

    inline Quat& operator+=(float f)                {m = _mm_add_ps(m, _mm_set1_ps(f)); return *this;}
    inline Quat& operator-=(float f)                {m = _mm_sub_ps(m, _mm_set1_ps(f)); return *this;}
    inline Quat& operator*=(float f)                {m = _mm_mul_ps(m, _mm_set1_ps(f)); return *this;}
    inline Quat& operator/=(float f)                {m = _mm_div_ps(m, _mm_set1_ps(f)); return *this;}

    inline Quat& operator^=(const Quat &q)          {return (*this)=(*this)^q;}

    inline Quat& operator=(const Quat &q)           {m = q.m; return *this;}
    inline Quat& operator=(const Vect &v)           {m = v.m; w=1.0f; return *this;}
    inline Quat& operator=(const AxisAngle &aa)     {return MakeFromAxisAngle(aa);}
    Quat& operator=(const Euler &e);


    /////////////////////////
    //Boolean operators
    inline BOOL operator==(const Quat &q) const     {return (q.x==x)&&(q.y==y)&&(q.z==z)&&(q.w==w);}
    inline BOOL operator!=(const Quat &q) const     {return (q.x!=x)||(q.y!=y)||(q.z!=z)||(q.w!=w);}

    /////////////////////////
    //Other
    inline Quat& Set(float a, float b, float c, float d) {m = _mm_setr_ps(a, b, c, d); return *this;}

    inline Quat& SetIdentity()                      {m = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f); return *this;}

    inline float Len() const
    {
        return sqrtf(*this | *this);
    }

    inline float Dist(const Quat &v) const
    {
        Quat subVal = *this - v;
        return sqrtf(subVal | subVal);
    }

    inline Quat& Norm()
    {
        m = _mm_mul_ps(m, _mm_set1_ps(1.0f/sqrtf(*this | *this)));
        return *this;
    }

    inline BOOL CloseTo(const Quat &v, float epsilon=EPSILON) const
    {
        Quat subVal = (*this - v);
        return (fabs(subVal.x) < epsilon) &&
               (fabs(subVal.y) < epsilon) &&
               (fabs(subVal.z) < epsilon) &&
               (fabs(subVal.w) < epsilon);
    }
#else
    inline Quat(float a, float b, float c, float d) : x(a), y(b), z(c), w(d)    {}
    inline Quat(const Vect &v, float d) : x(v.x), y(v.y), z(v.z), w(d)          {}
    explicit inline Quat(const Vect &v) : x(v.x), y(v.y), z(v.z), w(1.0f)       {}


    /////////////////////////
    //Arithmetic operators
    inline Quat operator+(const Quat &q) const      {return Quat(x+q.x, y+q.y, z+q.z, w+q.w);}
    inline Quat operator-(const Quat &q) const      {return Quat(x-q.x, y-q.y, z-q.z, w-q.w);}

    inline Quat operator+(float f) const            {return Quat(  x+f,   y+f,   z+f,   w+f);}
    inline Quat operator-(float f) const            {return Quat(  x-f,   y-f,   z-f,   w-f);}
    inline Quat operator*(float f) const            {return Quat(  x*f,   y*f,   z*f,   w*f);}
    inline Quat operator/(float f) const
    {
        f = 1 / f;
        return Quat(x*f, y*f, z*f, w*f);
    }

    inline float operator| (const Quat &q) const    {return (x*q.x)+(y*q.y)+(z*q.z)+(w*q.w);}
    inline Quat  operator^(const Quat &q) const
    {
        return Quat(
            (y*q.z)-(z*q.y)+(w*q.x)+(x*q.w),
            (z*q.x)-(x*q.z)+(w*q.y)+(y*q.w),
            (x*q.y)-(y*q.x)+(w*q.z)+(z*q.w),
            (w*q.w)-(x*q.x)-(y*q.y)-(z*q.z));
    }


    /////////////////////////
    //Transform operators
    Quat operator*(const Quat &q) const;


    /////////////////////////
    //Assignment operators
    inline Quat& operator+=(const Quat &q)          {return (*this)=(*this)+q;}
    inline Quat& operator-=(const Quat &q)          {return (*this)=(*this)-q;}
    Quat& Quat::operator*=(const Quat &q);
    inline Quat& operator*=(const Euler &e);


    inline Quat& operator+=(float f)                {return (*this)=(*this)+f;}
    inline Quat& operator-=(float f)                {return (*this)=(*this)-f;}
    inline Quat& operator*=(float f)                {return (*this)=(*this)*f;}
    inline Quat& operator/=(float f)                {return (*this)=(*this)/f;}

    inline Quat& operator^=(const Quat &q)          {return (*this)=(*this)^q;}

    inline Quat& operator=(const Quat &q)           {x=q.x; y=q.y; z=q.z; w=q.w; return *this;}
    inline Quat& operator=(const Vect &v)           {x=v.x; y=v.y; z=v.z; w=1.0f; return *this;}
    inline Quat& operator=(const AxisAngle &aa)     {return MakeFromAxisAngle(aa);}
    Quat& operator=(const Euler &e);


    /////////////////////////
    //Boolean operators
    inline BOOL operator==(const Quat &q) const     {return (q.x==x)&&(q.y==y)&&(q.z==z)&&(q.w==w);}
    inline BOOL operator!=(const Quat &q) const     {return (q.x!=x)||(q.y!=y)||(q.z!=z)||(q.w!=w);}

    /////////////////////////
    //Other
    inline Quat& Set(float a, float b, float c, float d) {x=a; y=b; z=c; w=d; return *this;}

    inline Quat& SetIdentity()                      {x=0.0f; y=0.0f; z=0.0f; w=1.0f; return *this;}

    inline float Len() const                        {return (float)sqrt((double)(x*x)+(y*y)+(z*z)+(w*w));}

    inline float Dist(const Quat &q) const
    {
        //only do this stuff once
        float a=(x-q.x),b=(y-q.y),c=(z-q.z),d=(w-q.w);
        return (float)sqrt((double)(a*a)+(b*b)+(c*c)+(d*d));
    }

    inline Quat& Norm()
    {
        float len=Len();

        if(len != 0)
        {
            len = 1 / len;

            x *= len; y *= len; z *= len; w *= len;
        }

        return *this;
    }

    inline BOOL CloseTo(const Quat &v, float epsilon=EPSILON) const
    {
        return CloseFloat(x, v.x, epsilon) && CloseFloat(y, v.y, epsilon) && CloseFloat(z, v.z, epsilon) && CloseFloat(w, v.w, epsilon);
    }
#endif

    inline Quat  operator-() const                  {return GetInv();}

    inline Quat& Inv()                              {w=-w;return *this;}//{x=-x;y=-y;z=-z;return *this;}
    inline Quat  GetInv() const                     {return Quat(*this).Inv();}

    inline Quat& Reverse()                          {x=-x;y=-y;z=-z;w=-w;return *this;}
    inline Quat  GetReverse() const                 {return Quat(*this).Reverse();}

    inline Quat GetNorm() const                     {return Quat(*this).Norm();}

    Quat& MakeFromAxisAngle(const AxisAngle &aa);
    AxisAngle GetAxisAngle() const;

    Quat& CreateFromMatrix(const Matrix &m);

    inline Quat&  operator*=(const AxisAngle &aa);
    inline Quat operator*(const AxisAngle &aa)const;

    inline operator float*()                        {return ptr;}


    Vect GetDirectionVector() const;

    Quat& MakeFromDirection(const Vect &dir);
    Quat& SetLookDirection(const Vect &dir);

    static inline Quat GetLookDirection(const Vect &dir) {return Quat().SetLookDirection(dir);}

    Quat& Log();
    Quat& Exp();

    inline Quat GetLog() const {return Quat(*this).Log();}
    inline Quat GetExp() const {return Quat(*this).Exp();}

    Quat GetInterpolationTangent(const Quat &prev, const Quat &next) const;

    static inline Quat Identity() {return Quat().SetIdentity();}

    ////////////////////////
    // Serialization
    friend inline Serializer& operator<<(Serializer &s, Quat &q)
    {
        s.Serialize(q.ptr, sizeof(float)*4);
        return s;
    }
};


/*=========================================================
    Axis Angle
==========================================================*/

struct BASE_EXPORT AxisAngle
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
        /*struct
        {
            Vect axis;
            float angle;
        };*/
        float ptr[4];
    };

    inline AxisAngle()                                                              {}
    
    inline AxisAngle(float a, float b, float c, float d) : x(a), y(b), z(c), w(d)   {}
    inline AxisAngle(const Vect &v, float d) : x(v.x), y(v.y), z(v.z), w(d)         {}
    explicit inline AxisAngle(const Vect &v) : x(v.x), y(v.y), z(v.z), w(1.0f)      {}
    explicit inline AxisAngle(const Quat &q)                                        {MakeFromQuat(q);}


    /////////////////////////
    //Conversion functions
    AxisAngle& MakeFromQuat(const Quat &q);
    Quat GetQuat() const  {Quat newQuat(*this); return newQuat;}

    inline AxisAngle& operator=(const Vect &v)      {x = v.x; y = v.y; z = v.z; return *this;}
    inline AxisAngle& operator=(const Quat &aa)     {return MakeFromQuat(aa);}


    /////////////////////////
    //Other
    inline AxisAngle& Set(float a, float b, float c, float d) {x=a; y=b; z=c; w=d; return *this;}

    inline AxisAngle& Clear() {zero(this, sizeof(AxisAngle)); return *this;}

    inline BOOL CloseTo(const AxisAngle &v, float epsilon=EPSILON) const
    {
        return CloseFloat(x, v.x, epsilon) && CloseFloat(y, v.y, epsilon) && CloseFloat(z, v.z, epsilon) && CloseFloat(w, v.w, epsilon);
    }

    ////////////////////////
    // Serialization
    friend inline Serializer& operator<<(Serializer &s, AxisAngle &aa)
    {
        return s << aa.x << aa.y << aa.z << aa.w;
    }
};


/*=========================================================
    Euler Rotation
==========================================================*/

struct BASE_EXPORT Euler
{
    union
    {
        struct
        {
            float pitch;
            float yaw;
            float roll;
        };
        struct
        {
            float x;
            float y;
            float z;
        };
        float ptr[3];
    };

    
    inline Euler()                                              {}
    inline Euler(float a, float b, float c) : x(a), y(b), z(c)  {}


    ////////////////////////
    //Arithmetic operators
    inline Euler operator+(const Euler &v) const     {return Euler(x+v.x, y+v.y, z+v.z);}
    inline Euler operator-(const Euler &v) const     {return Euler(x-v.x, y-v.y, z-v.z);}
    inline Euler operator*(const Euler &v) const     {return Euler(x*v.x, y*v.y, z*v.z);}
    inline Euler operator/(const Euler &v) const     {return Euler(x/v.x, y/v.y, z/v.z);}

    inline Euler operator+(float f) const            {return Euler(  x+f,   y+f,   z+f);}
    inline Euler operator-(float f) const            {return Euler(  x-f,   y-f,   z-f);}
    inline Euler operator*(float f) const            {return Euler(  x*f,   y*f,   z*f);}
    inline Euler operator/(float f) const
    {
        f = 1 / f;
        return Euler(x*f, y*f, z*f);
    }


    /////////////////////////
    //Assignment operators
    inline Euler& operator+=(const Euler &v)         {return (*this)=(*this)+v;}
    inline Euler& operator-=(const Euler &v)         {return (*this)=(*this)-v;}
    inline Euler& operator*=(const Euler &v)         {return (*this)=(*this)*v;}
    inline Euler& operator/=(const Euler &v)         {return (*this)=(*this)/v;}

    inline Euler& operator+=(float f)                {return (*this)=(*this)+f;}
    inline Euler& operator-=(float f)                {return (*this)=(*this)-f;}
    inline Euler& operator*=(float f)                {return (*this)=(*this)*f;}
    inline Euler& operator/=(float f)                {return (*this)=(*this)/f;}

    inline Euler& operator=(const Euler &v)          {x=v.x; y=v.y; z=v.z; return *this;}
    inline Euler& operator=(float f)                 {x=f; y=f; z=f; return *this;}

    ////////////////////////
    //Boolean operators
    inline BOOL operator==(const Euler &v) const     {return (v.x==x)&&(v.y==y)&&(v.z==z);}
    inline BOOL operator!=(const Euler &v) const     {return (v.x!=x)||(v.y!=y)||(v.z!=z);}

    inline Euler operator-() const                   {return Euler(-x, -y, -z);}


    ////////////////////////
    //Type casting
    inline operator float*()                         {return ptr;}

    ////////////////////////
    // Serialization
    friend inline Serializer& operator<<(Serializer &s, Euler &e)
    {
        return s << e.pitch << e.yaw << e.roll;
    }
};


/*=========================================================
    Bounds
==========================================================*/

enum {BOUNDS_OUTSIDE=1, BOUNDS_INSIDE=2, BOUNDS_PARTIAL=4};

//for use with getpoint, as flags
enum {MAX_Z=1, MAX_Y=2, MAX_X=4};

struct BASE_EXPORT Bounds
{
    Vect Min;
    Vect Max;


    inline Bounds()                                                 {}
    inline Bounds(const Vect &a, const Vect &b) : Min(a), Max(b)    {}

    inline Bounds operator+(const Vect &pos) const
    {
        Bounds movedBounds = *this;
        movedBounds.Min += pos;
        movedBounds.Max += pos;
        return movedBounds;
    }

    inline Bounds& operator+=(const Vect &pos)
    {
        Min += pos;
        Max += pos;
        return *this;
    }

    inline Bounds& operator=(const Bounds &box)
    {
        Min = box.Min;
        Max = box.Max;
        return *this;
    }

    inline Bounds GetScaled(const Vect &scale) const
    {
        return Bounds(*this).Scale(scale);
    }

    inline Bounds& Scale(const Vect &scale)
    {
        Min *= scale;
        Max *= scale;
        return *this;
    }

    inline Bounds GetMerged(const Bounds &bounds) const
    {
        return Bounds(*this).Merge(bounds);
    }

    inline Bounds& Merge(const Bounds &bounds)
    {
        Min.ClampMax(bounds.Min);
        Max.ClampMin(bounds.Max);

        return *this;
    }

    inline Bounds& MergePoint(const Vect &point)
    {
        Min.ClampMax(point);
        Max.ClampMin(point);
        return *this;
    }

    Vect GetPoint(unsigned int i) const;

    Bounds GetTransformedBounds(const Matrix &m) const;
    Bounds& Transform(const Matrix &m);

    Bounds& TransformNoRot(const Matrix &m);

    inline Vect GetCenter() const
    {
        return (((Max-Min)*0.5f)+Min);
    }

    inline BOOL BoundsInside(const Bounds &test) const
    {
        return (test.Min.x >= Min.x) && (test.Min.y >= Min.y) && (test.Min.z >= Min.z) &&
               (test.Max.x <= Max.x) && (test.Max.y <= Max.y) && (test.Max.z <= Max.z);
                
    }

    inline BOOL Intersects(const Bounds &test, float epsilon=EPSILON) const
    {
        /*int i;

        for(i=0;i<3;i++)
        {
            if(((Min.ptr[i]-test.Max.ptr[i]) > epsilon) || ((test.Min.ptr[i]-Max.ptr[i]) > epsilon))
            //if((Min.ptr[i] > test.Max.ptr[i]) || (test.Min.ptr[i] > Max.ptr[i]))
                return 0;
        }*/

        if( ((Min.x - test.Max.x) > epsilon) || ((test.Min.x - Max.x) > epsilon) ||
            ((Min.y - test.Max.y) > epsilon) || ((test.Min.y - Max.y) > epsilon) ||
            ((Min.z - test.Max.z) > epsilon) || ((test.Min.z - Max.z) > epsilon) )
        {
            return FALSE;
        }

        return TRUE;
    }

    BOOL IntersectsOBB(const Bounds &test, const Matrix &transform) const;

    inline BOOL SphereIntersects(const Vect &center, float radius) const
    {
        int i;
        float s, d = 0;

        for(i=0;i<3;i++)
        {
            if(center.ptr[i] < Min.ptr[i])
            {
                s = center.ptr[i] - Min.ptr[i];
                d += s*s;
            }
            else if(center.ptr[i] > Max.ptr[i])
            {
                s = center.ptr[i] - Max.ptr[i];
                d += s*s;
            }
        }
        return (d <= (radius*radius));
    }

    BOOL CylinderIntersects(const Vect &center, float radius, float height) const;

    inline float VectorOffsetLength(const Vect &v) const
    {
        return (Max-Min).Abs().Dot(v.GetAbs());
    }

    float MinDistFrom(const Plane &p) const;

    inline float MaxDistFrom(const Plane &p) const
    {
        float bestDist = 1.0f;

        for(int i=0; i<8; i++)
        {
            float dist = GetPoint(i).DistFromPlane(p);
            if(dist > bestDist)
                bestDist = dist;
        }

        return bestDist;
    }

    /*inline void Transform(const Matrix &m)
    {
        Min.TransformPoint(m);
        Max.TransformPoint(m);
    }*/

    inline BOOL PointInside(const Vect &point) const
    {
        return ((point.x >= (Min.x-EPSILON)) &&
                (point.x <= (Max.x+EPSILON)) &&
                (point.y >= (Min.y-EPSILON)) &&
                (point.y <= (Max.y+EPSILON)) &&
                (point.z >= (Min.z-EPSILON)) &&
                (point.z <= (Max.z+EPSILON)));

    }

    inline BOOL PointInsideTopDown(const Vect &point) const
    {
        return ((point.x >= (Min.x-EPSILON)) &&
                (point.x <= (Max.x+EPSILON)) &&
                (point.z >= (Min.z-EPSILON)) &&
                (point.z <= (Max.z+EPSILON)));

    }

    inline float GetDiamater()
    {
        return Max.Dist(Min);
    }

    BOOL RayIntersection(const Vect &rayOrig, const Vect &rayDir, float &fT) const;
    BOOL RayIntersectionTopDown(const Vect &rayOrig, const Vect &rayDir, float &fT) const;
    BOOL LineIntersection(const Vect &v1, const Vect &v2, float &fT) const;
    BOOL LineIntersectionTopDown(const Vect &v1, const Vect &v2, float &fT) const;

    inline BOOL RayIntersects(const Vect &rayOrig, const Vect &rayDir) const
    {
        float fT;
        return RayIntersection(rayOrig, rayDir, fT);
    }

    inline BOOL RayIntersectsTopDown(const Vect &rayOrig, const Vect &rayDir) const
    {
        float fT;
        return RayIntersectionTopDown(rayOrig, rayDir, fT);
    }

    inline BOOL LineIntersects(const Vect &v1, const Vect &v2) const
    {
        float fT;
        return LineIntersection(v1, v2, fT);
    }

    inline BOOL LineIntersectsTopDown(const Vect &v1, const Vect &v2) const
    {
        float fT;
        return LineIntersectionTopDown(v1, v2, fT);
    }

    int PlaneTest(const Plane &p) const;
    BOOL OutsidePlane(const Plane &p) const;

    inline Bounds ModifiedPos(const Vect &pos) const
    {
        Bounds modBounds = *this;
        modBounds.Min += pos;
        modBounds.Max += pos;
        return modBounds;
    }

    inline Vect GetSize() const
    {
        return Max-Min;
    }

    Bounds& MakeFromPoints(const List<Vect> &pointList);

    ////////////////////////
    // Serialization
    friend inline Serializer& operator<<(Serializer &s, Bounds &b)
    {
        return s << b.Min << b.Max;
    }
};


/*=========================================================
    Plane
==========================================================*/

struct BASE_EXPORT Plane
{
    Vect  Dir;
    float Dist;

    inline Plane()                                                              {}
    inline Plane(float a, float b, float c, float d) : Dir(a, b, c), Dist(d)    {}
    inline Plane(const Vect &a, float b) : Dir(a), Dist(b)                      {}
    inline Plane(const Vect &a, const Vect &b, const Vect &c)                   {CreateFromTri(a, b, c);}


    /////////////////////////
    //Arithmetic operators
    inline float operator|(const Vect& p) const             {return (p|Dir)-Dist;}
    inline Plane operator-() const                          {return Plane(-Dir, -Dist);}


    /////////////////////////
    //Boolean operators
    inline BOOL  operator==(const Plane& p) const           {return CloseTo(p, EPSILON);}
    inline BOOL  operator!=(const Plane& p) const           {return !CloseTo(p, EPSILON);}


    /////////////////////////
    //Assignment operators
    inline Plane& operator=(const Plane& p)                 {Dir = p.Dir; Dist = p.Dist; return *this;}


    /////////////////////////
    //Matrix Transformation
    Plane& Transform(const Matrix &m);
    Plane  GetTransform(const Matrix &m) const
    {
        return Plane(*this).Transform(m);
    }


    /////////////////////////
    //Other
    void CreateFromTri(const Vect &a, const Vect &b, const Vect &c);

    Plane& Set(float a, float b, float c, float d)
    {
        Dir.x = a; Dir.y = b; Dir.z = c; Dist = d;
        return *this;
    }

    BOOL GetRayIntersection(const Vect &rayOrigin, const Vect &rayDir, float &fT) const;
    BOOL GetIntersection(const Vect &p1, const Vect &p2, float &fT) const;

    BOOL GetDoublePlaneIntersection(const Plane &p2, Vect &intOrigin, Vect &intDir) const;
    BOOL GetTriplePlaneIntersection(const Plane &p2, const Plane &p3, Vect &intersection) const;


    inline int TriInside(const Vect &p1, const Vect &p2, const Vect &p3, float precision=LARGE_EPSILON) const
    {
        //bit 1: part or all of the triangle is behind the plane
        //bit 2: part or all of the triangle is in front of the plane
        int sides=0;
        float d1 = p1.DistFromPlane(*this);
        float d2 = p2.DistFromPlane(*this);
        float d3 = p3.DistFromPlane(*this);

        if(d1 >= precision)
            sides |= 2;
        else if(d1 <= -precision)
            sides |= 1;

        if(d2 >= precision)
            sides |= 2;
        else if(d2 <= -precision)
            sides |= 1;

        if(d3 >= precision)
            sides |= 2;
        else if(d3 <= -precision)
            sides |= 1;

        return sides;
    }

    inline int LineInside(const Vect &p1, const Vect &p2, float precision=EPSILON) const
    {
        int sides=0;
        float d1 = p1.DistFromPlane(*this);
        float d2 = p2.DistFromPlane(*this);

        if(d1 >= precision)
            sides |= 2;
        else if(d1 <= -precision)
            sides |= 1;

        if(d2 >= precision)
            sides |= 2;
        else if(d2 <= -precision)
            sides |= 1;

        return sides;
    }

    BOOL CloseTo(const Plane &plane, float precision=LARGE_EPSILON) const
    {
        return Dir.CloseTo(plane.Dir, precision) && CloseFloat(Dist, plane.Dist, precision);
    }

    BOOL Coplanar(const Plane &plane, float precision=EPSILON) const
    {
        float fCosAngle = Dir.Dot(plane.Dir);

        if(CloseFloat(fCosAngle, 1.0f, precision))
            return CloseFloat(plane.Dist, Dist, precision);
        else if(CloseFloat(fCosAngle, -1.0f, precision))
            return CloseFloat(plane.Dist, -Dist, precision);

        return FALSE;
    }

    ////////////////////////
    // Serialization
    friend inline Serializer& operator<<(Serializer &s, Plane &p)
    {
        return s << p.Dir << p.Dist;
    }

    static inline Serializer& SerializeList(Serializer &s, List<Plane> &list)
    {
        UINT num;
        if(s.IsLoading())
        {
            s << num;
            list.SetSize(num);
        }
        else
        {
            num = list.Num();
            s << num;
        }

        for(unsigned int i=0; i<num; i++)
            s << list[i];

        return s;
    }
};



/*=========================================================
    Matrix (3x4)
==========================================================*/

struct BASE_EXPORT Matrix
{
    /*union
    {
        struct
        {      */
            Vect X; //X axis
            Vect Y; //Y axis
            Vect Z; //Z axis
            Vect T; //location (origin)
        /*};
        float ptr[12];
        float ptr3x4[3][4];
    };   */

    inline Matrix()                                 {}
    inline Matrix(const Quat &q)                    {CreateFromQuat(q); T = 0;}
    inline Matrix(const AxisAngle &aa)              {CreateFromQuat(aa.GetQuat()); T = 0;}
    inline Matrix(const Euler &e)                   {SetIdentity() *= e;}

    inline Matrix(const Vect &a, const Vect &b, const Vect &c, const Vect &o)
        : X(a), Y(b), Z(c), T(o)  {}


    /////////////////////////
    //Arithmetic operators
    inline Matrix& operator*=(const Matrix &m)
    {
        X.TransformVector(m);
        Y.TransformVector(m);
        Z.TransformVector(m);
        T.TransformPoint(m);
        return *this;
    }

    inline Matrix operator*(const Matrix &m) const
    {
        return Matrix(*this) *= m;
    }

    inline Matrix& Multiply(const Matrix &m)
    {
        return (*this *= m);
    }

    inline Matrix GetMultiply(const Matrix &m) const
    {
        return (Matrix(*this) *= m);
    }


    inline Matrix& operator*=(const Vect &v)
    {
        /*Vect Temp(v);
        Temp.TransformVector(*this);
        T += Temp;*/

        T -= v;

        return *this;
    }

    inline Matrix operator*(const Vect &v) const
    {
        return Matrix(*this) *= v;
    }

    inline Matrix& Translate(const Vect &v)
    {
        return (*this *= v);
    }


    inline Matrix& operator*=(const Quat &q)
    {
        return *this *= Matrix(q);
    }

    inline Matrix operator*(const Quat &q) const
    {
        return Matrix(*this) *= q;
    }

    inline Matrix& operator*=(const Euler &e)
    {
        Matrix Temp;

        Temp.T = 0.0;

        if(e.x)
        {
            Temp.X.x = 1.0;         Temp.X.y = 0.0;         Temp.X.z = 0.0;
            Temp.Y.x = 0.0;         Temp.Y.y = cos(e.x);    Temp.Y.z = -sin(e.x);
            Temp.Z.x = 0.0;         Temp.Z.y = sin(e.x);    Temp.Z.z = cos(e.x);

            *this *= Temp;

        }
        if(e.y)
        {
            Temp.X.x = cos(e.y);    Temp.X.y = 0.0;         Temp.X.z = sin(e.y);
            Temp.Y.x = 0.0;         Temp.Y.y = 1.0;         Temp.Y.z = 0.0;
            Temp.Z.x = -sin(e.y);   Temp.Z.y = 0.0;         Temp.Z.z = cos(e.y);

            *this *= Temp;
        }
        if(e.z)
        {
            Temp.X.x = cos(e.z);    Temp.X.y = -sin(e.z);   Temp.X.z = 0.0;
            Temp.Y.x = sin(e.z);    Temp.Y.y = cos(e.z);    Temp.Y.z = 0.0;
            Temp.Z.x = 0.0;         Temp.Z.y = 0.0;         Temp.Z.z = 1.0;

            *this *= Temp;
        }

        return *this;
    }

    inline Matrix operator*(const Euler &e) const
    {
        return Matrix(*this) *= e;
    }

    inline Matrix& Rotate(const Quat &q)
    {
        return (*this *= q);
    }

    inline Matrix& Rotate(const AxisAngle &aa)
    {
        return (*this *= Quat(aa));
    }

    inline Matrix& Rotate(const Euler &e)
    {
        return (*this *= e);
    }


    /////////////////////////
    //Matrix functions
    inline Matrix& SetIdentity()
    {
        zero(this, sizeof(Matrix));
        X.x = Y.y = Z.z = 1.0;
        return *this;
    }

    static inline Matrix Identity()
    {
        Matrix Temp;
        Temp.SetIdentity();
        return Temp;
    }

#ifdef USE_SSE
    //transposing an affine matrix is the same as an inverse
    inline Matrix GetTranspose() const
    {
        return Matrix(*this).Transpose();
    }

    //tmp1 = x.x, x.y, y.x, y.y
    //tmp2 = x.z, x.w, y.z, y.w
    //tmp1 = shuffle X, Y, 0 1 0 1 = movelh
    //tmp2 = shuffle X, Y, 2 3 2 3 = movehl
    //X = shuffle tmp1, Z, 0 2 0 3
    //Y = shuffle tmp1, Z, 1 3 1 3
    //Z = shuffle tmp2, Z, 0 2 2 3
    inline Matrix& Transpose()
    {
        T.TransformVector(*this); T = -T;

        __m128 tmp1 = _mm_movelh_ps(X.m, Y.m);
        __m128 tmp2 = _mm_movehl_ps(Y.m, X.m);
        X.m = _mm_shuffle_ps(tmp1, Z.m, _MM_SHUFFLE(3, 0, 2, 0));
        Y.m = _mm_shuffle_ps(tmp1, Z.m, _MM_SHUFFLE(3, 1, 3, 1));
        Z.m = _mm_shuffle_ps(tmp2, Z.m, _MM_SHUFFLE(3, 2, 2, 0));

        return *this;
    }
#else
    //transposing an affine matrix is the same as an inverse
    inline Matrix GetTranspose() const
    {
        return Matrix(Vect(X.x, Y.x, Z.x), Vect(X.y, Y.y, Z.y), Vect(X.z, Y.z, Z.z), -T.GetTransformedVector(*this));
    }

    inline Matrix& Transpose()
    {
        return (*this = GetTranspose());
    }
#endif

    inline Matrix& Scale(float scaleX, float scaleY, float scaleZ)
    {
        /*X.x *= scaleX;
        X.y *= scaleY;
        X.z *= scaleZ;

        Y.x *= scaleX;
        Y.y *= scaleY;
        Y.z *= scaleZ;

        Z.x *= scaleX;
        Z.y *= scaleY;
        Z.z *= scaleZ;

        T.x /= scaleX;
        T.y /= scaleY;
        T.z /= scaleZ;
        return *this;*/
        return Scale(Vect(scaleX, scaleY, scaleZ));
    }

    inline Matrix& Scale(const Vect &scale)
    {
        X *= scale;
        Y *= scale;
        Z *= scale;
        T /= scale;

        return *this;
        //return Scale(scale.x, scale.y, scale.z);
    }

    inline Matrix GetScale(float scaleX, float scaleY, float scaleZ) const
    {
        return Matrix(*this).Scale(Vect(scaleX, scaleY, scaleZ));
    }

    inline Matrix GetScale(const Vect &scale) const
    {
        return Matrix(*this).Scale(scale);
    }

    Matrix GetInverse() const;
    Matrix& Inverse();

    inline Matrix& MirrorByVector(const Vect &v)
    {
        X.MirrorByVector(v);
        Y.MirrorByVector(v);
        Z.MirrorByVector(v);
        T.MirrorByVector(v);

        return *this;
    }

    inline Matrix& MirrorByPlane(const Plane &p)
    {
        Vect dir = p.Dir;
        X.MirrorByVector(dir);
        Y.MirrorByVector(dir);
        Z.MirrorByVector(dir);
        T.MirrorByPlane(p);

        return *this;
    }

    inline Matrix GetMirrorByVector(const Vect &v) const
    {
        return Matrix(*this).MirrorByVector(v);
    }

    inline Matrix GetMirrorByPlane(const Plane &p) const
    {
        return Matrix(*this).MirrorByPlane(p);
    }



    /////////////////////////
    //Other
    /*inline float operator[](unsigned int index) const
    {
        assert(index < 12);

        return ptr[index];
    }*/


    /////////////////////////
    //Conversion functions
    void CreateFromQuat(const Quat &q);

    ////////////////////////
    // Serialization
    friend inline Serializer& operator<<(Serializer &s, Matrix &m)
    {
        return s << m.X << m.Y << m.Z << m.T;
    }
};


/*=========================================================
    Line2
==========================================================*/

struct BASE_EXPORT Line2
{
    Vect2 A;
    Vect2 B;

    Line2()                                                 {}
    Line2(const Vect2 &v1, const Vect2 &v2) : A(v1), B(v2)  {}

    BOOL LinesIntersect(const Line2 &collider) const;

    inline Vect2& GetDirection() const
    {
        return (B-A).Norm();
    }

    ////////////////////////
    // Serialization
    friend inline Serializer& operator<<(Serializer &s, Line2 &line2)
    {
        return s << line2.A << line2.B;
    }
};


/*=========================================================
    Other
==========================================================*/

BASE_EXPORT Quat InterpolateQuat(const Quat &from, const Quat &to, float t);
BASE_EXPORT Quat CubicInterpolateQuat(const Quat &prev, const Quat &from, const Quat &to, const Quat &after, float t);

BASE_EXPORT Vect GetHSpline(const Vect &v0, const Vect &v1, const Vect &m0, const Vect &m1, float fT);

BASE_EXPORT BOOL SphereRayCollision(const Vect &sphereCenter, float sphereRadius, const Vect &rayOrig, const Vect &rayDir, Vect *collision=NULL, Plane *collisionPlane=NULL);
BASE_EXPORT BOOL CylinderRayCollision(const Vect &cylCenter, float cylRadius, float cylHeight, const Vect &rayOrig, const Vect &rayDir, Vect *collision=NULL, Plane *collisionPlane=NULL);

BASE_EXPORT BOOL PointOnFiniteLine(const Vect &lineV1, const Vect &lineV2, const Vect &p);

BASE_EXPORT BOOL ClosestLinePoint(const Vect &ray1Origin, const Vect &ray1Dir, const Vect &ray2Origin, const Vect &ray2Dir, float &fT);
BASE_EXPORT BOOL ClosestLinePoints(const Vect &ray1Origin, const Vect &ray1Dir, const Vect &ray2Origin, const Vect &ray2Dir, float &fT1, float &fT2);


inline Vect2::Vect2(const Vect &v)
{
    x = v.x;
    y = v.y;
}

inline Vect2& Vect2::operator=(const Vect &v)
{
    x=v.x;
    y=v.y;
    return *this;
}


inline CompactVect::CompactVect(const Vect &v)
{
    x = v.x; y = v.y; z = v.z;
}

inline CompactVect& CompactVect::operator=(const Vect &v)
{
    x = v.x; y = v.y; z = v.z;
    return *this;
}


#ifdef USE_SSE
inline Vect::Vect(const Vect4 &v) {m = v.m; w = 0.0f;}
#else
inline Vect::Vect(const Vect4 &v) : x(v.x), y(v.y), z(v.z), w(0.0f) {}
#endif

inline Vect& Vect::TransformPoint(const Matrix &m)
{
    Vect Temp(*this-m.T);
    x = Temp.Dot(m.X);
    y = Temp.Dot(m.Y);
    z = Temp.Dot(m.Z);

    return *this;
}

inline Vect& Vect::TransformVector(const Matrix &m)
{
    Vect Temp(*this);
    x = Temp.Dot(m.X);
    y = Temp.Dot(m.Y);
    z = Temp.Dot(m.Z);

    return *this;
}

inline void Vect::MirrorByPlane(const Plane& p)
{
    *this -= (p.Dir * (2.0f * DistFromPlane(p)));
}

inline void Vect::MirrorByVector(const Vect& v)
{
    (*this) -= (v * (2.0f * Dot(v)));
}

inline Quat Quat::operator*(const Quat &q) const
{
    return Quat(*this) *= q;
}

inline Quat& Quat::operator*=(const AxisAngle &aa)      {return *this *= aa.GetQuat();}
inline Quat  Quat::operator*(const AxisAngle &aa) const {return Quat(*this) *= aa.GetQuat();}

BASE_EXPORT Vect CartToPolar(const Vect &cart);
BASE_EXPORT Vect PolarToCart(const Vect &polar);
BASE_EXPORT Vect PolarToCart(float latitude, float longitude, float dist);

BASE_EXPORT Vect2 NormToPolar(const Vect  &norm);
BASE_EXPORT void  NormToPolar(const Vect &norm, float &latitude, float &longitude);
BASE_EXPORT Vect  PolarToNorm(const Vect2 &polar);

BASE_EXPORT float GetPlaneCylinderOffset(const Vect &dir, float cylHalfHeight, float cylRadius);
BASE_EXPORT float GetPlaneCapsuleOffset(const Vect &dir, float capsuleHalfHeight, float capsuleRadius);
BASE_EXPORT float GetPlaneConeOffset(const Vect &dir, float coneHeight, float coneRadius);

BASE_EXPORT float RandomFloat(BOOL bPositiveOnly=FALSE);
BASE_EXPORT Vect  RandomVect(BOOL bPositiveOnly=FALSE);

//calculates a torque between two values and makes the adjustment to val1
BASE_EXPORT void CalcTorque(float &val1, float val2, float torque, float minAdjust, float fT);
BASE_EXPORT void CalcTorque(Vect &val1, const Vect &val2, float torque, float minAdjust, float fT);

inline float GetPercentage(float startVal, float endVal, float middleVal)
{
    return (middleVal-startVal)/(endVal-startVal);
}

inline float GetPercentageI(int startVal, int endVal, int middleVal)
{
    return (float)(middleVal-startVal)/(float)(endVal-startVal);
}

inline float TriangleArea(const Vect &v0, const Vect &v1, const Vect &v2)
{
    return fabsf(((v1-v0)^(v2-v0)).Len()*0.5f);
}

//simple version
inline BOOL PointInsideTri(const Vect2 &v1, const Vect2 &v2, const Vect2 &v3, const Vect2 &p)
{
    Vect2 n1 = (v1-p).Norm();
    Vect2 n2 = (v2-p).Norm();
    Vect2 n3 = (v3-p).Norm();

    float a1 = acos(n1.Dot(n2));
    float a2 = acos(n2.Dot(n3));
    float a3 = acos(n3.Dot(n1));

    return CloseFloat((a1+a2+a3), M_PI*2.0f);
}

BASE_EXPORT void Matrix4x4Identity(float *destMatrix);
BASE_EXPORT void Matrix4x4Convert(float *destMat4x4, const Matrix &matSource);
BASE_EXPORT void Matrix4x4Convert(Matrix &destMat3x4, float *matSource);
BASE_EXPORT void Matrix4x4Multiply(float *destMatrix, float *M1, float *M2);
BASE_EXPORT void Matrix4x4TransformVect(Vect4 &out, float *M1, const Vect4 &vec);
BASE_EXPORT float Matrix3x3Determinant(float *M1);
BASE_EXPORT float Matrix4x4Determinant(float *M1);
BASE_EXPORT void Matrix4x4SubMatrix(float *destMatrix, float *M1, int i, int j);
BASE_EXPORT BOOL Matrix4x4Inverse(float *destMatrix, float *M1);
BASE_EXPORT void Matrix4x4Transpose(float *destMatrix, float *srcMatrix);
BASE_EXPORT void Matrix4x4Ortho(float *destMatrix, double left, double right, double bottom, double top, double near, double far);
BASE_EXPORT void Matrix4x4Frustum(float *destMatrix, double left, double right, double bottom, double top, double near, double far);
BASE_EXPORT void Matrix4x4Perspective(float *destMatrix, float angle, float aspect, float near, float far);

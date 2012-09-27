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


#include "XT.h"






/*=========================================================
    3-D Vector
==========================================================*/

Vect Vect::GetInterpolationTangent(const Vect &prev, const Vect &next) const
{
    return (((*this)-prev)+next-(*this))*0.5f;
}

float Vect::DistFromPlane(const Plane& p) const
{
    return Dot(p.Dir) - p.Dist;
}


/*=========================================================
    Quaternion
==========================================================*/

Quat::Quat(const Euler &e)
{
    SetIdentity() *= e;
}

Quat& Quat::operator=(const Euler &e)
{
    return SetIdentity() *= e;
}

Quat& Quat::operator*=(const Euler &e)
{
    Vect E;
    mcpy(&E, &e, sizeof(Euler));
    E *= 0.5f;

    *this *= Quat(0.0f, 0.0f, sin(E.z), cos(E.z));
    *this *= Quat(0.0f, sin(E.y), 0.0f, cos(E.y));
    *this *= Quat(sin(E.x), 0.0f, 0.0f, cos(E.x));
    w = -w;

    return *this;
}

inline Vect GetQuatVect(const Quat &q)
{
    Vect v = (Vect&)q;
    v.w = 0.0f;
    return v;
}

Quat& Quat::operator*=(const Quat &q)
{
    Vect tempAxis = GetQuatVect(*this), qAxis = GetQuatVect(q), axisOut;
    axisOut = ((qAxis * w) + (tempAxis * q.w)) + (tempAxis ^ qAxis);
    axisOut.w = (w * q.w) - (tempAxis | qAxis);
    *this = (Quat&)axisOut;

    return *this;
}

Quat& Quat::Log()
{
    float angle = (float)acos(w);
    float sine  = (float)sin(angle);

    if ((fabs(w) < 1.0f) && (fabs(sine) >= EPSILON))
    {
        sine = angle/sine;
        w = 0.0f;
        return *this *= sine;
    }
    else
        w = 0.0f;

    return *this;
}

Quat& Quat::Exp()
{
    float length = sqrtf(x*x+y*y+z*z);
    float sine   = sinf(length);
    if(length > EPSILON)
        sine /= length;
    else
        sine = 1.0f;

    *this *= sine;
    w = cosf(length);

    return *this;
}


Quat InterpolateQuat(const Quat &from, const Quat &to, float t)
{
    float dot = from | to;
    float anglef = acosf(dot);
    float sine,sinei,sinet,sineti;

    if(anglef >= EPSILON)
    {
        sine   = sinf(anglef);
        sinei  = 1/sine;
        sinet  = sinf(anglef*t)*sinei;
        sineti = sinf(anglef*(1.0f-t))*sinei;

        return Quat((from * sineti) + (to * sinet));
    }
    return Quat(Lerp<Quat>(from, to, t));
}


Quat CubicInterpolateQuat(const Quat &q0, const Quat &q1, const Quat &m0, const Quat &m1, float t)
{
    return InterpolateQuat(InterpolateQuat(q0, q1, t), InterpolateQuat(m0, m1, t), 2*(1-t)*t);
}



Quat& Quat::MakeFromAxisAngle(const AxisAngle &aa)
{
    float halfa = aa.w*0.5f;
    float sine  = sinf(halfa);

    x = aa.x * sine;
    y = aa.y * sine;
    z = aa.z * sine;
    w  = cosf(halfa);

    return *this;
}

AxisAngle Quat::GetAxisAngle() const
{
    AxisAngle newAA(*this);
    return newAA;
}

AxisAngle& AxisAngle::MakeFromQuat(const Quat &q)
{
    float len,leni;

    len = (q.x*q.x)+(q.y*q.y)+(q.z*q.z);
    if(!CloseFloat(len, 0.0f))
    {
        leni = (1.0f/(float)sqrt(len));
        x = q.x * leni;
        y = q.y * leni;
        z = q.z * leni;
        w  = (float)acos(q.w)*2.0f;
    }
    else
        x = y = z = w = 0.0f;

    return *this;
}

struct f4x4
{
    float ptr[4][4];
};

Quat& Quat::CreateFromMatrix(const Matrix &m)
{
    float Tr = (m.X.x + m.Y.y + m.Z.z);
    int i,j,k;
    
    if (Tr > 0.0f)
    {
        float fourD = sqrt(Tr+1.0f);
        w = fourD*0.5f;

        float invHalf = 0.5f/fourD;
        x = (m.Y.z - m.Z.y)*invHalf;
        y = (m.Z.x - m.X.z)*invHalf;
        z = (m.X.y - m.Y.x)*invHalf;
    }
    else
    {
        if (m.X.x > m.Y.y)
            i = 0;
        else
            i = 1;

        f4x4 &val = (f4x4&)m;

        if (m.Z.z > val.ptr[i][i])
            i = 2;
        
        j = (i+1)%3;
        k = (i+2)%3;

        //----------------------------------

        float fourD = sqrt((val.ptr[i][i] - val.ptr[j][j] - val.ptr[k][k]) + 1.0f);

        ptr[i] = fourD*0.5f;

        float invHalf = 0.5f/fourD;
        ptr[j]  = (val.ptr[i][j] + val.ptr[j][i])*invHalf;
        ptr[k]  = (val.ptr[i][k] + val.ptr[k][i])*invHalf;
        w =       (val.ptr[j][k] - val.ptr[k][j])*invHalf;
    }

    return *this;
}

Quat Quat::GetInterpolationTangent(const Quat &prev, const Quat &next) const
{
    Quat cinv = this->GetInv();

    return (*this)*(-((cinv*prev).Log() + (cinv*next).Log())/4.0f).Exp();
}

Vect Quat::GetDirectionVector() const
{
    Matrix m(*this);

    return m.Z;
}

Quat& Quat::MakeFromDirection(const Vect &dir)
{
    Vect identDir(0.0f, 0.0f, 1.0f);

    if(dir.CloseTo(identDir))
        return SetIdentity();
    else
    {
        float cosValue = fabsf(dir.z); //dir.Dot(identDir);
        float angle = acos(cosValue);
        Vect cross = Vect(dir.y, -dir.x, 0.0f).Norm(); //(dir.GetCross(identDir).Norm()

        return MakeFromAxisAngle(AxisAngle(cross.x, cross.y, cross.z, angle)).Norm();
    }
}

Quat& Quat::SetLookDirection(const Vect &dir)
{
    Vect newDir = -dir.GetNorm();
    Quat XZrot, YZrot;

    XZrot.SetIdentity();
    YZrot.SetIdentity();

    BOOL bXZValid = !CloseFloat(newDir.x, 0.0f, EPSILON) || !CloseFloat(newDir.z, 0.0f, EPSILON);
    BOOL bYZValid = !CloseFloat(newDir.y, 0.0f, EPSILON);

    if(bXZValid)
        XZrot = AxisAngle(0.0f, 1.0f, 0.0f, atan2f(newDir.x, newDir.z));
    if(bYZValid)
        YZrot = AxisAngle(-1.0f, 0.0f, 0.0f, asinf(newDir.y));

    if(!bXZValid)
        *this = YZrot;
    else if(!bYZValid)
        *this = XZrot;
    else
        *this = XZrot *= YZrot;

    return *this;
}


/*=========================================================
    Bounds
==========================================================*/

Vect Bounds::GetPoint(unsigned int i) const
{
    assert(i < 8);

    Vect v;

    // Note:
    //
    //  0 = min.x,min.y,min.z
    //  1 = min.x,min.y,MAX.z
    //  2 = min.x,MAX.y,min.z
    //  3 = min.x,MAX.y,MAX.z
    //  4 = MAX.x,min.y,min.z
    //  5 = MAX.x,min.y,MAX.z

    //  6 = MAX.x,MAX.y,min.z
    //  7 = MAX.x,MAX.y,MAX.z

    if(i > 3)    {v.x = Max.x;  i -= 4;}
    else         {v.x = Min.x;}

    if(i > 1)    {v.y = Max.y;  i -= 2;}
    else         {v.y = Min.y;}

    if(i == 1)   {v.z = Max.z;}
    else         {v.z = Min.z;}

    return v;
}

Bounds Bounds::GetTransformedBounds(const Matrix &m) const
{
    Bounds newBounds;
    BOOL bInitialized=0;
    for(int i=0; i<8; i++)
    {
        Vect p = GetPoint(i);
        p.TransformPoint(m);

        if(!bInitialized)
        {
            newBounds.Max = newBounds.Min = p;
            bInitialized = 1;
        }
        else
        {
            if(p.x < newBounds.Min.x)
                newBounds.Min.x = p.x;
            else if(p.x > newBounds.Max.x)
                newBounds.Max.x = p.x;

            if(p.y < newBounds.Min.y)
                newBounds.Min.y = p.y;
            else if(p.y > newBounds.Max.y)
                newBounds.Max.y = p.y;

            if(p.z < newBounds.Min.z)
                newBounds.Min.z = p.z;
            else if(p.z > newBounds.Max.z)
                newBounds.Max.z = p.z;
        }
    }

    return newBounds;
}

Bounds& Bounds::Transform(const Matrix &m)
{
    return (*this = GetTransformedBounds(m));
}

Bounds& Bounds::TransformNoRot(const Matrix &m)
{
    Min.TransformPoint(m);
    Max.TransformPoint(m);
    return *this;
}

BOOL Bounds::IntersectsOBB(const Bounds &test, const Matrix &transform) const
{
    Bounds testT = test.GetTransformedBounds(transform);
    Bounds thisT = GetTransformedBounds(transform.GetTranspose());

    if(Intersects(testT) && thisT.Intersects(test))
        return TRUE;

    return FALSE;
}

int Bounds::PlaneTest(const Plane &p) const
{
    Vect vmin,vmax;

    for(int i=0; i<3; i++)
    {
        if(p.Dir.ptr[i] >= 0.0f)
        {
            vmin.ptr[i] = Min.ptr[i];
            vmax.ptr[i] = Max.ptr[i];
        }
        else
        {
            vmin.ptr[i] = Max.ptr[i];
            vmax.ptr[i] = Min.ptr[i];
        }
    }

    if(vmin.DistFromPlane(p) > 0.0f)  return  BOUNDS_OUTSIDE;

    if(vmax.DistFromPlane(p) >= 0.0f) return  BOUNDS_PARTIAL;

    return BOUNDS_INSIDE;
}

BOOL Bounds::OutsidePlane(const Plane &p) const
{
    Vect vmin;

    vmin.x = (*(DWORD*)&p.Dir.x & 0x80000000) ? Max.x : Min.x;
    vmin.y = (*(DWORD*)&p.Dir.y & 0x80000000) ? Max.y : Min.y;
    vmin.z = (*(DWORD*)&p.Dir.z & 0x80000000) ? Max.z : Min.z;

    return (vmin.Dot(p.Dir) > p.Dist);
}

BOOL Bounds::CylinderIntersects(const Vect &center, float radius, float height) const
{
    float radiusAdj = radius+0.01f;
    float heightAdj = height+0.01f;

    if( (center.x > (Max.x + radiusAdj)) ||
        (center.x < (Min.x - radiusAdj)) ||
        (center.y > (Max.y + heightAdj)) ||
        (center.y < (Min.y - heightAdj)) ||
        (center.z > (Max.z + radiusAdj)) ||
        (center.z < (Min.z - radiusAdj)) )
    {
        return 0;
    }

    return 1;
}

BOOL Bounds::RayIntersection(const Vect &rayOrig, const Vect &rayDir, float &fT) const
{
    float tMax = M_INFINITE;
    float tMin = -M_INFINITE;

    Vect center = GetCenter();
    Vect E = Max-center;
    Vect T = center-rayOrig;

    for(int i=0; i<3; i++)
    {
        float e = T.ptr[i];
        float f = rayDir.ptr[i];
        float fI = 1.0f/f;

        if(fabs(f) > 0.0f)
        {
            float t1 = (e+E.ptr[i])*fI;
            float t2 = (e-E.ptr[i])*fI;
            if(t1 > t2)
            {
                if(t2 > tMin) tMin = t2;
                if(t1 < tMax) tMax = t1;
            }
            else
            {
                if(t1 > tMin) tMin = t1;
                if(t2 < tMax) tMax = t2;
            }
            if(tMin > tMax) return FALSE;
            if(tMax < 0.0f) return FALSE;
        }
        else if( ((-e - E.ptr[i]) > 0.0f) ||
                 ((-e + E.ptr[i]) < 0.0f) )
        {
            return FALSE;
        }
    }

    if(tMin > 0.0f)
    {
        fT = tMin;
        return TRUE;
    }
    else
    {
        fT = tMax;
        return TRUE; 
    }
}

BOOL Bounds::RayIntersectionTopDown(const Vect &rayOrig, const Vect &rayDir, float &fT) const
{
    float tMax = M_INFINITE;
    float tMin = -M_INFINITE;

    Vect2 newRayDir(rayDir.x, rayDir.y);
    newRayDir.Norm();

    Vect2 center(((Max.x-Min.x)*0.5f)+Min.x, ((Max.z-Min.z)*0.5f)+Min.z);
    Vect2 E(Max.x-center.x, Max.z-center.y);
    Vect2 T(center.x-rayOrig.x, center.y-rayOrig.z);

    for(int i=0; i<2; i++)
    {
        float e = T.ptr[i];
        float f = newRayDir.ptr[i];
        float fI = 1.0f/f;

        if(fabs(f) > 0.0f)
        {
            float t1 = (e+E.ptr[i])*fI;
            float t2 = (e-E.ptr[i])*fI;
            if(t1 > t2)
            {
                if(t2 > tMin) tMin = t2;
                if(t1 < tMax) tMax = t1;
            }
            else
            {
                if(t1 > tMin) tMin = t1;
                if(t2 < tMax) tMax = t2;
            }
            if(tMin > tMax) return FALSE;
            if(tMax < 0.0f) return FALSE;
        }
        else if( ((-e - E.ptr[i]) > 0.0f) ||
                 ((-e + E.ptr[i]) < 0.0f) )
        {
            return FALSE;
        }
    }

    if(tMin > 0.0f)
    {
        fT = tMin;
        return TRUE;
    }
    else
    {
        fT = tMax;
        return TRUE; 
    }
}

BOOL Bounds::LineIntersection(const Vect &v1, const Vect &v2, float &fT) const
{
    float tMax = M_INFINITE;
    float tMin = -M_INFINITE;

    Vect rayVect = (v2-v1);
    float rayLength = rayVect.Len();
    Vect rayDir = rayVect*(1.0f/rayLength);

    Vect center = GetCenter();
    Vect E = Max-center;
    Vect T = center-v1;

    for(int i=0; i<3; i++)
    {
        float e = T.ptr[i];
        float f = rayDir.ptr[i];
        float fI = 1.0f/f;

        if(fabs(f) > 0.0f)
        {
            float t1 = (e+E.ptr[i])*fI;
            float t2 = (e-E.ptr[i])*fI;
            if(t1 > t2)
            {
                if(t2 > tMin) tMin = t2;
                if(t1 < tMax) tMax = t1;
            }
            else
            {
                if(t1 > tMin) tMin = t1;
                if(t2 < tMax) tMax = t2;
            }
            if(tMin > tMax) return FALSE;
            if(tMax < 0.0f) return FALSE;
        }
        else if( ((-e - E.ptr[i]) > 0.0f) ||
                 ((-e + E.ptr[i]) < 0.0f) )
        {
            return FALSE;
        }

        if(tMin > rayLength) return FALSE;
    }

    if(tMin > 0.0f)
    {
        fT = (tMin/rayLength);
        return TRUE;
    }
    else
    {
        fT = (tMax/rayLength);
        return TRUE; 
    }
}

BOOL Bounds::LineIntersectionTopDown(const Vect &v1, const Vect &v2, float &fT) const
{
    float tMax = M_INFINITE;
    float tMin = -M_INFINITE;

    Vect2 rayVect(v2.x-v1.x, v2.z-v1.z);
    float rayLength = rayVect.Len();
    Vect2 rayDir = rayVect*(1.0f/rayLength);

    Vect2 center(((Max.x-Min.x)*0.5f)+Min.x, ((Max.z-Min.z)*0.5f)+Min.z);
    Vect2 E(Max.x-center.x, Max.z-center.y);
    Vect2 T(center.x-v1.x, center.y-v1.z);

    for(int i=0; i<2; i++)
    {
        float e = T.ptr[i];
        float f = rayDir.ptr[i];
        float fI = 1.0f/f;

        if(fabs(f) > 0.0f)
        {
            float t1 = (e+E.ptr[i])*fI;
            float t2 = (e-E.ptr[i])*fI;
            if(t1 > t2)
            {
                if(t2 > tMin) tMin = t2;
                if(t1 < tMax) tMax = t1;
            }
            else
            {
                if(t1 > tMin) tMin = t1;
                if(t2 < tMax) tMax = t2;
            }
            if(tMin > tMax) return FALSE;
            if(tMax < 0.0f) return FALSE;
        }
        else if( ((-e - E.ptr[i]) > 0.0f) ||
                 ((-e + E.ptr[i]) < 0.0f) )
        {
            return FALSE;
        }

        if(tMin > rayLength) return FALSE;
    }

    if(tMin > 0.0f)
    {
        fT = (tMin/rayLength);
        return TRUE;
    }
    else
    {
        fT = (tMax/rayLength);
        return TRUE; 
    }
}


float Bounds::MinDistFrom(const Plane &p) const
{
    float vecLen = VectorOffsetLength(p.Dir)*0.5f;
    float centerDist = GetCenter().DistFromPlane(p);

    return (p.Dist+centerDist)-vecLen;
}

Bounds& Bounds::MakeFromPoints(const List<Vect> &pointList)
{
    if(!pointList.Num()) return *this;

    Min = pointList[0];
    Max = pointList[0];
    for(unsigned int i=1; i<pointList.Num(); i++)
    {
        Min.ClampMax(pointList[i]);
        Max.ClampMin(pointList[i]);
    }

    return *this;
}


/*=========================================================
    Plane
==========================================================*/

Plane& Plane::Transform(const Matrix &m)
{
    Dir.TransformVector(m).Norm();
    Dist -= Dir.Dot(m.T.GetTransformedVector(m));
    return *this;
}

void Plane::CreateFromTri(const Vect &a, const Vect &b, const Vect &c)
{
    Dir = ((b-a)^(c-a)).Norm();
    Dist = (a | Dir);
}

BOOL Plane::GetRayIntersection(const Vect &rayOrigin, const Vect &rayDir, float &fT) const
{
    float c0 = Dir | rayDir;

    fT = 0;

    if(fabs(c0) < EPSILON)
        return 0;

    fT = ((Dist - (Dir | rayOrigin)) / c0);

    return 1;
}

BOOL Plane::GetIntersection(const Vect &p1, const Vect &p2, float &fT) const
{
    float P1Dist = p1.DistFromPlane(*this);
    float P2Dist = p2.DistFromPlane(*this);

    if(CloseFloat(P1Dist, 0.0f, EPSILON))
    {
        if(P2Dist == 0.0f)
            return 0;

        fT = 0.0f;
        return 1;
    }
    else if(CloseFloat(P2Dist, 0.0f, EPSILON))
    {
        fT = 1.0f;
        return 1;
    }

    BOOL bP1Over = (P1Dist > 0.0f);
    BOOL bP2Over = (P2Dist > 0.0f);

    if(bP1Over == bP2Over)
        return FALSE;

    float P1AbsDist = fabs(P1Dist);
    float dist2 = (P1AbsDist+fabs(P2Dist));
    if(dist2 < EPSILON)
        return FALSE;
    fT = P1AbsDist/dist2;

    return TRUE;
}

BOOL Plane::GetDoublePlaneIntersection(const Plane &p2, Vect &intOrigin, Vect &intDir) const
{
    float fCosAngle = (Dir|p2.Dir);
    float fSinAngle = sqrt(1.0f-(fCosAngle*fCosAngle));

    if(fabs(fSinAngle) < LARGE_EPSILON)
        return FALSE;

    float fInvSinAngle = 1.0f/fSinAngle;

    float fChi1 = (Dist-(fCosAngle*p2.Dist))*fInvSinAngle;
    float fChi2 = (p2.Dist-(fCosAngle*Dist))*fInvSinAngle;

    intDir      = Dir^p2.Dir;
    float iLen  = 1.0f/intDir.Len();
    intDir     *= iLen;

    intOrigin   = (Dir*fChi1) + (p2.Dir*fChi2);
    intOrigin  *= iLen;

    return TRUE;
}

BOOL Plane::GetTriplePlaneIntersection(const Plane &p2, const Plane &p3, Vect &intersection) const
{
    Vect rayOrigin, rayDir;

    if(GetDoublePlaneIntersection(p2, rayOrigin, rayDir))
    {
        float fT;
        if(p3.GetRayIntersection(rayOrigin, rayDir, fT))
        {
            intersection = rayOrigin + (rayDir*fT);
            return TRUE;
        }
    }

    return FALSE;
}

/*=========================================================
    Matrix (3x4)
==========================================================*/

void Matrix::CreateFromQuat(const Quat &q)
{
    float norm = q | q;
    float s = (norm > 0) ? 2/norm : 0;

    float xx = q.x * q.x * s;
    float yy = q.y * q.y * s;
    float zz = q.z * q.z * s;
    float xy = q.x * q.y * s;
    float xz = q.x * q.z * s;
    float yz = q.y * q.z * s;
    float wx = q.w * q.x * s;
    float wy = q.w * q.y * s;
    float wz = q.w * q.z * s;


    X.x = 1.0f - (yy + zz);
    X.y = xy + wz;
    X.z = xz - wy;

    Y.x = xy - wz;
    Y.y = 1.0f - (xx + zz);
    Y.z = yz + wx;

    Z.x = xz + wy;
    Z.y = yz - wx;
    Z.z = 1.0f - (xx + yy);
}


Matrix Matrix::GetInverse() const
{
    return Matrix(*this).Inverse();
}

Matrix& Matrix::Inverse()
{
    float m[16], cur[16];

    Matrix4x4Convert(cur, *this);
    Matrix4x4Inverse(m, cur);
    Matrix4x4Convert(*this, m);

    return *this;
}

/*=========================================================
    Line2
==========================================================*/

BOOL Line2::LinesIntersect(const Line2 &collider) const
{
    //this looks more complex than it really is.  I'm just an optimization freak.
    //...okay, this probably isn't the kind of function that's called every frame or something.
    //but I'm compulsive about optimization!  ..okay, I'll make it cleaner one of these days.

    Vect2 vec = (A-B);
    float len = vec.Len();
    Vect2 norm = (vec/len);
    Vect2 cross = norm.GetCross();
    float dist = cross.Dot(A);

    float fADist = collider.A.Dot(cross)-dist;
    float fBDist = collider.B.Dot(cross)-dist;

    BOOL bAAbove = (fADist > -EPSILON);
    BOOL bBAbove = (fBDist > -EPSILON);

    BOOL bFoundZeroLine1 = FALSE;

    if( (fabs(fADist) < EPSILON) ||
        (fabs(fBDist) < EPSILON) )
    {
        bFoundZeroLine1 = TRUE;
    }

    if(bAAbove == bBAbove)
        return FALSE;

    vec = (collider.A-collider.B);
    len = vec.Len();
    norm = (vec/len);
    cross = norm.GetCross();
    dist = cross.Dot(collider.A);

    fADist = A.Dot(cross)-dist;
    fBDist = B.Dot(cross)-dist;

    bAAbove = (fADist > -EPSILON);
    bBAbove = (fBDist > -EPSILON);

    if(fabs(fADist) < EPSILON)
        bAAbove = bBAbove;
    if(fabs(fBDist) < EPSILON)
        bBAbove = bAAbove;

    BOOL bFoundZeroLine2 = FALSE;

    if( (fabs(fADist) < EPSILON) ||
        (fabs(fBDist) < EPSILON) )
    {
        bFoundZeroLine2 = TRUE;
    }

    if(bAAbove == bBAbove)
        return FALSE;

    if(bFoundZeroLine1 && bFoundZeroLine2)
        return FALSE;

    return TRUE;
}



/*=========================================================
    Other
==========================================================*/

Vect GetHSpline(const Vect &v0, const Vect &v1, const Vect &m0, const Vect &m1, float fT)
{
    float fT2 = fT*fT;
    float fT3 = fT2*fT;

    float fT2_2 = fT2*2;
    float fT2_3 = fT2*3;
    float fT3_2 = fT3*2;

    return Vect((v0*(fT3_2-fT2_3+1)) + (m0*(fT3-fT2_2+fT)) + (m1*(fT3-fT2)) + (v1*(-fT3_2 + fT2_3)));
}

BOOL SphereRayCollision(const Vect &center, float radius, const Vect &rayOrig, const Vect &rayDir, Vect *collision, Plane *collisionPlane)
{
    //the standard algorithm
    Vect l   = (center-rayOrig);
    float d  = l | rayDir;          //distance from rayDir Plane

    if(d < radius)
        return FALSE;

    float l2 = l | l;               //c-o distance squared
    float r2 = radius*radius;

    if(l2 < r2)                     //if inside the sphere, fail
        return FALSE;

    if((d < 0.0f) && (l2 > r2))     //if the plane distance is negative, and
                                    //the distance is over the radius, fail
        return FALSE;

    float m2 = l2 - (d*d);          //distance from the sphere center to the
                                    //closest ray point

    if(m2 > r2) return FALSE;       //if m2 is larger than the radius, fail

    float q = sqrt(r2-m2);          //real distance from the edge of the
                                    //sphere to the cloest ray point
                                    //(forms a sort of triangle)
    float fT;

    fT = (l2 > r2) ? (d-q) : (d+q); //if the distance is over the radius,
                                    //d-q = T, else d+q=T

    Vect collisionValue = rayOrig+(rayDir*fT);

    if(collision)
        *collision = collisionValue;

    if(collisionPlane)
    {
        collisionPlane->Dir  = (collisionValue-center).Norm();
        collisionPlane->Dist = collisionPlane->Dir|collisionValue;
    }

    return TRUE;
}

// yes, I realize this breaks on caps when the ray origin is inside the cylinder.
// it's not like it's actually going to be in there anyway.
BOOL CylinderRayCollision(const Vect &center, float radius, float height, const Vect &rayOrig, const Vect &rayDir, Vect *collision, Plane *collisionPlane)
{
    Vect collisionValue;
    BOOL bHit = FALSE;
    float fT;

    Plane axisPlane(0.0f, 1.0f, 0.0f, center.y);

    //---------------------------------------
    // test the cap
    if(fabs(rayDir.y) > EPSILON)
    {
        BOOL bUnder = (rayDir.y<0.0f);

        Plane planeCap;
        planeCap.Dir.Set(0.0f, bUnder ? 1.0f : -1.0f, 0.0f);
        planeCap.Dist = (bUnder ? center.y : -center.y)+height;

        if(rayOrig.DistFromPlane(planeCap) > 0.0f)
        {
            if(planeCap.GetRayIntersection(rayOrig, rayDir, fT))
            {
                collisionValue = rayOrig+(rayDir*fT);
                Vect CapCenter = center+(planeCap.Dir*height);

                if(collisionValue.Dist(CapCenter) <= radius)
                {
                    if(collisionPlane)
                        *collisionPlane = planeCap;
                    bHit = TRUE;
                }
            }
        }
    }

    if(!bHit && ((1.0f-fabs(rayDir.y)) > EPSILON))
    {
        //---------------------------------------
        // test the body
        Vect adjDir, adjCenter;

        adjDir.Set(rayDir.x, 0.0f, rayDir.z).Norm();
        adjCenter.Set(center.x, rayOrig.y, center.z);

        Vect l   = (adjCenter-rayOrig);
        float d  = l | adjDir;          //distance from adjDir Plane
        float l2 = l | l;               //c-o distance squared
        float r2 = radius*radius;

        if(l2 < r2)                     //if inside the cylinder, fail
            return FALSE;

        if((d < 0.0f) && (l2 > r2))     //if the plane distance is negative, and
            return FALSE;               //the distance is over the radius, fail

        float m2 = l2 - (d*d);          //distance from the cylinder center to
                                        //the closest ray point

        if(m2 > r2)                     //if m2 is larger than the radius, fail
            return FALSE;

        float q = sqrt(r2-m2);          //real distance from the edge of the
                                        //cylinder to the cloest ray point
                                        //(forms a sort of triangle)

        fT = (l2 > r2) ? (d-q) : (d+q); //if the distance is over the radius,
                                        //d-q = T, else d+q=T
                                        //distance

        fT /= (adjDir|rayDir);          //divide by angle to get the proper
                                        //value

        collisionValue = rayOrig+(rayDir*fT);

        if(fabs(collisionValue.DistFromPlane(axisPlane)) >= height)
            return FALSE;

        if(collisionPlane)
        {
            Vect temp = collisionValue;
            temp.y = center.y;

            collisionPlane->Dir  = (temp-center).Norm();
            collisionPlane->Dist = collisionPlane->Dir|temp;
        }

        bHit = TRUE;
    }

    if(!bHit)
        return FALSE;

    if(collision)
        *collision = collisionValue;

    return TRUE;
}


BOOL PointOnFiniteLine(const Vect &lineV1, const Vect &lineV2, const Vect &p)
{
    Vect line = lineV2-lineV1;
    float lineDist = line.Len();

    Plane plane;
    plane.Dir  = line*(1.0f/lineDist);
    plane.Dist = plane.Dir.Dot(lineV1);

    float dist = p.DistFromPlane(plane);
    if((dist < 0.0f) || (dist > lineDist))
        return FALSE;

    float fT = (dist/lineDist);

    return p.CloseTo(Lerp(lineV1, lineV2, fT));
}


BOOL ClosestLinePoint(const Vect &ray1Origin, const Vect &ray1Dir, const Vect &ray2Origin, const Vect &ray2Dir, float &fT)
{
    Vect W = (ray1Origin-ray2Origin);

    float a = ray1Dir|ray1Dir;
    float b = ray1Dir|ray2Dir;
    float c = ray2Dir|ray2Dir;
    float d = ray1Dir|W;
    float e = ray2Dir|W;

    float c0 = (a*c)-(b*b);

    if(c0 < EPSILON)
        return FALSE;

    float c1 = (b*e)-(c*d);

    fT = c1/c0;

    return TRUE;
}



BOOL ClosestLinePoints(const Vect &ray1Origin, const Vect &ray1Dir, const Vect &ray2Origin, const Vect &ray2Dir, float &fT1, float &fT2)
{
    Vect W = (ray1Origin-ray2Origin);

    float a = ray1Dir|ray1Dir;
    float b = ray1Dir|ray2Dir;
    float c = ray2Dir|ray2Dir;
    float d = ray1Dir|W;
    float e = ray2Dir|W;

    float c0 = (a*c)-(b*b);

    if(c0 < EPSILON)
        return FALSE;

    float c1 = (b*e)-(c*d);
    float c2 = (a*e)-(b*d);

    fT1 = c1/c0;
    fT2 = c2/c0;

    return TRUE;
}


Vect CartToPolar(const Vect &cart)
{
    Vect polar;

    polar.z = cart.Len();

    if(CloseFloat(polar.z, 0.0f, EPSILON))
    {
        polar.x = 0.0f;
        polar.y = 0.0f;
        polar.z = 0.0f;
    }
    else
    {
        polar.x = asin(cart.y / polar.z);
        polar.y = atan2(cart.x, cart.z);
    }

    return polar;
}

Vect PolarToCart(const Vect &polar)
{
    Vect cart;

    float sinx = cos(polar.x);

    cart.x = polar.z * sinx * sin(polar.y);
    cart.z = polar.z * sinx * cos(polar.y);
    cart.y = polar.z * sin(polar.x);

    return cart;
}

Vect PolarToCart(float latitude, float longitude, float dist)
{
    Vect cart;

    float sinx = cos(latitude);

    cart.x = dist * sinx * sin(longitude);
    cart.z = dist * sinx * cos(longitude);
    cart.y = dist * sin(latitude);

    return cart;
}


Vect2 NormToPolar(const Vect &norm)
{
    Vect2 polar;

    polar.x = atan2(norm.x, norm.z);
    polar.y = asin(norm.y);

    return polar;
}

void NormToPolar(const Vect &norm, float &latitude, float &longitude)
{
    longitude = atan2(norm.x, norm.z);
    latitude = acos(norm.y);
}

Vect PolarToNorm(const Vect2 &polar)
{
    Vect norm;

    float sinx = sin(polar.x);

    norm.x = sinx * cos(polar.y);
    norm.y = sinx * sin(polar.y);
    norm.z = cos(polar.x);

    return norm;
}


float GetPlaneCylinderOffset(const Vect &dir, float cylHalfHeight, float cylRadius)
{
    Vect2 dir2(sqrtf(1.0f-(dir.y*dir.y)), fabs(dir.y));
    Vect2 centerOffset(cylRadius, cylHalfHeight);

    return centerOffset.Dot(dir2);
}

float GetPlaneCapsuleOffset(const Vect &dir, float capsuleHalfHeight, float capsuleRadius)
{
    Vect2 dir2(sqrtf(1.0f-(dir.y*dir.y)), fabs(dir.y));
    Vect2 centerOffset(capsuleRadius, capsuleHalfHeight);

    return centerOffset.Dot(dir2)-(1.0f-MAX(dir2.x, dir2.y));
}

float GetPlaneConeOffset(const Vect &dir, float coneHeight, float coneRadius)
{
    Vect2 dir2(sqrtf(1.0f-(dir.y*dir.y)), dir.y);
    Vect2 p1(coneRadius, coneHeight), p2(coneRadius, 0.0f);

    float dist1 = dir2.Dot(p1);
    float dist2 = dir2.Dot(p2);

    return MAX(dist1, dist2);
}


float RandomFloat(BOOL bPositiveOnly)
{
    if(bPositiveOnly)
        return (float)(((double)rand())/32767.0);
    else
        return (float)(((((double)rand())/32767.0)*2.0)-1.0);
}

Vect RandomVect(BOOL bPositiveOnly)
{
    return Vect(RandomFloat(bPositiveOnly), RandomFloat(bPositiveOnly), RandomFloat(bPositiveOnly)).Norm();
}


void CalcTorque(float &val1, float val2, float torque, float minAdjust, float fT)
{
    if(val1 == val2)
        return;

    float dist = (val2-val1)*torque;            //uses distance to determine torque speed

    BOOL bOver = dist > 0.0f;

    if(bOver)
    {
        if(dist < minAdjust) dist = minAdjust;  //prevents torque from going too slow
        val1 += dist*fT;                        //adds toque to val1
        if(val1 > val2) val1 = val2;            //sets val1 to val2 if overshot with the minimum torque value
    }
    else
    {
        if(dist > -minAdjust) dist = -minAdjust;
        val1 += dist*fT;
        if(val1 < val2) val1 = val2;
    }
}

//vector version
void CalcTorque(Vect &val1, const Vect &val2, float torque, float minAdjust, float fT)
{
    if(val1 == val2)
        return;

    Vect line = (val2-val1);
    float origDist = line.Len();
    Vect dir = line * (1.0f/origDist);

    float torqueDist = origDist*torque;                 //uses distance to determine torque speed

    float adjustDist = torqueDist*fT;

    if(torqueDist < minAdjust) torqueDist = minAdjust;  //prevents torque from going too slow

    if(adjustDist <= (origDist-LARGE_EPSILON))
        val1 += dir*adjustDist;                         //adds toque to val1
    else
        val1 = val2;                                    //sets val1 to val2 if overshot with the minimum torque value
}


void Matrix4x4Identity(float *destMatrix)
{
    float matrixOut[4][4];

    zero(matrixOut, 64);

    matrixOut[0][0] = matrixOut[1][1] = matrixOut[2][2] = matrixOut[3][3] = 1.0f;

    mcpy(destMatrix, matrixOut, 64);
}

void Matrix4x4Convert(float *destMatrix, const Matrix &mat)
{
    mcpy(destMatrix, &mat, sizeof(Matrix));

    destMatrix[15] = 1.0f;
}

void Matrix4x4Convert(Matrix &mat, float *m4x4)
{
    mcpy(&mat, m4x4, sizeof(Matrix));
}


void  Matrix4x4Multiply(float *destMatrix, float *M1, float *M2)
{
    assert(M1 && M2);

    int i,j;
    float matrixOut[4][4];

    for(i=0; i<4; i++)
    {
        int pos = i*4;
        Vect4 row(M1[pos], M1[pos+1], M1[pos+2], M1[pos+3]);

        for(j=0; j<4; j++)
        {
            Vect4 column(M2[j], M2[j+4], M2[j+8], M2[j+12]);

            matrixOut[i][j] = row | column;
        }
    }

    mcpy(destMatrix, matrixOut, 64);
}

void  Matrix4x4TransformVect(Vect4 &out, float *M1, const Vect4 &vect)
{
    int i;
    Vect4 vectOut;

    for(i=0; i<4; i++)
    {
        int pos = i*4;
        Vect4 row(M1[pos], M1[pos+1], M1[pos+2], M1[pos+3]);

        vectOut.ptr[i] = row|vect;
    }

    mcpy(out.ptr, vectOut.ptr, 16);
}

void Matrix4x4Transpose(float *destMatrix, float *srcMatrix)
{
    float matrixOut[4][4];

    matrixOut[0][0] = srcMatrix[0];
    matrixOut[1][0] = srcMatrix[1];
    matrixOut[2][0] = srcMatrix[2];
    matrixOut[3][0] = srcMatrix[3];
    matrixOut[0][1] = srcMatrix[4];
    matrixOut[1][1] = srcMatrix[5];
    matrixOut[2][1] = srcMatrix[6];
    matrixOut[3][1] = srcMatrix[7];
    matrixOut[0][2] = srcMatrix[8];
    matrixOut[1][2] = srcMatrix[9];
    matrixOut[2][2] = srcMatrix[10];
    matrixOut[3][2] = srcMatrix[11];
    matrixOut[0][3] = srcMatrix[12];
    matrixOut[1][3] = srcMatrix[13];
    matrixOut[2][3] = srcMatrix[14];
    matrixOut[3][3] = srcMatrix[15];

    mcpy(destMatrix, matrixOut, 64);
}

float Matrix3x3Determinant(float *M1)
{
    float det;

    det = (M1[0] * ((M1[4]*M1[8]) - (M1[7]*M1[5])))
        - (M1[1] * ((M1[3]*M1[8]) - (M1[6]*M1[5])))
        + (M1[2] * ((M1[3]*M1[7]) - (M1[6]*M1[4])));

    return det;
}


float Matrix4x4Determinant(float *M1)
{
    float  det, result = 0.0f, i = 1.0f;
    float  msub3[9];
    int    n;

    for(n = 0; n < 4; n++, i *= -1.0f)
    {
        Matrix4x4SubMatrix(msub3, M1, 0, n);

        det     = Matrix3x3Determinant(msub3);
        result += (M1[n] * det * i);
    }

    return result;
}

void Matrix4x4SubMatrix(float *destMatrix, float *M1, int i, int j)
{
    int ti, tj, idst, jdst;

    for (ti = 0; ti < 4; ti++)
    {
        if(ti < i)
            idst = ti;
        else if(ti > i)
            idst = ti-1;

        for(tj = 0; tj < 4; tj++)
        {
            if(tj < j)
                jdst = tj;
            else if(tj > j)
                jdst = tj-1;

            if((ti != i) && (tj != j))
                destMatrix[(idst*3) + jdst] = M1[(ti*4) + tj];
        }
    }
}

BOOL Matrix4x4Inverse(float *destMatrix, float *M1)
{
    float  mdet = Matrix4x4Determinant(M1);
    float  mtemp[9];
    int    i, j, sign;

    if(fabs(mdet) < 0.0005f)
        return 0;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            sign = 1 - ((i+j) % 2) * 2;

            Matrix4x4SubMatrix(mtemp, M1, i, j);

            destMatrix[i+(j*4)] = (Matrix3x3Determinant(mtemp) * sign) / mdet;
        }
    }

    return 1;
}

void Matrix4x4Ortho(float *destMatrix, double left, double right, double bottom, double top, double near, double far)
{
    float matrixOut[4][4];
    zero(matrixOut, 64);

    matrixOut[0][0] =          float(2.0 / (right-left));
    matrixOut[3][0] = float((left+right) / (left-right));

    matrixOut[1][1] =          float(2.0 / (top-bottom));
    matrixOut[3][1] = float((bottom+top) / (bottom-top));

    matrixOut[2][2] =          float(1.0 / (far-near));
    matrixOut[3][2] =         float(near / (near-far));

    matrixOut[3][3] = 1.0f;

    mcpy(destMatrix, matrixOut, 64);
}

void Matrix4x4Frustum(float *destMatrix, double left, double right, double bottom, double top, double near, double far)
{
    float matrixOut[4][4];
    zero(matrixOut, 64);

    matrixOut[0][0] =   float((2.0*near) / (right-left));
    matrixOut[2][0] = float((left+right) / (left-right));

    matrixOut[1][1] =   float((2.0*near) / (top-bottom));
    matrixOut[2][1] = float((top+bottom) / (bottom-top));

    matrixOut[2][2] =          float(far / (far-near));
    matrixOut[3][2] =   float((near*far) / (near-far));

    matrixOut[2][3] = 1.0f;

    mcpy(destMatrix, matrixOut, 64);
}

void Matrix4x4Perspective(float *destMatrix, float angle, float aspect, float near, float far)
{
   float xmin, xmax, ymin, ymax;

   ymax = near * tanf(RAD(angle)*0.5f);
   ymin = -ymax;

   xmin = ymin * aspect;
   xmax = ymax * aspect;

   Matrix4x4Frustum(destMatrix, xmin, xmax, ymin, ymax, near, far);
}

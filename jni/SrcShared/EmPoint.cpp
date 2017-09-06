/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2000-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#include "EmCommon.h"
#include "EmPoint.h"

// ----------------------------------------------------------------------

template <class coord>
void
EmPointTempl<coord>::Offset (coord x, coord y)
{
	fX += x;
	fY += y;
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmPointTempl<coord>::operator+ (const EmPointTempl<coord>& pt) const
{
	EmPointTempl<coord>	result;

	result.fX = this->fX + pt.fX;
	result.fY = this->fY + pt.fY;

	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmPointTempl<coord>::operator- (const EmPointTempl<coord>& pt) const
{
	EmPointTempl<coord>	result;

	result.fX = this->fX - pt.fX;
	result.fY = this->fY - pt.fY;

	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmPointTempl<coord>::operator- (void) const
{
	EmPointTempl<coord>	result;

	result.fX = -this->fX;
	result.fY = -this->fY;

	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>&
EmPointTempl<coord>::operator+= (const EmPointTempl<coord>& pt)
{
	fX += pt.fX;
	fY += pt.fY;

	return *this;
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>&
EmPointTempl<coord>::operator-= (const EmPointTempl<coord>& pt)
{
	fX -= pt.fX;
	fY -= pt.fY;

	return *this;
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmPointTempl<coord>::operator* (const EmPointTempl<coord>& pt) const
{
	EmPointTempl<coord>	result;

	result.fX = this->fX * pt.fX;
	result.fY = this->fY * pt.fY;

	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmPointTempl<coord>::operator/ (const EmPointTempl<coord>& pt) const
{
	EmPointTempl<coord>	result;

	EmAssert (pt.fX != 0);
	EmAssert (pt.fY != 0);

	result.fX = this->fX / pt.fX;
	result.fY = this->fY / pt.fY;

	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>&
EmPointTempl<coord>::operator*= (const EmPointTempl<coord>& pt)
{
	fX *= pt.fX;
	fY *= pt.fY;

	return *this;
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>&
EmPointTempl<coord>::operator/= (const EmPointTempl<coord>& pt)
{
	EmAssert (pt.fX != 0);
	EmAssert (pt.fY != 0);

	fX /= pt.fX;
	fY /= pt.fY;

	return *this;
}

// ----------------------------------------------------------------------

template <class coord>
Boolean
EmPointTempl<coord>::operator!= (const EmPointTempl<coord>& pt) const
{
	return
		(fX != pt.fX) ||
		(fY != pt.fY);
}

// ----------------------------------------------------------------------

template <class coord>
Boolean
EmPointTempl<coord>::operator== (const EmPointTempl<coord>& pt) const
{
	return
		(fX == pt.fX) &&
		(fY == pt.fY);
}


// ----------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(disable: 4660)  // template-class specialization is already instatiated
#endif

template class EmPointTempl<EmCoord>;


// ----------------------------------------------------------------------
//	Psuedo-type definitions to aid in the conversion process without
//	actually having to #include the requisite headers.
// ----------------------------------------------------------------------

#ifndef _WINDEF_
struct	tagSIZE			{ char	_[8]; };
struct	tagPOINT		{ char	_[8]; };
#endif

#ifndef __AFXWIN_H__
class	CSize			{ char	_[8]; };
class	CPoint			{ char	_[8]; };
#endif

#ifndef __MACTYPES__
struct	Point			{ char	_[4]; };
#endif

#ifndef _H_LPane
struct	SDimension16	{ char	_[4]; };
struct	SDimension32	{ char	_[8]; };
struct	SPoint32		{ char	_[8]; };
#endif
struct	VPoint			{ char	_[8]; };


// ----------------------------------------------------------------------
//	* EmPoint
// ----------------------------------------------------------------------

//
// Construct from another kind of point
// Assigned from another kind of point
//
	#undef FOR_POINT
	#define FOR_POINT(cls, size, x, y)										\
		EmPoint::EmPoint(const cls& pt) :									\
			EmPointTempl<EmCoord>(((size*)&pt)[x], ((size*)&pt)[y]) {}		\
		const EmPoint& EmPoint::operator=(const cls& pt)					\
			{ fX = ((size*)&pt)[x]; fY = ((size*)&pt)[y]; return *this; }

	POINT_LIST_XY_SHORT
	POINT_LIST_XY_LONG
	POINT_LIST_YX_SHORT
	POINT_LIST_YX_LONG

//
// Return another kind of point
//
	#undef FOR_POINT
	#ifndef BROKEN_RETURN_CASTING
	#define FOR_POINT(cls, size, x, y)		\
		EmPoint::operator cls() const		\
			{ size pt[2]; pt[x] = fX; pt[y] = fY; return *(cls*) pt; }
	#else
	#define FOR_POINT(cls, size, x, y)		\
		EmPoint::operator cls() const		\
			{ union {size pt[2]; cls c;} u;	\
			  u.pt[x] = fX; u.pt[y] = fY; return u.c; }
	#endif

	POINT_LIST_XY_LONG
	POINT_LIST_YX_LONG
	POINT_LIST_XY_SHORT
	POINT_LIST_YX_SHORT

//
// Return a [const] (* | &) to another kind of point
//
	#undef FOR_POINT
	#define FOR_POINT(cls, size, x, y)			\
		EmPoint::operator const cls*() const	\
			{ return (const cls*) this; }		\
		EmPoint::operator cls*()				\
			{ return (cls*) this; }				\
		EmPoint::operator const cls&() const	\
			{ return *(const cls*) this; }		\
		EmPoint::operator cls&()				\
			{ return *(cls*) this; }

	POINT_LIST_LONG

//#define TESTING
#ifdef TESTING
// ----------------------------------------------------------------------
#include "stdio.h"
void TestPoint();
void TestPoint()
{
	printf("Testing EmPointTempl class...\n");

	{
		EmPoint	pt1(1, 2);
		EmAssert(pt1.fX == 1);
		EmAssert(pt1.fY == 2);

		EmPoint	pt2(5, 6);
		EmPoint	pt3;
		pt3 = pt2;
		EmAssert(pt3.fX == 5);
		EmAssert(pt3.fY == 6);

		EmPoint	pt4(pt2);
		EmAssert(pt4.fX == 5);
		EmAssert(pt4.fY == 6);

		pt4.Offset(-5, -50);
		EmAssert(pt4.fX == 5 - 5);
		EmAssert(pt4.fY == 6 - 50);

		EmPoint	pt5 = pt1 + pt2;
		EmAssert(pt5.fX == 1 + 5);
		EmAssert(pt5.fY == 2 + 6);

		EmPoint	pt6 = pt1 - pt2;
		EmAssert(pt6.fX == 1 - 5);
		EmAssert(pt6.fY == 2 - 6);

		EmPoint	pt7 = -pt2;
		EmAssert(pt7.fX == -5);
		EmAssert(pt7.fY == -6);

		EmPoint	pt8(10, 11);
		pt8 += pt2;
		EmAssert(pt8.fX == 10 + 5);
		EmAssert(pt8.fY == 11 + 6);

		EmPoint	pt9(10, 11);
		pt9 -= pt2;
		EmAssert(pt9.fX == 10 - 5);
		EmAssert(pt9.fY == 11 - 6);

		EmAssert(pt9 != pt7);
		EmAssert(pt2 == pt2);
		EmAssert(pt3 == pt2);
		
		// !!! Need to test all the constructors and conversion operators
	}
}
#endif

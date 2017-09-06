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
#include "EmRect.h"

// ----------------------------------------------------------------------
//	* Helper functions
// ----------------------------------------------------------------------

template <class coord>
inline
coord Min (coord a, coord b) { return a < b ? a : b; }

template <class coord>
inline
coord Max (coord a, coord b) { return a > b ? a : b; }


// ----------------------------------------------------------------------

template <class coord>
coord
EmRectTempl<coord>::Width (void) const
{
	return fRight - fLeft;
}

// ----------------------------------------------------------------------

template <class coord>
coord
EmRectTempl<coord>::Height (void) const
{
	return fBottom - fTop;
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmRectTempl<coord>::Size (void) const
{
	return EmPointTempl<coord> (Width (), Height ());
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmRectTempl<coord>::TopLeft (void) const
{
	return EmPointTempl<coord> (fLeft, fTop);
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmRectTempl<coord>::TopRight (void) const
{
	return EmPointTempl<coord> (fRight, fTop);
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmRectTempl<coord>::BottomLeft (void) const
{
	return EmPointTempl<coord> (fLeft, fBottom);
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmRectTempl<coord>::BottomRight (void) const
{
	return EmPointTempl<coord> (fRight, fBottom);
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmRectTempl<coord>::North (void) const
{
	return EmPointTempl<coord> ((fLeft + fRight) / 2, fTop);
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmRectTempl<coord>::South (void) const
{
	return EmPointTempl<coord> ((fLeft + fRight) / 2, fBottom);
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmRectTempl<coord>::East (void) const
{
	return EmPointTempl<coord> (fRight, (fTop + fBottom) / 2);
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmRectTempl<coord>::West (void) const
{
	return EmPointTempl<coord> (fLeft, (fTop + fBottom) / 2);
}

// ----------------------------------------------------------------------

template <class coord>
EmPointTempl<coord>
EmRectTempl<coord>::Center (void) const
{
	return EmPointTempl<coord> ((fLeft + fRight) / 2, (fTop + fBottom) / 2);
}

// ----------------------------------------------------------------------

template <class coord>
Bool
EmRectTempl<coord>::IsEmpty (void) const
{
	return fRight <= fLeft || fBottom <= fTop;
}

// ----------------------------------------------------------------------

template <class coord>
Bool
EmRectTempl<coord>::IsNull (void) const
{
	return fTop == 0 && fLeft == 0 && fBottom == 0 && fRight == 0;
}

// ----------------------------------------------------------------------

template <class coord>
Bool
EmRectTempl<coord>::IsEqual (const EmRectTempl<coord>& other) const
{
	return
		fTop == other.fTop &&
		fLeft == other.fLeft &&
		fBottom == other.fBottom &&
		fRight == other.fRight;
}

// ----------------------------------------------------------------------

template <class coord>
Bool
EmRectTempl<coord>::Contains(const EmPointTempl<coord>& pt) const
{
	return
		pt.fY >= fTop &&
		pt.fY < fBottom &&
		pt.fX >= fLeft &&
		pt.fX < fRight;
}

// ----------------------------------------------------------------------

template <class coord>
Bool
EmRectTempl<coord>::Contains (const EmRectTempl<coord>& rt) const
{
	return rt.fTop >= fTop &&
		rt.fTop <= fBottom &&
		rt.fLeft >= fLeft &&
		rt.fLeft <= fRight &&
		rt.fBottom >= fTop &&
		rt.fBottom <= fBottom &&
		rt.fRight >= fLeft &&
		rt.fRight <= fRight;
}

// ----------------------------------------------------------------------

template <class coord>
Bool
EmRectTempl<coord>::Intersects (const EmRectTempl<coord>& rt) const
{
#if 0
	//
	// Different semantics we might want to consider.
	//
	if (IsEmpty() || rt.IsEmpty())
		return false;
#endif

	return
		(rt.fTop < this->fBottom && rt.fBottom > this->fTop) &&
		(rt.fLeft < this->fRight && rt.fRight > this->fLeft);
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::Set (coord left, coord top, coord right, coord bottom)
{
	fLeft	= left;
	fTop	= top;
	fRight	= right;
	fBottom	= bottom;
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::Set (const EmPointTempl<coord>& topLeft, const EmPointTempl<coord>& bottomRight)
{
	fLeft	= topLeft.fX;
	fTop	= topLeft.fY;
	fRight	= bottomRight.fX;
	fBottom	= bottomRight.fY;
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::BeEmpty (void)
{
	fTop = fLeft = fBottom = fRight = 0;
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::Inset (coord x, coord y)
{
	fTop	+= y;
	fLeft 	+= x;
	fBottom	-= y;
	fRight	-= x;
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::Offset (coord x, coord y)
{
	fTop	+= y;
	fLeft	+= x;
	fBottom	+= y;
	fRight	+= x;
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::ScaleUp (coord x, coord y)
{
	fTop	*= y;
	fLeft	*= x;
	fBottom	*= y;
	fRight	*= x;
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::ScaleDown (coord x, coord y)
{
	EmAssert (x != 0);
	EmAssert (y != 0);

	fTop	/= y;
	fLeft	/= x;
	fBottom	/= y;
	fRight	/= x;
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::IntersectWith (const EmRectTempl<coord>& other)
{
	if (this->IsEmpty ())
	{
	}
	else if (other.IsEmpty ())
	{
		this->BeEmpty ();
	}
	else
	{
		if (other.fLeft > fLeft)
			fLeft = other.fLeft;

		if (other.fTop > fTop)
			fTop = other.fTop;

		if (other.fRight < fRight)
			fRight = other.fRight;

		if (other.fBottom < fBottom)
			fBottom = other.fBottom;
	}
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::UnionWith (const EmRectTempl<coord>& other)
{
	if (this->IsEmpty ())
	{
		*this = other;
	}
	else if (other.IsEmpty ())
	{
	}
	else
	{
		this->ExtendTo (other);
	}
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::ExtendTo (const EmPointTempl<coord>& pt)
{
	if (pt.fX < fLeft)
		fLeft = pt.fX;

	if (pt.fY < fTop)
		fTop = pt.fY;

	if (pt.fX > fRight)
		fRight = pt.fX;

	if (pt.fY > fBottom)
		fBottom = pt.fY;
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::ExtendTo (const EmRectTempl<coord>& other)
{
	if (other.fLeft < fLeft)
		fLeft = other.fLeft;

	if (other.fTop < fTop)
		fTop = other.fTop;

	if (other.fRight > fRight)
		fRight = other.fRight;

	if (other.fBottom > fBottom)
		fBottom = other.fBottom;
}

// ----------------------------------------------------------------------

template <class coord>
void
EmRectTempl<coord>::Normalize (void)
{
	coord	temp;

	if (fBottom < fTop)
	{
		temp = fBottom;
		fBottom = fTop;
		fTop = temp;
	}

	if (fRight < fLeft)
	{
		temp = fRight;
		fRight = fLeft;
		fLeft = temp;
	}
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>
EmRectTempl<coord>::operator+ (const EmPointTempl<coord>& pt) const
{
	EmRectTempl<coord>	result (*this);
	result.Offset (pt.fX, pt.fY);
	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>
EmRectTempl<coord>::operator- (const EmPointTempl<coord>& pt) const
{
	EmRectTempl<coord>	result (*this);
	result.Offset (-pt.fX, -pt.fY);
	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>&
EmRectTempl<coord>::operator+= (const EmPointTempl<coord>& pt)
{
	this->Offset (pt.fX, pt.fY);
	return *this;
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>&
EmRectTempl<coord>::operator-= (const EmPointTempl<coord>& pt)
{
	this->Offset (-pt.fX, -pt.fY);
	return *this;
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>
EmRectTempl<coord>::operator* (const EmPointTempl<coord>& pt) const
{
	EmRectTempl<coord>	result (*this);
	result.ScaleUp (pt.fX, pt.fY);
	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>
EmRectTempl<coord>::operator/ (const EmPointTempl<coord>& pt) const
{
	EmRectTempl<coord>	result (*this);
	result.ScaleDown (pt.fX, pt.fY);
	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>&
EmRectTempl<coord>::operator*= (const EmPointTempl<coord>& pt)
{
	this->ScaleUp (pt.fX, pt.fY);
	return *this;
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>&
EmRectTempl<coord>::operator/= (const EmPointTempl<coord>& pt)
{
	this->ScaleDown (pt.fX, pt.fY);
	return *this;
}

// ----------------------------------------------------------------------

template <class coord>
Bool
EmRectTempl<coord>::operator== (const EmRectTempl<coord>& rt) const
{
	return this->IsEqual (rt);
}

// ----------------------------------------------------------------------

template <class coord>
Bool
EmRectTempl<coord>::operator!= (const EmRectTempl<coord>& rt) const
{
	return !this->IsEqual (rt);
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>
EmRectTempl<coord>::operator& (const EmRectTempl<coord>& rt) const
{
	EmRectTempl<coord>	result (*this);
	result.IntersectWith (rt);
	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>
EmRectTempl<coord>::operator| (const EmRectTempl<coord>& rt) const
{
	EmRectTempl<coord>	result (*this);
	result.UnionWith (rt);
	return result;
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>&
EmRectTempl<coord>::operator&= (const EmRectTempl<coord>& rt)
{
	this->IntersectWith (rt);
	return *this;
}

// ----------------------------------------------------------------------

template <class coord>
EmRectTempl<coord>&
EmRectTempl<coord>::operator|= (const EmRectTempl<coord>& rt)
{
	this->UnionWith (rt);
	return *this;
}

// ----------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(disable: 4660)  // template-class specialization is already instatiated
#endif

template class EmRectTempl<EmCoord>;


// ----------------------------------------------------------------------
//	Psuedo-type definitions to aid in the conversion process without
//	actually having to #include the requisite headers.
// ----------------------------------------------------------------------

#ifndef _WINDEF_
struct	tagRECT			{ char	_[16]; };
#endif

#ifndef __AFXWIN_H__
class	CRect			{ char	_[16]; };
#endif

#ifndef __MACTYPES__
struct	Rect			{ char	_[ 8]; };
#endif

struct	VRect			{ char	_[16]; };

// ----------------------------------------------------------------------
//	* EmRect
// ----------------------------------------------------------------------

//
// Construct from another kind of rectangle
// Assigned from another kind of rectangle
//
	#undef FOR_RECT
	#define FOR_RECT(cls, size, l, t, r, b)				\
		EmRect::EmRect(const cls& rt) :					\
			EmRectTempl<EmCoord>(((size*)&rt)[l], ((size*)&rt)[t], ((size*)&rt)[r], ((size*)&rt)[b]) {}		\
		const EmRect& EmRect::operator= (const cls& rt)	\
			{ fLeft = ((size*)&rt)[l]; fTop = ((size*)&rt)[t]; fRight = ((size*)&rt)[r]; fBottom = ((size*)&rt)[b]; return *this; }

	RECT_LIST_XY_SHORT
	RECT_LIST_XY_LONG
	RECT_LIST_YX_SHORT
	RECT_LIST_YX_LONG

//
// Return another kind of rectangle
//
	#undef FOR_RECT
	#define FOR_RECT(cls, size, l, t, r, b)		\
		EmRect::operator cls () const			\
			{ size pt[4]; pt[l] = fLeft; pt[t] = fTop; pt[r] = fRight; pt[b] = fBottom; return *(cls*) pt; }

	RECT_LIST_XY_LONG
	RECT_LIST_YX_LONG
	RECT_LIST_XY_SHORT
	RECT_LIST_YX_SHORT

//
// Return a [const] (* | &) to another kind of rectangle
//
	#undef FOR_RECT
	#define FOR_RECT(cls, size, l, t, r, b)		\
		EmRect::operator const cls*() const		\
			{ return (const cls*) this; }		\
		EmRect::operator cls*()					\
			{ return (cls*) this; }				\
		EmRect::operator const cls&() const		\
			{ return *(const cls*) this; }		\
		EmRect::operator cls&()					\
			{ return *(cls*) this; }

	RECT_LIST_LONG

//#define TESTING
#ifdef TESTING
// ----------------------------------------------------------------------
#include "stdio.h"
void TestRect();
void TestRect()
{
	printf("Testing EmRectTempl class...\n");

	{
		//
		// Constructor (coords)
		//
		EmRect	rect1(1, 2, 3, 4);
		EmAssert(rect1.fLeft == 1);
		EmAssert(rect1.fTop == 2);
		EmAssert(rect1.fRight == 3);
		EmAssert(rect1.fBottom == 4);

		//
		// Constructor (points)
		//
		EmRect	rect2(EmPoint(2, 4), EmPoint(6, 8));
		EmAssert(rect2.fLeft == 2);
		EmAssert(rect2.fTop == 4);
		EmAssert(rect2.fRight == 6);
		EmAssert(rect2.fBottom == 8);

		//
		// Constructor (copy)
		//
		EmRect	rect3(rect2);
		EmAssert(rect3.fLeft == 2);
		EmAssert(rect3.fTop == 4);
		EmAssert(rect3.fRight == 6);
		EmAssert(rect3.fBottom == 8);

		//
		// Assignment
		//
		EmRect	rect4;
		rect4 = rect2;
		EmAssert(rect4.fLeft == 2);
		EmAssert(rect4.fTop == 4);
		EmAssert(rect4.fRight == 6);
		EmAssert(rect4.fBottom == 8);

		//
		// Equality
		//
		EmAssert(rect1 == rect1);
		EmAssert(!(rect1 != rect1));
		EmAssert(rect1 != rect2);
		EmAssert(!(rect1 == rect2));
		EmAssert(rect2 == rect3);
		EmAssert(!(rect2 != rect3));

		EmAssert(rect1.IsEqual(rect1));
		EmAssert(!rect1.IsEqual(rect2));
		EmAssert(rect2.IsEqual(rect3));

		//
		// Width / height
		//
		EmAssert(rect1.Width() == 3 - 1);
		EmAssert(rect2.Width() == 6 - 2);
		EmAssert(rect1.Height() == 4 - 2);
		EmAssert(rect2.Height() == 8 - 4);

		//
		// Size
		//
#ifndef forWin32
		EmAssert(rect1.Size() == EmPoint(3 - 1, 4 - 2));
		EmAssert(rect2.Size() == EmPoint(6 - 2, 8 - 4));
#endif

		//
		// Top / left / bottom / right
		//
		EmAssert(rect1.TopLeft() == EmPoint(1, 2));
		EmAssert(rect2.TopLeft() == EmPoint(2, 4));
		EmAssert(rect1.TopRight() == EmPoint(3, 2));
		EmAssert(rect2.TopRight() == EmPoint(6, 4));
		EmAssert(rect1.BottomLeft() == EmPoint(1, 4));
		EmAssert(rect2.BottomLeft() == EmPoint(2, 8));
		EmAssert(rect1.BottomRight() == EmPoint(3, 4));
		EmAssert(rect2.BottomRight() == EmPoint(6, 8));

		//
		// N / S / E / W / Center
		//
		EmAssert(rect1.North() == EmPoint(2, 2));
		EmAssert(rect2.North() == EmPoint(4, 4));
		EmAssert(rect1.South() == EmPoint(2, 4));
		EmAssert(rect2.South() == EmPoint(4, 8));
		EmAssert(rect1.West() == EmPoint(1, 3));
		EmAssert(rect2.West() == EmPoint(2, 6));
		EmAssert(rect1.East() == EmPoint(3, 3));
		EmAssert(rect2.East() == EmPoint(6, 6));
		EmAssert(rect1.Center() == EmPoint(2, 3));
		EmAssert(rect2.Center() == EmPoint(4, 6));

		//
		// Empty / Null
		//
		EmRect	rect5(5, 5, 5, 5);
		EmRect	rect6(0, 0, 0, 0);

		EmAssert(rect5.IsEmpty());
		EmAssert(!rect5.IsNull());
		EmAssert(rect6.IsEmpty());
		EmAssert(rect6.IsNull());
		EmAssert(!rect2.IsEmpty());
		EmAssert(!rect2.IsNull());

		{
			//
			// Contains (point)
			//
			EmPoint	pt1(0, 0);
			EmPoint	pt2(3, 0);
			EmPoint	pt3(6, 0);
			EmPoint	pt4(0, 6);
			EmPoint	pt5(3, 6);
			EmPoint	pt6(6, 6);
			EmPoint	pt7(0, 12);
			EmPoint	pt8(3, 12);
			EmPoint	pt9(6, 12);

			EmAssert(!rect2.Contains(pt1));
			EmAssert(!rect2.Contains(pt2));
			EmAssert(!rect2.Contains(pt3));

			EmAssert(!rect2.Contains(pt4));
			EmAssert(rect2.Contains(pt5));
			EmAssert(!rect2.Contains(pt6));

			EmAssert(!rect2.Contains(pt7));
			EmAssert(!rect2.Contains(pt8));
			EmAssert(!rect2.Contains(pt9));

			EmAssert(rect2.Contains(rect2.TopLeft()));
			EmAssert(rect2.Contains(rect2.North()));
			EmAssert(!rect2.Contains(rect2.TopRight()));

			EmAssert(rect2.Contains(rect2.West()));
			EmAssert(rect2.Contains(rect2.Center()));
			EmAssert(!rect2.Contains(rect2.East()));

			EmAssert(!rect2.Contains(rect2.BottomLeft()));
			EmAssert(!rect2.Contains(rect2.South()));
			EmAssert(!rect2.Contains(rect2.BottomRight()));
		}

		{
			//
			// Contains (rect)
			//
			rect4 = rect2;
			EmAssert( rect2.Contains(rect4));
			EmAssert( rect4.Contains(rect2));

			rect4.fTop--;
			EmAssert(!rect2.Contains(rect4));
			EmAssert( rect4.Contains(rect2));

			rect4.fTop += 2;
			EmAssert( rect2.Contains(rect4));
			EmAssert(!rect4.Contains(rect2));

			rect4 = rect2;
			rect4.fLeft--;
			EmAssert(!rect2.Contains(rect4));
			EmAssert( rect4.Contains(rect2));

			rect4.fLeft += 2;
			EmAssert( rect2.Contains(rect4));
			EmAssert(!rect4.Contains(rect2));

			rect4 = rect2;
			rect4.fBottom++;
			EmAssert(!rect2.Contains(rect4));
			EmAssert( rect4.Contains(rect2));

			rect4.fBottom -= 2;
			EmAssert( rect2.Contains(rect4));
			EmAssert(!rect4.Contains(rect2));

			rect4 = rect2;
			rect4.fRight++;
			EmAssert(!rect2.Contains(rect4));
			EmAssert( rect4.Contains(rect2));

			rect4.fRight -= 2;
			EmAssert( rect2.Contains(rect4));
			EmAssert(!rect4.Contains(rect2));
		}

		{
			//
			// Intersects
			//
			EmRect	rect(100, 200, 300, 400);

			/*
			     0        50        100        200       300        350       400
			   0 +-------------------+--------------------+--------------------+
			     |                   |                    |                    |
			     |                   |                    |                    |
			     |                   |                    |                    |
			 100 |         0         *          1         *          2         |
			     |                   |                    |                    |
			     |                   |                    |                    |
			     |                   |                    |                    |
			 200 +---------*---------*----------*---------*----------*---------+
			     |                   |                    |                    |
			     |                   |                    |                    |
			     |                   |                    |                    |
			 300 |         3         *          4         *          5         |
			     |                   |                    |                    |
			     |                   |                    |                    |
			     |                   |                    |                    |
			 400 +---------*---------*----------*---------*----------*---------+
			     |                   |                    |                    |
			     |                   |                    |                    |
			     |                   |                    |                    |
			 500 |         6         *          7         *          8         |
			     |                   |                    |                    |
			     |                   |                    |                    |
			     |                   |                    |                    |
			 600 +-------------------+--------------------+--------------------+

			*/

			EmCoord	xCoords[] = {
				   0, 50, 100,		101, 200, 300,		301, 350, 400
			};

			EmCoord	yCoords[] = {
				  0,
				100,
				200,

				201,
				300,
				400,

				401,
				500,
				600
			};

			#define neither -1

			char results[9][9] = {
				false,		false,		false,
				false,		true,		true,
				false,		true,		true,

				neither,	false,		false,
				neither,	true,		true,
				neither,	true,		true,

				neither,	neither,	false,
				neither,	neither,	false,
				neither,	neither,	false,

				neither,	neither,	neither,
				false,		true,		true,
				false,		true,		true,

				neither,	neither,	neither,
				neither,	true,		true,
				neither,	true,		true,

				neither,	neither,	neither,
				neither,	neither,	false,
				neither,	neither,	false,

				neither,	neither,	neither,
				neither,	neither,	neither,
				false,		false,		false,

				neither,	neither,	neither,
				neither,	neither,	neither,
				neither,	false,		false,

				neither,	neither,	neither,
				neither,	neither,	neither,
				neither,	neither,	false,

			};

			for (int y1 = 0; y1 < 9; y1++)
			{
				for (int x1 = 0; x1 < 9; x1++)
				{
					for (int y2 = 0; y2 < 9; y2++)
					{
						for (int x2 = 0; x2 < 9; x2++)
						{
							int qx1 = xCoords[x1] >= 300 ? 2 : xCoords[x1] >= 100 ? 1 : 0;
							int qy1 = yCoords[y1] >= 400 ? 2 : yCoords[y1] >= 200 ? 1 : 0;

							int qx2 = xCoords[x2] <= 100 ? 0 : xCoords[x2] <= 300 ? 1 : 2;
							int qy2 = yCoords[y2] <= 200 ? 0 : yCoords[y2] <= 400 ? 1 : 2;

							int q1 = qy1 * 3 + qx1;
							int q2 = qy2 * 3 + qx2;

							if (results[q1][q2] != neither)
							{
								EmRect	testRect(xCoords[x1], yCoords[y1], xCoords[x2], yCoords[y2]);
								testRect.Normalize();

								if (rect.Intersects(testRect) != results[q1][q2])
								{
									printf("rect (%d, %d, %d, %d) %s rect (%d, %d, %d, %d)\n",
										rect.fLeft, rect.fTop, rect.fRight, rect.fBottom,
										rect.Intersects(testRect) ? "intersects" : "does not intersect",
										testRect.fLeft, testRect.fTop, testRect.fRight, testRect.fBottom);
								}
							}

						}
					}
				}
			}
		}

		{
			//
			// Set(coords) / Set(point) / BeEmpty
			//
			EmRect	rect;

			rect.Set(1, 2, 3, 4);
			EmAssert(rect.fLeft == 1);
			EmAssert(rect.fTop == 2);
			EmAssert(rect.fRight == 3);
			EmAssert(rect.fBottom == 4);

			rect.Set(EmPoint(2, 4), EmPoint(6, 8));
			EmAssert(rect.fLeft == 2);
			EmAssert(rect.fTop == 4);
			EmAssert(rect.fRight == 6);
			EmAssert(rect.fBottom == 8);

			rect.BeEmpty();
			EmAssert(rect.IsEmpty());

			rect.Set(10, 20, 30, 40);
			rect.Inset(6, 12);
			EmAssert(rect.fLeft == 10 + 6);
			EmAssert(rect.fTop == 20 + 12);
			EmAssert(rect.fRight == 30 - 6);
			EmAssert(rect.fBottom == 40 - 12);

			rect.Set(10, 20, 30, 40);
			rect.Offset(6, 12);
			EmAssert(rect.fLeft == 10 + 6);
			EmAssert(rect.fTop == 20 + 12);
			EmAssert(rect.fRight == 30 + 6);
			EmAssert(rect.fBottom == 40 + 12);
		}

		{
			// Intersect

			EmRect	rect(100, 200, 300, 400);

			EmRect	rect1(0, 0, 0, 0);
			rect1.IntersectWith(rect);
			EmAssert(rect1.IsEmpty());

			EmRect	rect2(0, 0, 100, 200);
			rect2.IntersectWith(rect);
			EmAssert(rect2.IsEmpty());

			EmRect	rect3(0, 0, 200, 300);
			rect3.IntersectWith(rect);
			EmAssert(rect3 == EmRect(100, 200, 200, 300));
		}

		{
			// !!! Union
		}

		{
			//
			// Normalize
			//

			EmRect	rect(0, 1, 2, 3);

			EmRect	rect1(0, 1, 2, 3);
			rect1.Normalize();
			EmAssert(rect1 == rect);

			EmRect	rect2(2, 1, 0, 3);
			rect2.Normalize();
			EmAssert(rect2 == rect);

			EmRect	rect3(0, 3, 2, 1);
			rect3.Normalize();
			EmAssert(rect3 == rect);

			EmRect	rect4(2, 3, 0, 1);
			rect4.Normalize();
			EmAssert(rect4 == rect);
		}

		// !!! Need to test all the constructors and conversion operators
	}
}
#endif

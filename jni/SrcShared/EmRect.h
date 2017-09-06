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

#ifndef EmRect_h
#define EmRect_h

/*
	Contains classes for working with rectangles:

		EmRectTempl<coord>:	Generic template class. The type of rectangle
							coordinate is provided by the client.

		EmRect:				Specific specialization of EmRectTempl for EmCoord.

	For flexibility, the x/y ordering of coordinates within the object
	is controlled by the XYRect pre-processor symbol. Choosing a
	particular order enables certain conversion operations; choosing the
	other order enables other conversion operations. For instance, when
	using long coordinates and XY ordering, we can produce a RECT*
	(Win32 rect), but not a VRect* (MacApp rect).

	EmRect offers no new operations. What it does offer is constructors
	for creation from a platform-specific rectangle (like Rect, RECT, CRect,
	or VRect) or for producing some variant of a platform-specific
	rectangle. The different kinds of rects that EmRect attempts to convert
	to/from are:

		RECT		Rect
		CRect		VRect

	Where possible, EmRect produces pointers, const pointers, references,
	and const references to the above types.

	Note that there are no virtual functions. These rect classes are
	not intended to be used polymorphically.
*/

#include "EmPoint.h"			// EmPoint

#if PLATFORM_WINDOWS
	#define XYRect 1
#else
	#define XYRect 0
#endif

#undef topLeft

template <class coord>
class EmRectTempl
{
	public:
								EmRectTempl	()
								{}
								EmRectTempl	(coord left, coord top, coord right, coord bottom) :
#if XYRect
									fLeft (left),
									fTop (top),
									fRight (right),
									fBottom (bottom)
#else
									fTop (top),
									fLeft (left),
									fBottom (bottom),
									fRight (right)
#endif
								{}
								EmRectTempl	(const EmPointTempl<coord>& topLeft, const EmPointTempl<coord>& bottomRight) :
#if XYRect
									fLeft (topLeft.fX),
									fTop (topLeft.fY),
									fRight (bottomRight.fX),
									fBottom (bottomRight.fY)
#else
									fTop (topLeft.fY),
									fLeft (topLeft.fX),
									fBottom (bottomRight.fY),
									fRight (bottomRight.fX)
#endif
								{}
								EmRectTempl	(const EmRectTempl<coord>& other) :
#if XYRect
									fLeft (other.fLeft),
									fTop (other.fTop),
									fRight (other.fRight),
									fBottom (other.fBottom)
#else
									fTop (other.fTop),
									fLeft (other.fLeft),
									fBottom (other.fBottom),
									fRight (other.fRight)
#endif
								{}

//								~EmRectTempl() {};
			// Constructors/Destructor. The default constructor doesn't do anything,
			// including setting its members to zero. There is no destructor to cut
			// down on the amount of code generated for this lightweight class.

		coord					Width		(void) const;
		coord					Height		(void) const;
			// Return the width and height of the rectangle. The return type
			// is the same as the coordinate type.

		EmPointTempl<coord>		Size		(void) const;
			// Return the size of the rectangle. The return type is an
			// EmPointTempl<coord> with the same type as the coordinate type.

		EmPointTempl<coord>		TopLeft		(void) const;
		EmPointTempl<coord>		TopRight	(void) const;
		EmPointTempl<coord>		BottomLeft	(void) const;
		EmPointTempl<coord>		BottomRight	(void) const;
		EmPointTempl<coord>		North		(void) const;
		EmPointTempl<coord>		South		(void) const;
		EmPointTempl<coord>		East		(void) const;
		EmPointTempl<coord>		West		(void) const;
		EmPointTempl<coord>		Center		(void) const;
			// Return the following marked points of a rectangle. The return
			// type is an EmPointTempl<coord> with the same type as the coordinate
			// type.
			//				    North
			//		TopLeft X-----X-----X TopRight
			// 				|           |
			//				|           |
			//		   West X     X     X East
			//				|    Ctr    |
			//				|           |
			//	 BottomLeft X-----X-----X BottomRight
			//				    South

		Bool					IsEmpty		(void) const;
			// Returns whether or not the rectangle contains positive area. The
			// coordinates do not necessarily have to be zero.

		Bool					IsNull		(void) const;
			// Returns whether or not all four coordinates are zero.

		Bool					IsEqual		(const EmRectTempl<coord>& other) const;
			// Returns whether or not two rectangles have the same coordinates.
			// Same as operator==().

		Bool					Contains	(const EmPointTempl<coord>&) const;
		Bool					Contains	(const EmRectTempl<coord>&) const;
			// Returns whether or not the given point or rectangle is contained
			// by the rectangle. Comparisons are made on a half-open interval.

		Bool					Intersects	(const EmRectTempl<coord>&) const;
			// Returns whether or not the two rectangles intersect each other.

		void					Set			(coord left, coord top, coord right, coord bottom);
		void					Set			(const EmPointTempl<coord>& topLeft, const EmPointTempl<coord>& bottomRight);
			// Set the coordinates of an already created rectangle. Performs
			// essentially the same operation as the constructors.

		void					BeEmpty		(void);
			// Set all four coordinates to zero. Technically, I supposed this
			// is a "SetNull" operation, but a future implementation could
			// conceivably just set fRight to fLeft.

		void					Inset		(coord x, coord y);
			// Inset the four coordinates by the given amounts.

		void					Offset		(coord x, coord y);
			// Add x and y to the rects's coordinates. Same as operator+=()
			// except that you specify coordinates, not another point.

		void					ScaleUp		(coord x, coord y);
			// Multiply the rects's coordinates by x and y. Same as operator*=()
			// except that you specify coordinates, not another point.

		void					ScaleDown	(coord x, coord y);
			// Divide the rects's coordinates by x and y. Same as operator/=()
			// except that you specify coordinates, not another point.

		void					IntersectWith(const EmRectTempl<coord>& other);
			// Intersect *this with other, storing the result in *this.
			// If other is an empty rectangle, *this will end up being an
			// empty rectangle, but no other guarantee is made about its
			// coordinates.

		void					UnionWith	(const EmRectTempl<coord>& other);
			// Union *this with other, storing the result in *this.
			// Empty rectangles are ignored (i.e., if *this is an empty
			// rectangle, other is copied into *this. If other is an empty
			// rectangle, *this is unchanged).

		void					ExtendTo	(const EmPointTempl<coord>& pt);
		void					ExtendTo	(const EmRectTempl<coord>& other);
			// Extend *this to contain the points expressed by the given
			// point or rectangle. The version taking a rectangle is similar
			// to UnionWith, the difference being that empty rectangles are
			// not ignored.

		void					Normalize	(void);
			// Order the coordinates such that fLeft <= fRight and fTop
			// <= fBottom.

		EmRectTempl<coord>		operator+	(const EmPointTempl<coord>& pt) const;
		EmRectTempl<coord>		operator-	(const EmPointTempl<coord>& pt) const;
		EmRectTempl<coord>&		operator+=	(const EmPointTempl<coord>& pt);
		EmRectTempl<coord>&		operator-=	(const EmPointTempl<coord>& pt);
			// Offset the rectangle by the amounts indicated by the EmPointTempl
			// parameter. The first two methods return a new EmRectTempl, leaving
			// the original untouched. The second two alter the original
			// rectangle in-place.

		EmRectTempl<coord>		operator*	(const EmPointTempl<coord>& pt) const;
		EmRectTempl<coord>		operator/	(const EmPointTempl<coord>& pt) const;
		EmRectTempl<coord>&		operator*=	(const EmPointTempl<coord>& pt);
		EmRectTempl<coord>&		operator/=	(const EmPointTempl<coord>& pt);
			// Scale the rectangle by the amounts indicated by the EmPointTempl
			// parameter. The first two methods return a new EmRectTempl, leaving
			// the original untouched. The second two alter the original
			// rectangle in-place.

		Bool					operator==	(const EmRectTempl<coord>& rt) const;
		Bool					operator!=	(const EmRectTempl<coord>& rt) const;
			// (In)equality operators. Returns whether or not the coordinates
			// of the two rectangles are equal to each other.

		EmRectTempl<coord>		operator&	(const EmRectTempl<coord>& rt) const;	// Intersection
		EmRectTempl<coord>		operator|	(const EmRectTempl<coord>& rt) const;	// Union
		EmRectTempl<coord>&		operator&=	(const EmRectTempl<coord>& rt);
		EmRectTempl<coord>&		operator|=	(const EmRectTempl<coord>& rt);
			// Synonyms for Intersect and Union, respectively. The first
			// two operators return the result in a new EmRectTempl. The second
			// pair of operators alter the target rectangle in-place.

			// Directly accessible data members.
#if XYRect
		coord	fLeft;
		coord	fTop;
		coord	fRight;
		coord	fBottom;
#else
		coord	fTop;
		coord	fLeft;
		coord	fBottom;
		coord	fRight;
#endif
};


// ----------------------------------------------------------------------
//	Types we interoperate with
// ----------------------------------------------------------------------

struct	tagRECT;
typedef struct tagRECT RECT;
class	CRect;

struct	Rect;
struct	VRect;


/* ----------------------------------------------------------------------
	Macros to help with creating the massive number of constructors and
	conversion operators we support:

		RECT_LIST_XY_SHORT:		List of rect-types using x/y coordinate
								ordering and short coordinates.

		RECT_LIST_XY_LONG:		List of rect-types using x/y coordinate
								ordering and long coordinates.

		RECT_LIST_YX_SHORT:		List of rect-types using y/x coordinate
								ordering and short coordinates.

		RECT_LIST_YX_LONG:		List of rect-types using y/x coordinate
								ordering and long coordinates.

		RECT_LIST_SHORT:		List of rect-types using short coordinates.
								Only rect-types using the same x/y ordering
								as indicated by XYRect are included.

		RECT_LIST_LONG:			List of rect-types using long coordinates.
								Only point-types using the same x/y ordering
								as indicated by XYRect are included.

	Each of the above lists includes a reference to a rect-type using a
	FOR_RECT macro. The FOR_RECT macro takes as parameters the
	rect-type, the size of coordinates, and offsets to each coordinate.
	This information is all that's needed for constructing all of
	constructors and convertion operators, both declarations and
	implementations. To perform the actual construction, FOR_RECT is
	defined to expand to the appropriate code for a single constructor
	or conversion, and then the appropriate RECT_LIST macro from above
	is "invoked" to expand the list as needed.
  ---------------------------------------------------------------------- */

#define RECT_LIST_XY_SHORT				\
	FOR_RECT(AbsRectType, short, 0, 1, 2, 3)

#define RECT_LIST_XY_LONG				\
	FOR_RECT(RECT, long, 0, 1, 2, 3)	\
	FOR_RECT(CRect, long, 0, 1, 2, 3)

#define RECT_LIST_YX_SHORT				\
	FOR_RECT(Rect, short, 1, 0, 3, 2)

#define RECT_LIST_YX_LONG				\
	FOR_RECT(VRect, long, 1, 0, 3, 2)

#if XYRect
	#define RECT_LIST_LONG	RECT_LIST_XY_LONG
	#define RECT_LIST_SHORT	RECT_LIST_XY_SHORT
#else
	#define RECT_LIST_LONG	RECT_LIST_YX_LONG
	#define RECT_LIST_SHORT	RECT_LIST_YX_SHORT
#endif

// ----------------------------------------------------------------------
//	* EmRect
// ----------------------------------------------------------------------

class EmRect : public EmRectTempl<EmCoord>
{
	public:
								EmRect	()
								{}
								EmRect	(EmCoord left, EmCoord top, EmCoord right, EmCoord bottom) :
									EmRectTempl<EmCoord> (left, top, right, bottom)
								{}
								EmRect	(const RectangleType& r) :
									EmRectTempl<EmCoord> (	r.topLeft.x, r.topLeft.y,
															r.topLeft.x + r.extent.x,
															r.topLeft.y + r.extent.y)
								{}
								EmRect	(const EmPointTempl<EmCoord>& topLeft, const EmPointTempl<EmCoord>& bottomRight) :
									EmRectTempl<EmCoord>(topLeft, bottomRight)
								{}
								EmRect	(const EmRectTempl<EmCoord>& r) :
									EmRectTempl<EmCoord>(r)
								{}

//								~EmRect() {};

	//
	// Construct from another kind of rectangle
	// Assigned from another kind of rectangle
	// !!! Move functionality inline?
	//
		#undef FOR_RECT
		#define FOR_RECT(cls, size, l, t, b, r)		\
			EmRect (const cls&); 					\
			const EmRect& operator= (const cls&);

		RECT_LIST_XY_SHORT
		RECT_LIST_XY_LONG
		RECT_LIST_YX_SHORT
		RECT_LIST_YX_LONG

	//
	// Return another kind of rectangle
	// !!! Move functionality inline?
	//
		#undef FOR_RECT
		#define FOR_RECT(cls, size, l, t, b, r)		\
			operator cls () const;

		RECT_LIST_XY_LONG
		RECT_LIST_YX_LONG
		RECT_LIST_XY_SHORT
		RECT_LIST_YX_SHORT

	//
	// Return a [const] (* | &) to another kind of rectangle
	// !!! Move functionality inline?
	//
		#undef FOR_RECT
		#define FOR_RECT(cls, size, l, t, b, r)	\
			operator const cls* () const; 		\
			operator cls* (); 					\
			operator const cls& () const; 		\
			operator cls& ();

		RECT_LIST_LONG
};

#endif	// EmRect_h

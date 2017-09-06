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

#ifndef EmPoint_h
#define EmPoint_h

/*
	Contains classes for working with points:

		EmPointTempl<coord>:	Generic template class. The type of point
								coordinate is provided by the client.

		EmPoint:				Specific specialization of EmPointTempl for EmCoord.

	For flexibility, the x/y ordering of coordinates within the object
	is controlled by the XYPoint pre-processor symbol. Choosing a
	particular order enables certain conversion operations; choosing the
	other order enables other conversion operations. For instance, when
	using long coordinates and XY ordering, we can produce a POINT*
	(Win32 point), but not a VPoint* (MacApp point).

	EmPoint offers no new operations. What it does offer is constructors
	for creation from a platform-specific point (like Point, POINT, CPoint,
	VPoint, etc.) or for producing some variant of a platform-specific
	point. The different kinds of points that EmPoint attempts to convert
	to/from are:

		SIZE		SDimension16		Point
		CSize		SDimension32		VPoint

		POINT		SPoint16
		CPoint		SPoint32

	Where possible, EmPoint produces pointers, const pointers, references,
	and const references to the above types.

	Note that there are no virtual functions. These point classes are
	not intended to be used polymorphically.
*/

#if PLATFORM_WINDOWS
	#define XYPoint 1
#else
	#define XYPoint 0
#endif

template <class coord>
class EmPointTempl
{
	public:
								EmPointTempl	(void)
								{}
								EmPointTempl	(coord x, coord y) :
#if XYPoint
									fX (x),
									fY (y)
#else
								    fY (y),
									fX (x)
#endif
								{}
								EmPointTempl	(const EmPointTempl<coord>& pt) :
#if XYPoint
									fX (pt.fX),
									fY (pt.fY)
#else
								    fY (pt.fY),
									fX (pt.fX)
#endif
								{}
//								~EmPointTempl (void) {};
			// Constructors/Destructor. The default constructor doesn't do anything,
			// including setting its members to zero. There is no destructor to cut
			// down on the amount of code generated for this lightweight class.
			
		void					Offset			(coord x, coord y);
			// Add x and y to the point's coordinates. Same as operator+=()
			// except that you specify coordinates, not another point.

		EmPointTempl<coord>		operator+		(const EmPointTempl<coord>& pt) const;
		EmPointTempl<coord>		operator-		(const EmPointTempl<coord>& pt) const;
			// Add or subtract the two points with each other, producing a
			// new EmPointTempl<coord>.

		EmPointTempl<coord>		operator-		(void) const;
			// Unary minus. Negate our coordinates.

		EmPointTempl<coord>&	operator+=		(const EmPointTempl<coord>& pt);
		EmPointTempl<coord>&	operator-=		(const EmPointTempl<coord>& pt);
			// Add or subtract the coordinates of the given point from
			// ourselves.

		EmPointTempl<coord>		operator*		(const EmPointTempl<coord>& pt) const;
		EmPointTempl<coord>		operator/		(const EmPointTempl<coord>& pt) const;
		EmPointTempl<coord>&	operator*=		(const EmPointTempl<coord>& pt);
		EmPointTempl<coord>&	operator/=		(const EmPointTempl<coord>& pt);
			// Scale up or down the controlled point by the given point.

		Boolean					operator!=		(const EmPointTempl<coord>& pt) const;
		Boolean					operator==		(const EmPointTempl<coord>& pt) const;
			// (In)equality operators. Returns whether or not the coordinates
			// of the two points are equal to each other.

			// Directly accessible data members.
#if XYPoint
		coord	fX;
		coord	fY;
#else
		coord	fY;
		coord	fX;
#endif
};


// ----------------------------------------------------------------------
//	Types we interoperate with
// ----------------------------------------------------------------------

struct	tagSIZE;
typedef	struct tagSIZE SIZE;
class	CSize;

struct	tagPOINT;
typedef struct tagPOINT POINT;
class	CPoint;

struct	SDimension16;
struct	SDimension32;

struct	Point;
typedef Point	SPoint16;

struct	SPoint32;
struct	VPoint;


/* ----------------------------------------------------------------------
	Macros to help with creating the massive number of constructors and
	conversion operators we support:

		POINT_LIST_XY_SHORT:	List of point-types using x/y coordinate
								ordering and short coordinates.

		POINT_LIST_XY_LONG:		List of point-types using x/y coordinate
								ordering and long coordinates.

		POINT_LIST_YX_SHORT:	List of point-types using y/x coordinate
								ordering and short coordinates.

		POINT_LIST_YX_LONG:		List of point-types using y/x coordinate
								ordering and long coordinates.

		POINT_LIST_SHORT:		List of point-types using short coordinates.
								Only point-types using the same x/y ordering
								as indicated by XYPoint are included.

		POINT_LIST_LONG:		List of point-types using long coordinates.
								Only point-types using the same x/y ordering
								as indicated by XYPoint are included.

	Each of the above lists includes a reference to a point-type using a
	FOR_POINT macro. The FOR_POINT macro takes as parameters the
	point-type, the size of coordinates, and offsets to each coordinate.
	This information is all that's needed for constructing all of
	constructors and convertion operators, both declarations and
	implementations. To perform the actual construction, FOR_POINT is
	defined to expand to the appropriate code for a single constructor
	or conversion, and then the appropriate POINT_LIST macro from above
	is "invoked" to expand the list as needed.
  ---------------------------------------------------------------------- */

#define POINT_LIST_XY_SHORT				\
	FOR_POINT(SDimension16, short, 0, 1)\
	FOR_POINT(PointType, short, 0, 1)

#define POINT_LIST_XY_LONG				\
	FOR_POINT(SIZE, long, 0, 1)			\
	FOR_POINT(CSize, long, 0, 1)		\
	FOR_POINT(SDimension32, long, 0, 1)	\
	FOR_POINT(POINT, long, 0, 1)		\
	FOR_POINT(CPoint, long, 0, 1)		\
	FOR_POINT(SPoint32, long, 0, 1)

#define POINT_LIST_YX_SHORT				\
	FOR_POINT(Point, short, 1, 0)

#define POINT_LIST_YX_LONG				\
	FOR_POINT(VPoint, long, 1, 0)

#if XYPoint
	#define POINT_LIST_LONG		POINT_LIST_XY_LONG
	#define POINT_LIST_SHORT	POINT_LIST_XY_SHORT
#else
	#define POINT_LIST_LONG		POINT_LIST_YX_LONG
	#define POINT_LIST_SHORT	POINT_LIST_YX_SHORT
#endif

// ----------------------------------------------------------------------
//	* EmPoint
// ----------------------------------------------------------------------

class EmPoint : public EmPointTempl<EmCoord>
{
	public:
		EmPoint() {};
		EmPoint(EmCoord x, EmCoord y) : EmPointTempl<EmCoord>(x, y) {};
		EmPoint(const EmPointTempl<EmCoord>& pt) : EmPointTempl<EmCoord>(pt) {};
//		~EmPoint() {};

	//
	// Construct from another kind of point
	// Assigned from another kind of point
	// !!! Move functionality inline?
	//
		#undef FOR_POINT
		#define FOR_POINT(cls, size, x, y)	\
			EmPoint(const cls&);			\
			const EmPoint& operator=(const cls&);

		POINT_LIST_XY_SHORT
		POINT_LIST_XY_LONG
		POINT_LIST_YX_SHORT
		POINT_LIST_YX_LONG

	//
	// Return another kind of point
	// !!! Move functionality inline?
	//
		#undef FOR_POINT
		#define FOR_POINT(cls, size, x, y)	\
			operator cls() const;

		POINT_LIST_XY_LONG
		POINT_LIST_YX_LONG
		POINT_LIST_XY_SHORT
		POINT_LIST_YX_SHORT

	//
	// Return a [const] (* | &) to another kind of point
	// !!! Move functionality inline?
	//
		#undef FOR_POINT
		#define FOR_POINT(cls, size, x, y)	\
			operator const cls*() const;	\
			operator cls*();				\
			operator const cls&() const;	\
			operator cls&();

		POINT_LIST_LONG
};

#endif	// EmPoint_h

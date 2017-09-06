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
#include "EmRegion.h"

/*
	See .h file for usage.

	EmRegion relies on an internal helper class, EmRegionImpl. EmRegionImpl
	is a ref-counted implementation class. It can be shared among multiple
	ARegions for efficient copying. If one region is altered, it creates
	a new EmRegionImpl to contain the results, dropping the reference to
	the previous EmRegionImpl (possibly causing it to be deleted).

	EmRegionImpl provides basic operations on regions. It stores region
	data in a buffer of ACoords in the following format:

	<number of transition points in this scanline>
		<y coordinate of scanline>
			<x coordinate of "enter region" event>
			<x coordinate of "exit region" event>
			...
	<number of transition points in this scanline>
		<y coordinate of scanline>
			<x coordinate of "enter region" event>
			<x coordinate of "exit region" event>
			...
	...
	<number of points in this scanline>
		<y coordinate of scanline>
	<End of region marker>

	For example, take the following region:


		|     5     10    15    20    25    30
	----+-----|-----|-----|-----|-----|-----|-----
		|
		|
	5	-     +-----+           +-----+
		|     |.....|           |.....|
		|     |.....|           |.....|
	10	-     |.....+-----------+.....|
		|     |.......................|
		|     |.......................|
	15	-     +-----------------------+
		|

	This region would be represented with the following data:

		5	// Number of coordinates in this line
		5	// y coordinate
		5	// entering x coordinate
		10	// exiting x coordinate
		20	// entering x coordinate
		25	// exiting x coordinate
		3	// Number of coordinates on this line
		10	// y coordinate
		5	// entering x coordinate
		25	// exiting x coordinate
		1	// Number of coordinates in this line
		15	// y coordinate
		0	// End of region marker

	Thus, the buffer would look as follows:

		5	5	5	10	20	25	3	10	5	25	1	15	0
*/

#include "EmCommon.h"
#include "EmRegion.h"

#include "string.h"				// memcpy

// ---------------------------------------------------------------------------
//		* EmRegionImpl
// ---------------------------------------------------------------------------
//	Constructor for an empty region.

EmRegion::EmRegionImpl::EmRegionImpl (void) :
	fBounds (0, 0, 0, 0),
	fCapacity (0),
	fBuf (NULL)
{
}


// ---------------------------------------------------------------------------
//		* EmRegionImpl
// ---------------------------------------------------------------------------
//	Constructor for a rectangular region. Uses a built-in array instead of
//	getting memory from the heap. The array ends up looking as follows:
//
//		3, top, left, right, 1, bottom ,0

EmRegion::EmRegionImpl::EmRegionImpl (const EmRect& r) :
	fBounds (r),
	fCapacity (7),
	fBuf (fRectBuf)
{
	EmCoord*	s = fBuf;

	*s++ = 3;
	*s++ = r.fTop;
	*s++ = r.fLeft;
	*s++ = r.fRight;

	*s++ = 1;
	*s++ = r.fBottom;

	*s = 0;
}


// ---------------------------------------------------------------------------
//		* EmRegionImpl
// ---------------------------------------------------------------------------
//	Constructor for a general region. Works from a buffer created by
//	RegionOp, so it's not likely that clients will be calling this constructor.

EmRegion::EmRegionImpl::EmRegionImpl (const EmCoord* s, long len) :
	fCapacity (0),
	fBuf (NULL)
{
	if (s && len > 0)
	{
		fCapacity = len;

		if (len <= 7)
			fBuf = fRectBuf;
		else
			fBuf = new EmCoord[len];

		memcpy (fBuf, s, len * sizeof (EmCoord));

		CalcBounds ();
	}
}


// ---------------------------------------------------------------------------
//		* ~EmRegionImpl
// ---------------------------------------------------------------------------
//	Destructor

EmRegion::EmRegionImpl::~EmRegionImpl (void)
{
	if (fRectBuf != fBuf)
		delete [] fBuf;
}


// ---------------------------------------------------------------------------
//		* Equality
// ---------------------------------------------------------------------------

Bool
EmRegion::EmRegionImpl::operator== (const EmRegionImpl& other) const
{
	return this->IsEqual (other);
}


// ---------------------------------------------------------------------------
//		* GetRects
// ---------------------------------------------------------------------------
//	Returns the number of rects this region decomposes to. If a buffer is
//	provided, return the rects themselves, too. This method assumes the
//	buffer is already large enough, so you might want to call it with NULL
//	first to get a rect count. Or, better yet, use EmRegionRectIterator to
//	get the rectangles one-by-one.

long
EmRegion::EmRegionImpl::GetRects (EmRect* rp) const
{
	EmCoord*	s = fBuf;
	long		ii = 0;

	if (s)
	{
		EmCoord	next;
		EmRect	r;

		while ((next = *s++) != 0)
		{
			r.fTop = *s++;
			r.fBottom = s[next];

			while (next > 1)
			{
				r.fLeft = *s++;
				r.fRight = *s++;
				next -= 2;

				if (!r.IsEmpty ())
				{
					if (rp)
						rp[ii] = r;

					++ii;
				}
			}
		}

		EmAssert (s <= fBuf + fCapacity);
	}

	return ii;
}


// ---------------------------------------------------------------------------
//		* CalcBounds
// ---------------------------------------------------------------------------
//	Scan the buffer data and determine the region's extent. This is done by
//	essentially decomposing the region into a series of rectangles and then
//	unioning those rectangles together.

void
EmRegion::EmRegionImpl::CalcBounds (void)
{
	EmCoord*	s = fBuf;

	fBounds.BeEmpty ();

	if (s)
	{
		EmCoord	next;
		EmRect	r;

		while ((next = *s++) != 0)
		{
			r.fTop = *s++;
			r.fBottom = s[next];

			while (next > 1)
			{
				r.fLeft = *s++;
				r.fRight = *s++;
				next -= 2;
				fBounds.UnionWith (r);
			}
		}

		EmAssert (s <= fBuf + fCapacity);
	}
}


// ---------------------------------------------------------------------------
//		* Contains
// ---------------------------------------------------------------------------
//	Scan the buffer data and determine if the given point falls within
//	the region. For each scanline, see if the y-coordinate of the point
//	falls between it and the next scanline. If so, scan all the x-coordinate
//	pairs for the scanline to see if the x-coordinate of the point falls
//	between any of them.

Bool
EmRegion::EmRegionImpl::Contains (const EmPoint& p) const
{
	EmCoord*	bp = fBuf;
	EmCoord		s, e, next;

	if (fBounds.Contains (p))
	{
		while ((next = *bp++) != 0)				// while we haven't hit the end...
		{
			s = *bp++;
			if (p.fY >= s && p.fY < bp[next])	// if between right scanlines...
			{
				while (next > 1)				// while we have more x-pairs...
				{
					s = *bp++;					// entering region x-coord
					e = *bp++;					// exiting region x-coord
					if (p.fX >= s && p.fX < e)
						return true;			// *** We're in! Return TRUE. ***

					next -= 2;					// move to next pair
				}
			}
			else
			{
				bp += next - 1;					// move to next scanline
			}
		}

		EmAssert (bp <= fBuf + fCapacity);
	}

	return false;
}


// ---------------------------------------------------------------------------
//		* Offset
// ---------------------------------------------------------------------------
//	Scan the buffer data, adding dy to each y-event, and dx to each pair of
//	x-events. Also update the region bounds.

void
EmRegion::EmRegionImpl::Offset (EmCoord dx, EmCoord dy)
{
	EmCoord*	s = fBuf;

	if (s && (dx || dy))
	{
		EmCoord next;

		while ((next = *s++) != 0)
		{
			*s++ += dy;
			while (next > 1)
			{
				*s++ += dx;
				*s++ += dx;
				next -= 2;
			}
		}

		fBounds.Offset (dx, dy);	// !!! Do this outside "if (s)..."?

		EmAssert (s <= fBuf + fCapacity);
	}
}


// ---------------------------------------------------------------------------
//		* IsEqual
// ---------------------------------------------------------------------------
//	Determine if two regions encompass the same area. First do some
//	quick-tests:
//
//		- See if the buffer lengths are equal. If not, regions are not equal.
//
//		- See if the buffers are both NULL. If so, they're both empty regions
//		  and are considered equal.
//
//		- See if just one buffer is NULL. If so, it's empty and the other is
//		  not, so they are not equal.
//
//	Finally, compare the two buffers. Since the data in the buffer is well-
//	ordered, we can do a direct memory compare to see if they are equal.

Bool
EmRegion::EmRegionImpl::IsEqual (const EmRegionImpl& r2) const
{
	if (fCapacity != r2.fCapacity)
		return false;

	if (fBuf == NULL && r2.fBuf == NULL)
		return true;

	if (fBuf == NULL || r2.fBuf == NULL)
		return false;

	return memcmp (fBuf, r2.fBuf, fCapacity * sizeof (EmCoord)) == 0;
}




// ---------------------------------------------------------------------------
//		* EmRegion
// ---------------------------------------------------------------------------
//	You want constructors, we got constructors...

EmRegion::EmRegion (void) :
	fImpl (new EmRegionImpl)
{
	EmAssert (fImpl.get ());
}

EmRegion::EmRegion (const EmCoord* s, long len) :
	fImpl (new EmRegionImpl (s, len))
{
	EmAssert (fImpl.get ());
}

EmRegion::EmRegion (const EmRect& r) :
	fImpl (new EmRegionImpl (r))
{
	EmAssert(fImpl.get ());
}

EmRegion::EmRegion (const EmRegion& r) :
	fImpl (r.fImpl)
{
	EmAssert (fImpl.get ());
}


// ---------------------------------------------------------------------------
//		* ~EmRegion
// ---------------------------------------------------------------------------
//	Destructor. Merely unreference our implementation object. If we're the
//	last/only region referencing it, it will delete itself.

EmRegion::~EmRegion (void)
{
}


// ---------------------------------------------------------------------------
//		* Assignment operator
// ---------------------------------------------------------------------------
//	If one region is being assigned to another, drop the reference to the
//	old implementation object and alias the new implementation object.
//
//	If a rectangle is being assigned to a region (a common operation), see
//	if the region already holds rectangular information and if we are the
//	exclusive owners of that information. If so, do a quick rect-copy. If
//	not, create a new implementation object to hold the rect.

EmRegion&
EmRegion::operator= (const EmRegion& r)
{
	if (this != &r)
	{
		fImpl = r.fImpl;
	}

	return *this;
}


EmRegion&
EmRegion::operator= (const EmRect& r)
{
	EmAssert (fImpl.get ());

	if (!fImpl->isShared () && fImpl->fCapacity <= 7)
	{
//		*fImpl = EmRegionImpl(r);	// !!! Do it this way?

		if (fImpl->fBuf == NULL)
			fImpl->fBuf = fImpl->fRectBuf;

		fImpl->fCapacity = 7;

		EmCoord* s = fImpl->fBuf;

		*s++ = 3;
		*s++ = r.fTop;
		*s++ = r.fLeft;
		*s++ = r.fRight;

		*s++ = 1;
		*s++ = r.fBottom;

		*s = 0;

		fImpl->fBounds = r;
	}
	else
	{
		fImpl = EmRefCounter<EmRegionImpl> (new EmRegionImpl (r));
	}

	return *this;
}


// ---------------------------------------------------------------------------
//		* BeEmpty
// ---------------------------------------------------------------------------
//	If we are not already empty and if we are the exclusive owner of our
//	implementation object, quickly set the implementation to an empty rect.
//	Otherwise, create a new empty implementation object.

void
EmRegion::BeEmpty (void)
{
	EmAssert (fImpl.get ());

	if (fImpl->fBuf)	// If not already empty
	{
		if (!fImpl->isShared () && fImpl->fBuf == fImpl->fRectBuf)
		{
			fImpl->fBuf = NULL;
			fImpl->fCapacity = 0;
			fImpl->fBounds.BeEmpty ();
		}
		else
		{
			fImpl = new EmRegionImpl;
		}
	}
}


// ---------------------------------------------------------------------------
//		* Bounds
// ---------------------------------------------------------------------------

const EmRect&
EmRegion::Bounds (void) const
{
	EmAssert (fImpl.get ());

	return fImpl->fBounds;
}


// ---------------------------------------------------------------------------
//		* GetRects
// ---------------------------------------------------------------------------

long
EmRegion::GetRects (EmRect* rp) const
{
	EmAssert (fImpl.get ());

	return fImpl->GetRects (rp);
}


// ---------------------------------------------------------------------------
//		* IsEmpty
// ---------------------------------------------------------------------------

Bool
EmRegion::IsEmpty (void) const
{
	EmAssert (fImpl.get ());

	return fImpl->fBounds.IsEmpty ();
}


// ---------------------------------------------------------------------------
//		* IsEqual
// ---------------------------------------------------------------------------

Bool
EmRegion::IsEqual (const EmRegion& other) const
{
	EmAssert (fImpl.get ());

	if (fImpl == other.fImpl)
		return true;

	return fImpl->IsEqual (*other.fImpl);
}


// ---------------------------------------------------------------------------
//		* Contains
// ---------------------------------------------------------------------------

Bool
EmRegion::Contains (const EmPoint& p) const
{
	EmAssert (fImpl.get ());

	return fImpl->Contains (p);
}


// ---------------------------------------------------------------------------
//		* Equality/inequality
// ---------------------------------------------------------------------------

Bool operator== (const EmRegion& r1, const EmRegion& r2)
{
	return r1.IsEqual (r2);
}

Bool operator!= (const EmRegion& r1, const EmRegion& r2)
{
	return !r1.IsEqual (r2);
}


// ---------------------------------------------------------------------------
//		* Offset
// ---------------------------------------------------------------------------

void
EmRegion::Offset (const EmPoint& pt)
{
	EmAssert (fImpl.get ());

	fImpl->Offset (pt.fX, pt.fY);
}

void
EmRegion::Offset (EmCoord dx, EmCoord dy)
{
	EmAssert (fImpl.get ());

	fImpl->Offset (dx, dy);
}


// ---------------------------------------------------------------------------
//		* Inset
// ---------------------------------------------------------------------------
//	Doing a horizontal inset is easy: Just take each x-event pair and move
//	the first one to the right and the second one to the left.
//
//	But wait, is it really so easy? What happens if the two x values cross
//	over each other? Then the pair must be discarded. Or what if the inset
//	operation is passed negative values and the x range now overlaps an
//	adjacent x range?
//
//	The easiest thing to do is convert the region into a series of rectangles,
//	perform the inset/outset on the left and right coordinates of the
//	rectangles, and then union the whole shebang back together, letting
//	the RegionOp function sort out overlaps and empty rectangles.
//
//	Once that's done, we have to figure out how to perform the vertical
//	inset operation. The easiest way is to turn the region 90 degrees and
//	perform another horizontal inset. To do the rotation, the region
//	is turned into rectangles, the rectangles are flipped around the X==Y
//	axis, and a new region is built up. That new region is inset, and
//	the resulting region is again flipped, giving us our final answer.

void
EmRegion::Inset (const EmPoint& pt)
{
	this->Inset (pt.fX, pt.fY);
}

void
EmRegion::Inset (EmCoord dx, EmCoord dy)
{
	EmRegion	newRgn, rectRgn;
	EmRect		r, r2;

	for (int ii = 0; ii < 2; ii++)
	{
		EmRegionRectIterator	iter1(*this);
		newRgn.BeEmpty();

		while (iter1.Next (r))
		{
			r.fLeft += dx;
			r.fRight -= dx;

			if (!r.IsEmpty ())
			{
				// !!! I can probably flip the rectangles here,
				// and forego doing it as a seperate loop below.
				rectRgn = r;
				newRgn.UnionWith (rectRgn);
			}
		}

		*this = newRgn;

		EmRegionRectIterator	iter2(*this);
		newRgn.BeEmpty();

		while (iter2.Next (r))
		{
			r2.fTop = r.fLeft;
			r2.fLeft = r.fTop;
			r2.fBottom = r.fRight;
			r2.fRight = r.fBottom;

			rectRgn = r2;
			newRgn.UnionWith (rectRgn);
		}

		*this = newRgn;

		dx = dy;
	}
}


// ---------------------------------------------------------------------------
//		* Operations
// ---------------------------------------------------------------------------

EmRegion&
EmRegion::UnionWith (const EmRegion& other)
{
	return *this = RegionOp (EmRegion::eUnion, *this, other);
}

EmRegion&
EmRegion::IntersectWith (const EmRegion& other)
{
	return *this = RegionOp (EmRegion::eIntersection, *this, other);
}

EmRegion&
EmRegion::Subtract (const EmRegion& other)
{
	return *this = RegionOp (EmRegion::eDifference, *this, other);
}

EmRegion&
EmRegion::XorWith (const EmRegion& other)
{
	return *this = Xor (*this, other);
}

EmRegion Union (const EmRegion& r1, const EmRegion& r2)
{
	return EmRegion::RegionOp (EmRegion::eUnion, r1, r2);
}

EmRegion Intersection (const EmRegion& r1, const EmRegion& r2)
{
	return EmRegion::RegionOp (EmRegion::eIntersection, r1, r2);
}

EmRegion Difference (const EmRegion& r1, const EmRegion& r2)
{
	return EmRegion::RegionOp (EmRegion::eDifference, r1, r2);
}

EmRegion Xor (const EmRegion& r1, const EmRegion& r2)
{
	return Difference (Union (r1, r2), Intersection (r1, r2));
}


// ---------------------------------------------------------------------------
//		* GetBuf
// ---------------------------------------------------------------------------

EmCoord*
EmRegion::GetBuf (void) const
{
	EmAssert (fImpl.get ());

	return fImpl->fBuf;
}


// ---------------------------------------------------------------------------
//		* Length
// ---------------------------------------------------------------------------

long
EmRegion::Length (void) const
{
	EmAssert (fImpl.get ());

	return fImpl->fCapacity;
}


// ---------------------------------------------------------------------------
//		* RegionOp
// ---------------------------------------------------------------------------
//	Scan over two regions, building a third region from them. As we scan
//	over the two regions, keep track of our state. We keep track of whether
//	or not we are in each of the regions. At each point where we transition
//	in or out of particular region, we optionally record an x event depending
//	on the operation.

EmRegion
EmRegion::RegionOp (EOpcode code, const EmRegion& r1, const EmRegion& r2)
{
	//
	// Do the easy cases first
	//
	switch (code)
	{
		case EmRegion::eUnion:
			if (r1.IsEmpty () && r2.IsEmpty ())
				return EmRegion();

			if (r1.IsEmpty ())
				return r2;

			if (r2.IsEmpty ())
				return r1;

			break;

		case EmRegion::eDifference:
			if (r1.IsEmpty ())
				return EmRegion ();

			if (r2.IsEmpty ())
				return r1;

			break;

		case EmRegion::eRevDifference:
			if (r2.IsEmpty ())
				return EmRegion ();

			if (r1.IsEmpty ())
				return r2;

			break;

		case EmRegion::eIntersection:
			if (r1.IsEmpty () || r2.IsEmpty ())
				return EmRegion ();

			break;
	}

	//
	// Get working pointers to our two input buffers.
	// "shape1" and "shape2" point to the beginnings
	// of the next scanlines to process.
	//
	EmCoord*	shape1 = r1.GetBuf ();
	EmCoord*	shape2 = r2.GetBuf ();

	//
	// Get a pointer to our destination buffer.
	// Use a buffer on the stack for speed if possible.
	//
	long buflen = (r1.Length () * r2.Length ());
	EmAssert (buflen > 0);

	EmCoord*	buf;
	EmCoord		stackBuf[100];				// !!! Bigger? Smaller?
	if (buflen > (long) countof (stackBuf))
		buf = new EmCoord[buflen];
	else
		buf = stackBuf;

	EmAssert(buf);

	EmCoord*	ss = buf;
	EmCoord*	oldss = NULL;

	EmCoord*	x1 = 0;
	EmCoord*	x2 = 0;
	EmCoord*	fixup;
	EmCoord		y, l1 = 0, l2 = 0, tl1, tl2;
	long		len, oldlen = -1;
	int			test;

	for (;;)
	{
		//
		// Get the lengths of the next scanlines to work with.
		//
		tl1 = *shape1;
		tl2 = *shape2;

		//
		// If they are both zero, we are at the ends of both regions,
		// and can quit.
		//
		if (tl1 == 0 && tl2 == 0)
			break;

		//
		// Set "test" based on which scanline is above the other.
		// If scanline1 is above scanline2, set "test" to negative.
		// If scanline1 is below scanline2, set "test" to positive.
		// If both scanlines are on the same line, set "test" to zero.
		// If the length of the scanline is zero, we are at the end
		// of the region, and so are conceptually at the bottom of
		// the coordinate space.
		//
		if (tl1 == 0)
			test = 1;
		else if (tl2 == 0)
			test = -1;
		else
			test = shape1[1] - shape2[1];

		//
		// Start processing the scanlines. Remember some information
		// on each one before merging them together.
		//
		if (test <= 0)
		{
			y = shape1[1];			// Remember y coordinate
			x1 = &shape1[2];		// Point to start of x-pairs
			shape1 += tl1 + 1;		// Bump to next scanline in this region
			l1 = tl1 - 1;			// Remember number of x-coordinates
		}

		if (test >= 0)
		{
			y = shape2[1];			// Remember y coordinate
			x2 = &shape2[2];		// Point to start of x-pairs
			shape2 += tl2 + 1;		// Bump to next scanline in this region
			l2 = tl2 - 1;			// Remember number of x-coordinates
		}

		//
		// Start outputting a new scanline. First, remember where it
		// starts so we can write the final length later. Next, write
		// an initial length of 1. Follow it with the y-coordinate of
		// the new scanline.
		//
		fixup = ss;
		*ss++ = 1;
		*ss++ = y;

		EmAssert(ss - buf < buflen);

		if (l1 == 0 && l2 == 0)
		{
			//
			// If there are no x events in either scanline, there's nothing
			// to merge together, so there's nothing to output.
			//
		}
		else if (l1 == 0)
		{
			//
			// The first region is depleted or we haven't reached its
			// first scanline yet, so work solely with the second
			// region. If we're doing a union or reverse difference
			// operation (where region A is subtracted from region B),
			// copy region B's data.
			//
			if (code == EmRegion::eUnion || code == EmRegion::eRevDifference)
			{
				memcpy (ss, x2, l2 * sizeof (EmCoord));
				ss += l2;
				EmAssert (ss - buf < buflen);
			}
		}
		else if (l2 == 0)
		{
			//
			// The second region is depleted or we haven't reached its
			// first scanline yet, so work solely with the first
			// region. If we're doing a union or difference
			// operation (where region B is subtracted from region A),
			// copy region A's data.
			//
			if (code == EmRegion::eUnion || code == EmRegion::eDifference)
			{
				memcpy (ss, x1, l1 * sizeof (EmCoord));
				ss += l1;
				EmAssert (ss - buf < buflen);
			}
		}
		else
		{
			//
			// Start merging two scanlines together. While there
			// are still x-pairs left to examine:
			//
			//	--	Compare an x-value from region A against an
			//		x-value from region B.
			//
			//	--	If Region A's is less than Region B's, bump
			//		to the next x-value in Region A and invert
			//		a state bit indicating whether or not we are
			//		"in" region A.
			//
			//	--	If Region B's is less than Region A's, bump
			//		to the next x-value in Region B and invert
			//		a state bit indicating whether or not we are
			//		"in" region B.
			//
			//	--	If both x-values are the same, bump to the next
			//		x-values in both regions, and invert the state
			//		bits for both regions.
			//
			//	--	Examine the state bits. They tell us when we are
			//		leaving one state and entering another. If we
			//		are entering a desired state (for instance, we
			//		are doing an intersection operation and both
			//		state bits are now on), emit an x-event in the
			//		destination region. Similarly, if we were just
			//		in a desired state and are now leaving it, again
			//		emit an x-event to close off the previous x-event.
			//
			// Note that our operation codes are cleverly chosen to
			// correspond to our state value. If we are doing an
			// intersection operation, for example, we are interested
			// in pixels that are in both Region A and Region B. Thus
			// we are interested in ranges where both state bits are
			// on. When both state bits are on, the state value is "3",
			// which also happens to be the value of eIntersection.
			//
			// As another example, consider the "difference" operation,
			// where Region B is subtracted from Region A. In that case,
			// we are interested in the pixels that are in Region A but
			// not in Region B. In other words, we are interested in
			// ranges where Region A's state bit (bit 0) is set and
			// Region B's state bit (bit 1) is clear. At that time, the
			// state value is "1", which also happens to be the value
			// of eDifference.
			//
			// The process of the eRevDifference operation is analogous.
			//
			// The process of the eUnion operation is a little
			// tricky. The value of eUnion is zero, which means that we
			// are interested in ranges that belong to _neither_ of the
			// two regions. That means that our resulting region logically
			// describes the pixels _outside_ the union of the two source
			// regions. However, it's all a matter of definition. Ultimately,
			// what we have is an outline. Whether we choose to be interested
			// in the bits "inside" the outline or "outside" the outline is
			// up to us. In other words, if the union of two regions happens
			// to result in a square, our algorithm will think its generating
			// the following region:
			//
			//		. . . . . . . . . . . . . . .
			//		 . . . . . . . . . . . . . . .
			//		. . . . . . . . . . . . . . .
			//		 . . . . +--------+. . . . . .
			//		. . . . .|        | . . . . .
			//		 . . . . |        |. . . . . .
			//		. . . . .|        | . . . . .
			//		 . . . . |        |. . . . . .
			//		. . . . .+--------+ . . . . .
			//		 . . . . . . . . . . . . . . .
			//		. . . . . . . . . . . . . . .
			//		 . . . . . . . . . . . . . . .
			//
			// But we are perfectly free to think of it as:
			//
			//		         +--------+
			//		         |. . . . |
			//		         | . . . .|
			//		         |. . . . |
			//		         | . . . .|
			//		         +--------+
			//
			EmCoord*	p1 = x1;
			EmCoord*	p2 = x2;
			EmCoord		xl1 = l1, xl2 = l2;
			EmCoord		x, xold = 0;
			int			xflag = 0;

			while (xl1 > 0 && xl2 > 0)
			{
				test = *p1 - *p2;

				if (test <= 0)
				{
					x = *p1++;
					xflag ^= 1;
					xl1--;
				}

				if (test >= 0)
				{
					x = *p2++;
					xflag ^= 2;
					xl2--;
				}

				if (xflag == code || xold == code)
				{
					*ss++ = x;
					EmAssert (ss - buf < buflen);
				}

				xold = xflag;
			}

			//
			// One of the scanlines has been exhausted. Determine what
			// to do with the remaining one based on the operation we're
			// performing. If we're doing a union or difference operation
			// and there's still data left in Region A's scanline, copy
			// that data to the destination. If we're doing a union or
			// reverse difference operation and there's still data in
			// Region B's scanline, copy that data to the destination.
			//
			if (code == EmRegion::eUnion || code == EmRegion::eDifference)
			{
				while (xl1-- > 0)
				{
					*ss++ = *p1++;
					EmAssert (ss - buf < buflen);
				}
			}

			if (code == EmRegion::eUnion || code == EmRegion::eRevDifference)
			{
				while (xl2-- > 0)
				{
					*ss++ = *p2++;
					EmAssert (ss - buf < buflen);
				}
			}
		}

		//
		// We've just merged two scanlines. First, determine the new
		// scanline's length and write it out to the fixup location.
		//
		len = ss - fixup - 2;
		*fixup = len + 1;

		//
		// Next, if the new scanline happens to be identical to the
		// previous scanline, don't bother recording it (reset our
		// "ss" pointer so that a subsequent scanline will overwrite
		// the one we're getting rid of).
		//
		if (len > 0 &&
			len == oldlen &&
			memcmp (&oldss[2], &fixup[2], len * sizeof (EmCoord)) == 0)
		{
			ss = fixup;
		}
		else
		{
			oldss = fixup;
			oldlen = len;
		}
	}

	//
	// Done with all the scanlines. Write out our terminator.
	//
	*ss++ = 0;

	EmAssert (ss - buf < buflen);

	//
	// Create our new region from the raw buffer.
	//
	EmRegion	result (buf, ss - buf);

	//
	// Delete our raw buffer if necessary.
	//
	if (buf != stackBuf)
		delete [] buf;

	return result;
}




// ---------------------------------------------------------------------------
//		* EmRegionRectIterator
// ---------------------------------------------------------------------------
//	Constructor. Note that "fRegion" is an EmRegion, _not_ a reference to an
//	EmRegion. This means that any changes to the source region will not affect
//	the iterator, as the iterator effectively makes a copy of the region
//	at construction time.

EmRegionRectIterator::EmRegionRectIterator (const EmRegion& r) :
	fRegion (r)
{
	this->Reset ();
}


// ---------------------------------------------------------------------------
//		* Reset
// ---------------------------------------------------------------------------

void
EmRegionRectIterator::Reset (void)
{
	fBufPtr = fRegion.GetBuf ();
	fNext = 0;
}


// ---------------------------------------------------------------------------
//		* Next
// ---------------------------------------------------------------------------
//	Return the next rectangle composing the region.
//
//	Iteration works as follows. The first time we're called, we notice the
//	fact and fetch information on the next scanline. That information
//	includes the scanline's y-coordinate and an offset to the next scanline.
//	The y-coordinate of the current scanline is our rectangle's top value,
//	while the y-coordinate of the next scanline is our rectangles bottom
//	value. Next, we start processing the rest of the scanline, which
//	contains x-coordinate pairs. Each pair makes up the left and right
//	values of the rectangle. Rectangles are formed until we reach the
//	end of the scanline, at which time we move to the next scanline. If
//	we run out of scanlines, we're done iterating.

Bool
EmRegionRectIterator::Next (EmRect& r)
{
	while (fBufPtr)
	{
		if (fNext > 1)
		{
			r.fLeft = *fBufPtr++;
			r.fRight = *fBufPtr++;
			fNext -= 2;

			EmAssert (fBufPtr <= fRegion.GetBuf () + fRegion.Length ());
			return true;
		}

		fNext = *fBufPtr++;
		if (fNext == 0)
		{
			EmAssert (fBufPtr <= fRegion.GetBuf () + fRegion.Length ());
			return false;
		}

		r.fTop = *fBufPtr++;
		r.fBottom = fBufPtr[fNext];
	}

	EmAssert (fBufPtr <= fRegion.GetBuf () + fRegion.Length ());

	return false;
}

#if 0
// ---------------------------------------------------------------------------
//		* Testing code
// ---------------------------------------------------------------------------
#include "stdio.h"

void	TestRegion();
void	PrintRegion(const EmRegion& r);
Bool	VerifyRegion(const char*, const EmRegion& r, EmRect rects[], int numRects);

void PrintRegion(const EmRegion& r)
{
	int ii = 0;
	EmRect	testRect;
	EmRegionRectIterator	iter(r);
	while (iter.Next(testRect))
	{
		ii++;
		printf("rect #%d: l = %ld, t = %ld, r = %ld, b = %ld\n", ii,
			testRect.fLeft, testRect.fTop, testRect.fRight, testRect.fBottom);
	}

	printf("\n");
}

Bool VerifyRegion(const char* testName, const EmRegion& r, EmRect rects[], int numRects)
{
	Bool	success = true;

	printf("Region test: %s\n", testName);

	int ii = 0;
	EmRect	testRect;
	EmRegionRectIterator	iter(r);
	while (iter.Next(testRect))
	{
		if (ii < numRects)
		{
			if (testRect != rects[ii])
			{
				printf("expected rect #%d: l = %ld, t = %ld, r = %ld, b = %ld\n", ii,
					rects[ii].fLeft, rects[ii].fTop, rects[ii].fRight, rects[ii].fBottom);
				printf("returned rect #%d: l = %ld, t = %ld, r = %ld, b = %ld\n", ii,
					testRect.fLeft, testRect.fTop, testRect.fRight, testRect.fBottom);

				success = false;
			}
		}
		else
		{
			printf("Iterator returned extra rectangle:\n");
			printf("returned rect #%d: l = %ld, t = %ld, r = %ld, b = %ld\n", ii,
				testRect.fLeft, testRect.fTop, testRect.fRight, testRect.fBottom);

			success = false;
		}
		ii++;
	}

	if (ii != numRects)
	{
		printf("Iterator returned %d rects, expected %d\n", ii, numRects);

		success = false;
	}

	printf("\n");

	return success;
}

void TestRegion()
{
	printf("Testing EmRegion class...\n");

	Bool success = true;

	EmRegion		testRegion1;
	EmRegion		testRegion2;

	testRegion1 = EmRegion(EmRect(0, 0, 0, 0));
	EmRect	resultRects1[] =
	{
		EmRect( 0, 0, 0, 0 )
	};
	success &= VerifyRegion("Set to empty rect", testRegion1, resultRects1, countof(resultRects1));

	testRegion2 = EmRegion(EmRect(1, 2, 3, 4));
	EmRect	resultRects2[] =
	{
		EmRect( 1, 2, 3, 4 )
	};
	success &= VerifyRegion("Set to non-empty rect", testRegion2, resultRects2, countof(resultRects2));

	testRegion2 = EmRegion(EmRect(5, 6, 7, 8));
	EmRect	resultRects3[] =
	{
		EmRect( 5, 6, 7, 8 )
	};
	success &= VerifyRegion("Reassign to non-empty rect", testRegion2, resultRects3, countof(resultRects3));

	EmRegion		testRegion(EmRect(0, 0, 10, 10));
	EmRegion		containedRegion(EmRect(3, 3, 7, 7));
	EmRegion		containingRegion(EmRect(-5, -5, 15, 15));
	EmRegion		disjointRegion(EmRect(15, 15, 25, 25));
	EmRegion		overlappingRegion(EmRect(5, 5, 15, 15));
	EmRegion		result;

	{
		//
		// Test Subtract
		//

		result = Difference(testRegion, containedRegion);
		EmRect	resultRects1[] =
		{
			EmRect( 0, 0, 10, 3 ),
			EmRect( 0, 3, 3, 7 ),
			EmRect( 7, 3, 10, 7 ),
			EmRect( 0, 7, 10, 10 )
		};
		success &= VerifyRegion("Difference/contained", result, resultRects1, countof(resultRects1));

		result = Difference(testRegion, containingRegion);
		success &= VerifyRegion("Difference/containing", result, NULL, 0);

		result = Difference(testRegion, disjointRegion);
		EmRect	resultRects3[] =
		{
			EmRect( 0, 0, 10, 10 )
		};
		success &= VerifyRegion("Difference/disjoint", result, resultRects3, countof(resultRects3));

		result = Difference(testRegion, overlappingRegion);
		EmRect	resultRects4[] =
		{
			EmRect( 0, 0, 10, 5 ),
			EmRect( 0, 5, 5, 10 )
		};
		success &= VerifyRegion("Difference/overlapping", result, resultRects4, countof(resultRects4));
	}

	{
		//
		// Test Intersection
		//

		result = Intersection(testRegion, containedRegion);
		EmRect	resultRects1[] =
		{
			EmRect( 3, 3, 7, 7 )
		};
		success &= VerifyRegion("Intersection/contained", result, resultRects1, countof(resultRects1));

		result = Intersection(testRegion, containingRegion);
		EmRect	resultRects2[] =
		{
			EmRect( 0, 0, 10, 10 )
		};
		success &= VerifyRegion("Intersection/containing", result, resultRects2, countof(resultRects2));

		result = Intersection(testRegion, disjointRegion);
		success &= VerifyRegion("Intersection/disjoint", result, NULL, 0);

		result = Intersection(testRegion, overlappingRegion);
		EmRect	resultRects4[] =
		{
			EmRect( 5, 5, 10, 10 )
		};
		success &= VerifyRegion("Intersection/overlapping", result, resultRects4, countof(resultRects4));
	}

	{
		//
		// Test Union
		//

		result = Union(testRegion, containedRegion);
		EmRect	resultRects1[] =
		{
			EmRect( 0, 0, 10, 10 )
		};
		success &= VerifyRegion("Union/contained", result, resultRects1, countof(resultRects1));

		result = Union(testRegion, containingRegion);
		EmRect	resultRects2[] =
		{
			EmRect( -5, -5, 15, 15 )
		};
		success &= VerifyRegion("Union/containing", result, resultRects2, countof(resultRects2));

		result = Union(testRegion, disjointRegion);
		EmRect	resultRects3[] =
		{
			EmRect( 0, 0, 10, 10 ),
			EmRect( 15, 15, 25, 25 )
		};
		success &= VerifyRegion("Union/disjoint", result, resultRects3, countof(resultRects3));

		result = Union(testRegion, overlappingRegion);
		EmRect	resultRects4[] =
		{
			EmRect( 0, 0, 10, 5 ),
			EmRect( 0, 5, 15, 10 ),
			EmRect( 5, 10, 15, 15 )
		};
		success &= VerifyRegion("Union/overlapping", result, resultRects4, countof(resultRects4));
	}

	{
		//
		// Test offset
		//

		EmRegion	rgn1(EmRect(10, 0, 40, 10));
		EmRegion	rgn2(EmRect(20, 0, 30, 5));

		result = Difference(rgn1, rgn2);
		result.Offset(1, 1);

		EmRect	resultRects1[] =
		{
			EmRect( 11, 1, 21, 6 ),
			EmRect( 31, 1, 41, 6 ),
			EmRect( 11, 6, 41, 11 )
		};
		success &= VerifyRegion("Offset", result, resultRects1, countof(resultRects1));

		// Test with different x and y values to make
		// sure I haven't swapped them.
		result = Difference(rgn1, rgn2);
		result.Offset(1, 2);

		EmRect	resultRects2[] =
		{
			EmRect( 11, 2, 21, 7 ),
			EmRect( 31, 2, 41, 7 ),
			EmRect( 11, 7, 41, 12 )
		};
		success &= VerifyRegion("Offset", result, resultRects2, countof(resultRects2));

		// Test with one of them zero to make sure I
		// don't improperly optimize.
		result = Difference(rgn1, rgn2);
		result.Offset(0, 2);

		EmRect	resultRects3[] =
		{
			EmRect( 10, 2, 20, 7 ),
			EmRect( 30, 2, 40, 7 ),
			EmRect( 10, 7, 40, 12 )
		};
		success &= VerifyRegion("Offset", result, resultRects3, countof(resultRects3));
	}

	{
		//
		// Test inset
		//

		EmRegion	rgn1(EmRect(10, 0, 40, 10));
		EmRegion	rgn2(EmRect(20, 0, 30, 5));

		result = Difference(rgn1, rgn2);
		result.Inset(1, 1);

		EmRect	resultRects[] =
		{
			EmRect( 11, 1, 19, 6 ),
			EmRect( 31, 1, 39, 6 ),
			EmRect( 11, 6, 39, 9 )
		};
		success &= VerifyRegion("Inset", result, resultRects, countof(resultRects));
	}

	if (success)
		printf("TestRegion() succeeded.\n");
	else
		printf("TestRegion() failed!\n");
}

#endif	// if 0

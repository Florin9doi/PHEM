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

#ifndef EmRegion_h
#define EmRegion_h

/*
	EmRegion is a cross-platform region class. It provides for the following needs:

		- A common API that can be used on both Mac and Windows

		- An internal 32-bit representation so that regions can be calculated
		  in a 32-bit coordinate plane.

		- Automatic management of heap structures so that you don't have to
		  remember to call DisposeRgn or DeleteObject.

	Use regions as you would something like EmRect. Create them on the stack,
	declare them as data members, pass them by reference, etc. For instance,
	ScriptViewCore uses regions as follows to determine the highlight area:

	{
		EmRegion	scratch;
		EmRect		aRect;

		// The selection's bounding rectangle
		aRect = ...;	// Calculate full bounds
		highlight = aRect;

		// ... minus beginning of the first line
		aRect = ...;	// Calculate leading area to subtract
		scratch = aRect;
		highlight.Subtract(scratch);

		// ... and the end of the last line
		aRect = ...;	// Calculate trailing area to subtract
		scratch = aRect;
		highlight.Subtract(scratch);
	}

	If you need to decompose the region into a series of rectangles, you can use
	the EmRegionRectIterator class for this. It can be used as follows:

		EmRegionRectIterator	iter(*this);
		EmRect					r;
		while (iter.Next(r))
		{
			// ...do something with the rect...
		}
	
	This region class is based on the region class from ET++ 3.2.2, used by
	permission from André Weinand:

			Regions: yes sure, just use the code and send me the bugs!

			--andre
			P.S.: my official life-long mail address is weinand@acm.org
*/

#include "EmPoint.h"			// EmPoint
#include "EmRect.h"				// EmRect
#include "EmRefCounted.h"


class EmRegion
{
	public:
								EmRegion	(void);
			// Creates a region spanning {0, 0, 0, 0}.

								EmRegion	(const EmRect& r);
			// Creates a region spanning the given rectangle.

								EmRegion	(const EmRegion& r);
			// Copy constructor. The internal data is copied via an internal
			// reference counted object, so cloning regions is quick.

								~EmRegion	(void);

		EmRegion&				operator=	(const EmRegion& r);
		EmRegion&				operator=	(const EmRect& r);
			// Assignment operator. Similar to copy constructor. There is also
			// a version that takes an EmRect as an r-value.

		void					BeEmpty		(void);
			// Quickly empty out a region so that you don't have to do anything
			// goofy like assigning a NULL rectangle to it.

		const EmRect&			Bounds		(void) const;
			// Returns an EmRect containing the bounds of the region.

		long					GetRects	(EmRect* r) const;
			// Decompose the region into a set of rectangles, returning the
			// number of rectangles. You pass in a pointer to a buffer that
			// will receive the rectangles. If you pass in NULL, no rectangles
			// will be returned, but you'll still get the rectangle count.
			// You should probably use a region iterator instead of this method
			// if you want to mess with the rectangles.

		Bool					IsEmpty		(void) const;
			// Returns whether or not the region encompases any area.

		Bool					IsEqual		(const EmRegion& r2) const;
			// Compares two regions, returning whether or not they encompass
			// the same area (set of pixels).

		Bool					Contains	(const EmPoint& p) const;
			// Returns whether or not the given point is in the interior of
			// the area described by the region. Testing is performed on the
			// normal half-open interval.

		friend inline Bool		operator==	(const EmRegion& r1, const EmRegion& r2);
		friend inline Bool		operator!=	(const EmRegion& r1, const EmRegion& r2);
			// Shortcuts for the IsEqual method

		void					Offset		(const EmPoint&);
		void					Offset		(EmCoord dx, EmCoord dy);
			// Offset the region by the given deltas. Positive deltas move
			// the region down and to the right.

		void					Inset		(const EmPoint&);
		void					Inset		(EmCoord dx, EmCoord dy);
			// Inset the region by the given deltas. Positive deltas move
			// the region inward.

		EmRegion&				UnionWith	(const EmRegion&);
		EmRegion&				IntersectWith(const EmRegion&);
		EmRegion&				Subtract	(const EmRegion&);
		EmRegion&				XorWith		(const EmRegion&);
			// Modify the region in place. Note that XorWith is synthesized from
			// the other operations, so it's not quite as effiecient as it
			// could be.

		friend inline EmRegion	Union		(const EmRegion& r1, const EmRegion& r2);
		friend inline EmRegion	Intersection(const EmRegion& r1, const EmRegion& r2);
		friend inline EmRegion	Difference	(const EmRegion& r1, const EmRegion& r2);
		friend inline EmRegion	Xor			(const EmRegion& r1, const EmRegion& r2);
			// Versions of the previous four operations, with the difference
			// being that neither of the given regions are modified. Instead,
			// a new region containing the result is returned.

	private:
		enum EOpcode
		{
			eUnion           = 0,
			eDifference      = 1,
			eRevDifference   = 2,
			eIntersection    = 3
		};

								EmRegion	(const EmCoord* s, long len);
		EmCoord*				GetBuf		(void) const;
		long					Length		(void) const;

		static EmRegion			RegionOp	(EOpcode code, const EmRegion& r1, const EmRegion& r2);

		friend class EmRegionRectIterator;

		class EmRegionImpl : public EmRefCounted
		{
			public:
										EmRegionImpl	(void);
										EmRegionImpl	(const EmCoord* s, long len);
										EmRegionImpl	(const EmRect& r);
										EmRegionImpl	(const EmRegion& r);
										~EmRegionImpl	(void);
		
				Bool					operator==		(const EmRegionImpl& other) const;

				long					GetRects		(EmRect* r) const;
				Bool					IsEqual			(const EmRegionImpl&) const;
				Bool					Contains		(const EmPoint& p) const;
		
				void					Offset			(EmCoord dx, EmCoord dy);
		
			protected:
				void					CalcBounds		(void);
		
			protected:
				friend class EmRegion;
		
				EmRect					fBounds;
				long					fCapacity;
				EmCoord*				fBuf;
				EmCoord					fRectBuf[7];
		};

		EmRefCounter<EmRegionImpl>	fImpl;
};

class EmRegionRectIterator
{
	public:
								EmRegionRectIterator	(const EmRegion& r);

		void					Reset					(void);
		Bool					Next					(EmRect& r);

	private:
		EmRegion				fRegion;
		EmCoord*				fBufPtr;
		long					fNext;
};


#endif	// EmRegion_h

/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1999-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#ifndef EmQuantizer_h
#define EmQuantizer_h

#include "EmStructs.h"			// RGBList

class EmPixMap;

struct NODE;

class EmQuantizer
{
	public:
								EmQuantizer			(uint32 nMaxColors, uint32 nColorBits);
		virtual					~EmQuantizer		(void);
	
		Bool					ProcessImage		(const EmPixMap&);
		Bool					ProcessColorTable	(const RGBList&);
		void					GetColorTable		(RGBList&);

	protected:
		void					AddColor			(NODE** ppNode, uint8 r, uint8 g, uint8 b,
													 uint32 nColorBits, uint32 nLevel,
													 uint32* pLeafCount, NODE** pReducibleNodes);
		NODE*					CreateNode			(uint32 nLevel, uint32 nColorBits,
													 uint32* pLeafCount, NODE** pReducibleNodes);
		void					ReduceTree			(uint32 nColorBits,
													 uint32* pLeafCount, NODE** pReducibleNodes);
		void					DeleteTree			(NODE** ppNode);
		void					GetPaletteColors	(NODE* pTree, RGBType* prgb, uint32* pIndex);

	protected:
		NODE*					m_pTree;
		uint32					m_nLeafCount;
		NODE*					m_pReducibleNodes[9];
		uint32					m_nMaxColors;
		uint32					m_nColorBits;
};

#endif	// EmQuantizer_h

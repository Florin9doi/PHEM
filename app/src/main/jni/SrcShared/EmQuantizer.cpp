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

#include "EmCommon.h"
#include "EmQuantizer.h"

#include "EmPixMap.h"
#include "Platform.h"			// Platform::AllocateMemory

#include <string.h>

struct NODE
{
	Bool	bIsLeaf;		// true if node has no children
	uint32	nPixelCount;	// Number of pixels represented by this leaf
	uint32	nRedSum;		// Sum of red components
	uint32	nGreenSum;		// Sum of green components
	uint32	nBlueSum;		// Sum of blue components
	NODE*	pChild[8];		// Pointers to child nodes
	NODE*	pNext;			// Pointer to next reducible node
};

EmQuantizer::EmQuantizer (uint32 nMaxColors, uint32 nColorBits)
{
	EmAssert (nColorBits <= 8);

	m_pTree			= NULL;
	m_nLeafCount	= 0;

	for (int ii = 0; ii <= (int) nColorBits; ++ii)
	{
		m_pReducibleNodes[ii] = NULL;
	}

	m_nMaxColors = nMaxColors;
	m_nColorBits = nColorBits;
}

EmQuantizer::~EmQuantizer ()
{
	if (m_pTree != NULL)
		this->DeleteTree (&m_pTree);
}

Bool EmQuantizer::ProcessImage (const EmPixMap& pixMap)
{
	uint8*			pbBits;
	uint8			r, g, b;
	int				i, j;

	EmPoint			size		= pixMap.GetSize ();
	EmPixMapFormat	bitFormat	= pixMap.GetFormat ();
	int				bitCount	= pixMap.GetDepth ();
	int				nPad		= pixMap.GetRowBytes () - (((size.fX * bitCount) + 7) / 8);

	switch (bitFormat)
	{
		case kPixMapFormat8:
		{
			const RGBList&	colors = pixMap.GetColorTable ();
			pbBits = (uint8*) pixMap.GetBits ();

			for (i = 0; i < size.fY; ++i)
			{
				for (j = 0; j < size.fX; ++j)
				{
					uint8			pixel	= *pbBits++;
					const RGBType&	color	= colors[pixel];

					b = color.fBlue;
					g = color.fGreen;
					r = color.fRed;

					this->AddColor (&m_pTree, r, g, b, m_nColorBits, 0, &m_nLeafCount,
						m_pReducibleNodes);

					while (m_nLeafCount > m_nMaxColors)
					{
						this->ReduceTree (m_nColorBits, &m_nLeafCount, m_pReducibleNodes);
					}
				}

				pbBits += nPad;
			}
			break;
		}

		case kPixMapFormat24RGB:
		{
			pbBits = (uint8*) pixMap.GetBits ();
			for (i = 0; i < size.fY; ++i)
			{
				for (j = 0; j < size.fX; ++j)
				{
					r = *pbBits++;
					g = *pbBits++;
					b = *pbBits++;

					this->AddColor (&m_pTree, r, g, b, m_nColorBits, 0, &m_nLeafCount,
						m_pReducibleNodes);

					while (m_nLeafCount > m_nMaxColors)
					{
						this->ReduceTree (m_nColorBits, &m_nLeafCount, m_pReducibleNodes);
					}
				}

				pbBits += nPad;
			}
			break;
		}

		case kPixMapFormat32ARGB:
		{
			pbBits = (uint8*) pixMap.GetBits ();
			for (i = 0; i < size.fY; ++i)
			{
				for (j = 0; j < size.fX; ++j)
				{
					pbBits++;		// Skip the alpha channel
					r = *pbBits++;
					g = *pbBits++;
					b = *pbBits++;

					this->AddColor (&m_pTree, r, g, b, m_nColorBits, 0, &m_nLeafCount,
						m_pReducibleNodes);

					while (m_nLeafCount > m_nMaxColors)
					{
						this->ReduceTree (m_nColorBits, &m_nLeafCount, m_pReducibleNodes);
					}
				}

				pbBits += nPad;
			}
			break;
		}

		default:
			EmAssert (false);
			return false;
	}

	return true;
}

Bool EmQuantizer::ProcessColorTable (const RGBList& colors)
{
	uint8			r, g, b;

	for (size_t ii = 0; ii < colors.size (); ++ii)
	{
		const RGBType&	color	= colors[ii];

		b = color.fBlue;
		g = color.fGreen;
		r = color.fRed;

		this->AddColor (&m_pTree, r, g, b, m_nColorBits, 0, &m_nLeafCount,
			m_pReducibleNodes);

		while (m_nLeafCount > m_nMaxColors)
		{
			this->ReduceTree (m_nColorBits, &m_nLeafCount, m_pReducibleNodes);
		}
	}

	return true;
}

void EmQuantizer::AddColor (NODE** ppNode, uint8 r, uint8 g, uint8 b,
	uint32 nColorBits, uint32 nLevel, uint32* pLeafCount, NODE** pReducibleNodes)
{
	//
	// If the node doesn't exist, create it.
	//
	if (*ppNode == NULL)
	{
		*ppNode = this->CreateNode (nLevel, nColorBits, pLeafCount, pReducibleNodes);
	}

	//
	// Update color information if it's a leaf node.
	//
	if ((*ppNode)->bIsLeaf)
	{
		(*ppNode)->nPixelCount++;
		(*ppNode)->nRedSum		+= r;
		(*ppNode)->nGreenSum	+= g;
		(*ppNode)->nBlueSum		+= b;
	}

	//
	// Recurse a level deeper if the node is not a leaf.
	//
	else
	{
		int shift = 7 - nLevel;
		int nIndex =
			(((r >> shift) & 1) << 2) |
			(((g >> shift) & 1) << 1) |
			(((b >> shift) & 1) << 0);

		this->AddColor (&((*ppNode)->pChild[nIndex]), r, g, b, nColorBits,
			nLevel + 1, pLeafCount, pReducibleNodes);
	}
}

NODE* EmQuantizer::CreateNode (uint32 nLevel, uint32 nColorBits, uint32* pLeafCount,
	NODE** pReducibleNodes)
{
	NODE* pNode;

	pNode = (NODE*) Platform::AllocateMemory (sizeof (NODE));
	if (pNode == NULL)
		return NULL;

	memset (pNode, 0, sizeof (NODE));

	pNode->bIsLeaf = (nLevel == nColorBits) ? true : false;
	if (pNode->bIsLeaf)
	{
		(*pLeafCount)++;
	}
	else
	{
		pNode->pNext = pReducibleNodes[nLevel];
		pReducibleNodes[nLevel] = pNode;
	}

	return pNode;
}

void EmQuantizer::ReduceTree (uint32 nColorBits, uint32* pLeafCount,
	NODE** pReducibleNodes)
{
	int ii;

	//
	// Find the deepest level containing at least one reducible node.
	//
	for (ii = nColorBits - 1; (ii > 0) && (pReducibleNodes[ii] == NULL); --ii)
		;

	//
	// Reduce the node most recently added to the list at level i.
	//
	NODE* pNode = pReducibleNodes[ii];
	pReducibleNodes[ii] = pNode->pNext;

	uint32 nRedSum		= 0;
	uint32 nGreenSum	= 0;
	uint32 nBlueSum		= 0;
	uint32 nChildren	= 0;

	for (ii = 0; ii < 8; ++ii)
	{
		if (pNode->pChild[ii] != NULL)
		{
			nRedSum				+= pNode->pChild[ii]->nRedSum;
			nGreenSum			+= pNode->pChild[ii]->nGreenSum;
			nBlueSum			+= pNode->pChild[ii]->nBlueSum;
			pNode->nPixelCount	+= pNode->pChild[ii]->nPixelCount;

			Platform::DisposeMemory (pNode->pChild[ii]);
			nChildren++;
		}
	}

	pNode->bIsLeaf		= true;
	pNode->nRedSum		= nRedSum;
	pNode->nGreenSum	= nGreenSum;
	pNode->nBlueSum		= nBlueSum;
	*pLeafCount			-= (nChildren - 1);
}

void EmQuantizer::DeleteTree (NODE** ppNode)
{
	for (int ii = 0; ii < 8; ++ii)
	{
		if ((*ppNode)->pChild[ii] != NULL)
		{
			this->DeleteTree (&((*ppNode)->pChild[ii]));
		}
	}

	Platform::DisposeMemory (*ppNode);
}

void EmQuantizer::GetPaletteColors (NODE* pTree, RGBType* prgb, uint32* pIndex)
{
	if (pTree->bIsLeaf)
	{
		prgb[*pIndex].fRed		= (uint8) ((pTree->nRedSum) / (pTree->nPixelCount));
		prgb[*pIndex].fGreen	= (uint8) ((pTree->nGreenSum) / (pTree->nPixelCount));
		prgb[*pIndex].fBlue		= (uint8) ((pTree->nBlueSum) / (pTree->nPixelCount));

		(*pIndex)++;
	}
	else
	{
		for (int ii = 0; ii < 8; ++ii)
		{
			if (pTree->pChild[ii] != NULL)
			{
				this->GetPaletteColors (pTree->pChild[ii], prgb, pIndex);
			}
		}
	}
}

void EmQuantizer::GetColorTable (RGBList& colors)
{
	colors.clear ();
	colors.resize (m_nLeafCount);

	uint32 nIndex = 0;
	this->GetPaletteColors (m_pTree, &colors[0], &nIndex);
}

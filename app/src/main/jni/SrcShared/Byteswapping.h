/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#ifndef _BYTESWAPPING_H_
#define _BYTESWAPPING_H_

#if !defined (BYTESWAP)
#error "Must define BYTESWAP"
#endif


struct regstruct;


/*
**	--------------------------------------------------------------------------------
**		Byte swapping
**	--------------------------------------------------------------------------------
*/

#define BYTE_SWAP_16(n)	((((uint16) n) >> 8) |	\
						 (((uint16) n) << 8))

#define BYTE_SWAP_32(n)	( (((uint32) n) << 24) |				\
						 ((((uint32) n) << 8) & 0x00FF0000) |	\
						 ((((uint32) n) >> 8) & 0x0000FF00) |	\
						  (((uint32) n) >> 24))

#ifdef _MSC_VER
#define BYTE_SWAP_64(n)	( (((uint64) n) << 56) |							\
						 ((((uint64) n) << 40) & 0x00FF000000000000i64) |	\
						 ((((uint64) n) << 24) & 0x0000FF0000000000i64) |	\
						 ((((uint64) n) <<  8) & 0x000000FF00000000i64) |	\
						 ((((uint64) n) >>  8) & 0x00000000FF000000i64) |	\
						 ((((uint64) n) >> 24) & 0x0000000000FF0000i64) |	\
						 ((((uint64) n) >> 40) & 0x000000000000FF00i64) |	\
						  (((uint64) n) >> 56))
#else
#define BYTE_SWAP_64(n)	( (((uint64) n) << 56) |							\
						 ((((uint64) n) << 40) & 0x00FF000000000000LL) |	\
						 ((((uint64) n) << 24) & 0x0000FF0000000000LL) |	\
						 ((((uint64) n) <<  8) & 0x000000FF00000000LL) |	\
						 ((((uint64) n) >>  8) & 0x00000000FF000000LL) |	\
						 ((((uint64) n) >> 24) & 0x0000000000FF0000LL) |	\
						 ((((uint64) n) >> 40) & 0x000000000000FF00LL) |	\
						  (((uint64) n) >> 56))
#endif


inline void Byteswap(bool&)
{
}

inline void Byteswap(char&)
{
}

inline void Byteswap(signed char&)
{
}

inline void Byteswap(unsigned char&)
{
}

inline void Byteswap(short& v)
{
	v = (short) BYTE_SWAP_16(v);
}

inline void Byteswap(unsigned short& v)
{
	v = (unsigned short) BYTE_SWAP_16(v);
}

inline void Byteswap(int& v)
{
	if (sizeof(int) == 4)
		v = (int) BYTE_SWAP_32(v);

	else if (sizeof(int) == 2)
		v = (int) BYTE_SWAP_16(v);
}

inline void Byteswap(unsigned int& v)
{
	if (sizeof(unsigned int) == 4)
		v = (unsigned int) BYTE_SWAP_32(v);

	else if (sizeof(unsigned int) == 2)
		v = (unsigned int) BYTE_SWAP_16(v);
}

inline void Byteswap(long& v)
{
	v = (long) BYTE_SWAP_32(v);
}

inline void Byteswap(unsigned long& v)
{
	v = (unsigned long) BYTE_SWAP_32(v);
}

inline void Byteswap(int64& v)
{
	v = (int64) BYTE_SWAP_64(v);
}

inline void Byteswap(uint64& v)
{
	v = (uint64) BYTE_SWAP_64(v);
}

template <class T>
inline void Byteswap(T*& v)
{
	Byteswap ((uint32&) v);
}

void Byteswap (HwrM68328Type&);
void Byteswap (HwrM68EZ328Type&);
void Byteswap (HwrM68VZ328Type&);
void Byteswap (regstruct& r);
void Byteswap (SED1375RegsType& p);

#if BYTESWAP

	template <class T>
	inline void Canonical(T& v)
	{
		Byteswap (v);
	}

#else

	template <class T>
	inline void Canonical(T&)
	{
	}

#endif


#if WORDSWAP_MEMORY

	inline void ByteswapWords (void* start, unsigned long length)
	{
		for (unsigned long ii = 0; ii < length / 2; ++ii)
		{
			unsigned short*	p = ((unsigned short*) start) + ii;
			unsigned char*	bp = (unsigned char*) p;
			*p = (((unsigned short) *bp) << 8) | (unsigned short) (*(bp + 1));
		}
	}

#else

	inline void ByteswapWords (void*, unsigned long)
	{
	}

#endif	// WORDSWAP_MEMORY

#endif /* _BYTESWAPPING_H_ */


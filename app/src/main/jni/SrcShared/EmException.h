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

#ifndef EmException_h
#define EmException_h

#include <exception>
#include <string>

// ---------- EmException ----------

class EmException : public exception
{
	public:
								EmException			(void) throw ();
		virtual					~EmException		(void) throw ();
};


// ---------- EmExceptionTopLevelAction ----------
// This class of exception is used to escape back
// to the top level of the CPU loop and optionally
// perform some action there.

class EmExceptionTopLevelAction : public EmException
{
	public:
								EmExceptionTopLevelAction	(void) throw ();
		virtual					~EmExceptionTopLevelAction	(void) throw ();

		virtual void			DoAction			(void) = 0;
};


// ---------- EmExceptionEnterDebugger ----------

class EmExceptionEnterDebugger : public EmExceptionTopLevelAction
{
	public:
								EmExceptionEnterDebugger	(void) throw ();
		virtual					~EmExceptionEnterDebugger	(void) throw ();

		virtual void			DoAction			(void);
};


// ---------- EmExceptionReset ----------

class EmExceptionReset : public EmExceptionTopLevelAction
{
	public:
								EmExceptionReset	(EmResetType) throw ();
		virtual					~EmExceptionReset	(void) throw ();

		virtual void			DoAction			(void);

		void					SetException		(uint16 e)		{ fException = e; }
		void					SetMessage			(const char* m)	{ fMessage = m; }
		void					SetTrapWord			(uint16 tw)		{ fTrapWord = tw; }

		virtual const char*		what				(void) const throw ();

		void					Display				(void) const;

	private:
		EmResetType				fType;
		mutable string			fWhat;
		string					fMessage;
		uint16					fException;
		uint16					fTrapWord;
};


// ---------- EmExceptionNextGremlin ----------

class EmExceptionNextGremlin : public EmExceptionTopLevelAction
{
	public:
								EmExceptionNextGremlin	(void) throw ();
		virtual					~EmExceptionNextGremlin	(void) throw ();

		virtual void			DoAction			(void);
};


#endif	// EmException_h

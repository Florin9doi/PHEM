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

#ifndef EmAction_h
#define EmAction_h

#include "omnithread.h"			// omni_mutex

#include <vector>

class EmAction;
typedef vector <EmAction*>	EmActionList;

class EmAction
{
	public:
								EmAction			(StrCode strCode) : fStrCode (strCode) {}
		virtual					~EmAction			(void) {}

		virtual void			Do					(void) = 0;

		StrCode					GetDescription		(void) const { return fStrCode; };

	private:
		StrCode					fStrCode;
};


class EmActionComposite : public EmAction
{
	public:
								EmActionComposite	(void);
		virtual					~EmActionComposite	(void);

		virtual void			Do					(void);

		void					AddAction			(EmAction*);

	private:
		EmActionList			fActions;
};


class EmActionHandler
{
	public:
								EmActionHandler		(void);
		virtual					~EmActionHandler	(void);

	public:
		void					PostAction			(EmAction*);
		EmAction*				GetNextAction		(void);
		void					DoAll				(void);

	private:
		void					DeleteAll			(void);

	private:
		omni_mutex				fMutex;
		EmActionList			fActions;
};

#endif	// EmAction

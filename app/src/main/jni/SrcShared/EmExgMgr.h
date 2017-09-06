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

#ifndef EmExgMgr_h
#define EmExgMgr_h

#include <string>

class EmFileImport;
class EmStream;

class EmExgMgr
{
	public:
								EmExgMgr			(void);
		virtual					~EmExgMgr			(void);

		static EmExgMgr*		GetExgMgr			(UInt16 libRefNum);

	public:
		virtual Err				ExgLibOpen			(UInt16 libRefNum) = 0;
		virtual Err				ExgLibClose			(UInt16 libRefNum) = 0;
		virtual Err				ExgLibSleep			(UInt16 libRefNum) = 0;
		virtual Err				ExgLibWake			(UInt16 libRefNum) = 0;
		virtual Boolean			ExgLibHandleEvent	(UInt16 libRefNum, emuptr eventP) = 0;
		virtual Err				ExgLibConnect		(UInt16 libRefNum, emuptr exgSocketP) = 0;
		virtual Err				ExgLibAccept		(UInt16 libRefNum, emuptr exgSocketP) = 0;
		virtual Err				ExgLibDisconnect	(UInt16 libRefNum, emuptr exgSocketP, Err error) = 0;
		virtual Err				ExgLibPut			(UInt16 libRefNum, emuptr exgSocketP) = 0;
		virtual Err				ExgLibGet			(UInt16 libRefNum, emuptr exgSocketP) = 0;
		virtual UInt32			ExgLibSend			(UInt16 libRefNum, emuptr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP) = 0;
		virtual UInt32			ExgLibReceive		(UInt16 libRefNum, emuptr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP) = 0;
		virtual Err				ExgLibControl		(UInt16 libRefNum, UInt16 op, emuptr valueP, emuptr valueLenP) = 0;
		virtual Err				ExgLibRequest		(UInt16 libRefNum, emuptr exgSocketP) = 0;
};


class EmExgMgrStream : public EmExgMgr
{
	public:
								EmExgMgrStream		(EmStream&);
		virtual					~EmExgMgrStream		(void);

	public:
		virtual Err				ExgLibOpen			(UInt16 libRefNum);
		virtual Err				ExgLibClose			(UInt16 libRefNum);
		virtual Err				ExgLibSleep			(UInt16 libRefNum);
		virtual Err				ExgLibWake			(UInt16 libRefNum);
		virtual Boolean			ExgLibHandleEvent	(UInt16 libRefNum, emuptr eventP);
		virtual Err				ExgLibConnect		(UInt16 libRefNum, emuptr exgSocketP);
		virtual Err				ExgLibAccept		(UInt16 libRefNum, emuptr exgSocketP);
		virtual Err				ExgLibDisconnect	(UInt16 libRefNum, emuptr exgSocketP,Err error);
		virtual Err				ExgLibPut			(UInt16 libRefNum, emuptr exgSocketP);
		virtual Err				ExgLibGet			(UInt16 libRefNum, emuptr exgSocketP);
		virtual UInt32			ExgLibSend			(UInt16 libRefNum, emuptr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP);
		virtual UInt32			ExgLibReceive		(UInt16 libRefNum, emuptr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP);
		virtual Err				ExgLibControl		(UInt16 libRefNum, UInt16 op, emuptr valueP, emuptr valueLenP);
		virtual Err				ExgLibRequest		(UInt16 libRefNum, emuptr exgSocketP);

	private:
		EmStream&				fStream;
		string					fFileName;
};


class EmExgMgrImportWrapper : public EmExgMgr
{
	public:
								EmExgMgrImportWrapper	(EmExgMgr&, EmFileImport&);
		virtual					~EmExgMgrImportWrapper	(void);

		void					Cancel				(void);

	public:
		virtual Err				ExgLibOpen			(UInt16 libRefNum);
		virtual Err				ExgLibClose			(UInt16 libRefNum);
		virtual Err				ExgLibSleep			(UInt16 libRefNum);
		virtual Err				ExgLibWake			(UInt16 libRefNum);
		virtual Boolean			ExgLibHandleEvent	(UInt16 libRefNum, emuptr eventP);
		virtual Err				ExgLibConnect		(UInt16 libRefNum, emuptr exgSocketP);
		virtual Err				ExgLibAccept		(UInt16 libRefNum, emuptr exgSocketP);
		virtual Err				ExgLibDisconnect	(UInt16 libRefNum, emuptr exgSocketP, Err error);
		virtual Err				ExgLibPut			(UInt16 libRefNum, emuptr exgSocketP);
		virtual Err				ExgLibGet			(UInt16 libRefNum, emuptr exgSocketP);
		virtual UInt32			ExgLibSend			(UInt16 libRefNum, emuptr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP);
		virtual UInt32			ExgLibReceive		(UInt16 libRefNum, emuptr exgSocketP, void* bufP, const UInt32 bufLen, Err* errP);
		virtual Err				ExgLibControl		(UInt16 libRefNum, UInt16 op, emuptr valueP, emuptr valueLenP);
		virtual Err				ExgLibRequest		(UInt16 libRefNum, emuptr exgSocketP);

	private:
		EmExgMgr&				fExgMgr;
		EmFileImport&			fImporter;
		Bool					fAborting;
};

#endif	// EmExgMgr_h

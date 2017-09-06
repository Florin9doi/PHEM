/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
	Copyright (c) 2001 PocketPyro, Inc.
	All rights reserved.

	This file is part of the Palm OS Emulator.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
\* ===================================================================== */

#ifndef EcmObject_h
#define EcmObject_h

#include "EcmIf.h"

/*
	This file define a concrete implementation of the IEcmComponent
	interface, as defined in EcmIf.h.  IEcmComponent describes an object
	with RequestInterface, Refer, and Release functions.  EcmObject
	provides implementations of those functions that find the requested
	interface, increment a refcount, and decrement a refcount (deleting
	the object if necessary), respectively.

	RequestInterface works by making use of the FindInterface helper
	function.  FindInterface is implemented by all EcmObject subclasses
	to cast itself to the right type and return the casted pointer if
	the subclass supports the requested interface.

	In order to make the implementation of FindInterface in all subclasses
	simple and consistant, three macros are provided: ECM_CLASS_IF_LIST_BEGIN,
	ECM_CLASS_IF, and ECM_CLASS_IF_LIST_END.  The first macro introduces
	a list of interfaces a particular class supports, the second macro is
	used once for each interface supported, and the final macro cleans up
	the list.  In all, the three macros as used as follows in the class
	declaration of the class implementing an interface:

		class EmImplementation : public EcmObject,
			ecm_implements IEmBaseInterface1,
			ecm_implements IEmBaseInterface2
		{
			public:

				ECM_CLASS_IF_LIST_BEGIN(EmImplementation, EcmObject)
					ECM_CLASS_IF(kBaseIfn1, IEmBaseInterface1)
					ECM_CLASS_IF(kBaseIfn2, IEmBaseInterface2)
				ECM_CLASS_IF_LIST_END(EmImplementation, EcmObject)

			...
		};
*/


// ===========================================================================
//
//		Extended Component Model (ECM) -- Base Class
//
//		We're adding support for component interfaces because 
//			exporting  c++ classes is a real pain from loadable libraries 
//			(at least Windows DLLs), Anyway, component design using interfaces
//			works really well for "plugin" type designs.
//
//		In our implementation, an interface is a c++ abstract base class.
// ===========================================================================


#ifdef _MSC_VER
#pragma warning( disable : 4250 )

	// 'class1' : inherits 'class2::member' via dominance
	//
	// There were two or more members with the same name. The one in class2
	// was inherited since it was a base class for the other classes that
	// contained this member.
#endif


class EcmObject : 
	ecm_implements IEcmComponent
{
	public:
		EcmObject() : fRefCount(0) 
		{
		}



		//This forms the root of the interface request implementation:
		//	Derived classes will implement the same function, then a request for an interface will move up
		//  through the ranks, until a match has been found.
		//
		virtual EcmErr FindInterface(const EcmIfName &name, void **iPP)
		{
			if (iPP == NULL)
				return kEcmErrInvalidParameter;

			if (name == kEcmComponentIfn)
			{
				*iPP = (void *) static_cast<IEcmComponent *>(this);
				return kEcmErrNone;
			}

			//Interface never found:
			return kEcmErrInvalidIfName;
		}




		/***********************************************************************
		 *
		 * FUNCTION:	RequestInterface
		 *
		 * DESCRIPTION:	Requests an interface of type EcmIfName from the component
		 *
		 * PARAMETERS:	name	[IN   ] Name of the interface being requested.
		 *				iPP		[OUT  ] Interface to the event being sent.
		 *
		 * RETURNED:	kEcmErrNone
		 *				kEcmErrInvalidName
		 *
		 *
		 ***********************************************************************/
		virtual EcmErr RequestInterface(const EcmIfName &name, void **iPP)
		{
			EcmErr err = FindInterface(name, iPP);

			if (err == kEcmErrNone)
			{
				Refer();
			}

			return err;
		}


		/***********************************************************************
		 *
		 * FUNCTION:	Refer
		 *
		 * DESCRIPTION:	Called before handing an interface pointer off to another 
		 *					"piece of code", adding an addition owner.  This is used to
		 *					maintain the reference count so the component can be destroyed
		 *					at the appropriate time.
		 *
		 * PARAMETERS:	none
		 *
		 * RETURNED:	kEcmErrNone
		 *
		 *
		 ***********************************************************************/

		virtual EcmErr Refer()
		{
			fRefCount++;
			return kEcmErrNone;
		}
		

		/***********************************************************************
		 *
		 * FUNCTION:	Release
		 *
		 * DESCRIPTION:	Releases the interface from a reference, when the reference count is 0,
		 *					the component can be destroyed.
		 *
		 * PARAMETERS:	none
		 *
		 * RETURNED:	kEcmErrNone
		 *
		 *
		 ***********************************************************************/

		virtual EcmErr Release()
		{
			fRefCount--;

			if (fRefCount == 0)
				delete this;

			return kEcmErrNone;
		}

	protected:
		unsigned long fRefCount;
};


#endif // EcmObject_h

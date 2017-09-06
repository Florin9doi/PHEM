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


#ifndef EcmIf_h
#define EcmIf_h

#include <string>

/*
	This file is the root file for ECM, the Extended Component Model
	system used within the Palm OS Emulator.

	ECM is a COM-like component system.  "Packages" (that is, plug-ins
	in the form of DLLs and the like) publicize their functionality in
	the form of "interfaces".  Packages can be queried (that is, the
	module can be asked for a particular interface by name, and the
	module can either return the interface, or fail, indicating that the
	requested facility is not supported).

	Interfaces are expressed as a table of function pointers.  As such,
	they are very similar to C++ vtables, are are often created using
	C++ class definitions.

	Every interface has base functionality.  That is, every interface
	starts off its function table with three standard functions:
	RequestInterface, Refer, and Release.  These three functions are
	defined as member functions of the pure virtual class IEcmComponent.
	Therefore, for consistancy, all C++ classes defining interfaces
	should ultimately descend from IEcmComponent.

	The three methods in IEcmComponent are pure virtual, and so need to
	be implemented in the class descending from it.  The implementation
	of these functions will likely be the same for all interfaces, and
	so there is a concrete subclass of IEcmComponent that provides a
	standard implementation.  This subclass is EcmObject.  Therefore,
	most interface implementation classes will probably descend from
	EcmObject and not directly from IEcmComponent.

	When an interface is defined (that is, the set of functions in the
	function table is established), it is introduced with the macro
	"ecm_interface".  This tells the reader that the following C++
	struct/class merely defines a set of functions for a client to use.
	Thus a new interface would look as follows:

		ecm_interface MyBaseInterface
		{
			virtual EcmErr MyFunction1() = 0;
			virtual EcmErr MyFunction2() = 0;
			virtual EcmErr MyFunction3() = 0;
		};

	When an interface needs to be extended (that is, a base interface
	needs to have some more functions added to it), the interface
	is again introduced with "ecm_interface", but is followed by the
	macro "ecm_extends" in the place where a C++ class indicates the
	base class it descends from.  Thus, an interface that inherited
	from a base interface would look as follows:

		ecm_interface MyDerivedInterface : ecm_extends MyBaseInterface
		{
			virtual EcmErr MyFunction4() = 0;
			virtual EcmErr MyFunction5() = 0;
			virtual EcmErr MyFunction6() = 0;
		};

	So far, we've just defined interfaces: the layout of the function
	pointer table.  We also need to implement the functions.  To do
	that, we create a class that descends from the interface we want
	to implement, using the macro "ecm_implements".  Thus, to implement
	the functions described by MyDerivedInterface, we use the following:

		class MyDerivedImplementation : ecm_implements MyDerivedInterface
		{
			virtual EcmErr MyFunction1();
			virtual EcmErr MyFunction2();
			virtual EcmErr MyFunction3();
			virtual EcmErr MyFunction4();
			virtual EcmErr MyFunction5();
			virtual EcmErr MyFunction6();
		};

	Once all of these interface and implementation classes are defined,
	they need to be used somehow.  The plug-in needs to be given a way
	to get to the interfaces it needs, and the container for the plug-in
	needs to know what interfaces the plug-in supports.

	This exchange of interface information takes place when the plug-in
	is loaded.  The plug-in needs to support an entry point that will
	(a) accept a container object and (b) return a package object.  The
	as with all interfaces, these objects contain RequestInterface methods
	from with all other interfaces can be obtained.

	Obtaining an interface using RequestInterface is simple: you pass a
	"name" of an interface to some ECM object, and that object will
	say that it supports or doesn't support that interface.  If it does
	support that interface, it returns a pointer to the interface.
*/


#define ecm_interface	struct
#define ecm_implements	virtual public
#define ecm_extends		virtual public


#define ECM_CLASS_IF_LIST_BEGIN(classname, basename)					\
	virtual EcmErr FindInterface(const EcmIfName &name, void **iPP)		\
	{																	\
		if (iPP == NULL)												\
			return kEcmErrInvalidParameter;


#define ECM_CLASS_IF(ifid, iftype)										\
		if (name == (ifid))												\
		{																\
			*iPP = (void *) static_cast<iftype *>(this);				\
			return kEcmErrNone;											\
		}


#define ECM_CLASS_IF_LIST_END(classname, basename)						\
		  return(basename::FindInterface(name, iPP));					\
	}



// ===========================================================================
//
//		Extended Component Model (ECM) -- Interfaces
//
//		We're adding support for component interfaces because 
//			exporting  c++ classes is a real pain from loadable libraries 
//			(at least Windows DLLs), Anyway, component design using interfaces
//			works really well for "plugin" type designs.
//
//		In our implementation, an interface is a c++ abstract base class.
// ===========================================================================


enum EcmErr
{
	kEcmErrNone,
	kEcmErrYikes,				// if this shows up, something really unexpected happened!
								// flag any appearance of this return code as a defect.

	kEcmErrInvalidParameter,	// a parameter was not set up correctly
	kEcmErrInvalidHandle,		// handle doesn't refer to anything appropriate
	kEcmErrInvalidIfName,		// invalid name of interface or event interface
	kEcmErrInvalidPubName,		// invalid name of publisher
	kEcmErrInvalidSrvName,		// invalid name of service
	kEcmErrInvalidCompName,		// invalid name of component
	kEcmErrBadEventType,		// inapropriate event type
	kEcmErrInvalidConnection,	//
	kEcmErrAlreadyConnected,	//
	kEcmErrNotInitialized		//
};




// ===========================================================================
//		EcmIfName -- type to define name used to reference an interface.
//			NOTE: If ECM becomes more widely used, this can be optimized to use an
//			atom table of strings.  An actual string comparison would then only need
//			to be done once.
// ===========================================================================

typedef string	EcmIfName;
typedef string	EcmCompName;


// ===========================================================================
//		EcmHandle -- Handle used for opaque references
// ===========================================================================

typedef void*	EcmHandle;



// ===========================================================================
//		IEcmComponent all interfaces must extend IEcmComponent..
//			this is how all patch modules appear to the patching sub-system
// ===========================================================================

const EcmIfName kEcmComponentIfn = "component.i.ecm";

ecm_interface IEcmComponent
{
	/***********************************************************************
	 *
	 * FUNCTION:	RequestInterface
	 *
	 * DESCRIPTION:	Requests an interface of type EcmIfName from the component
	 *
	 * PARAMETERS:	name	[IN   ] Name of the interface being requested.
	 *				iPP		[OUT  ] Interface to the event being sent.
	 *
	 * RETURNED:	ECMErrNone
	 *				ECMErrInvalidName
	 *
	 *
	 ***********************************************************************/

	virtual EcmErr RequestInterface(const EcmIfName &name, void **iPP) = 0;


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
	 * RETURNED:	ECMErrNone
	 *
	 *
	 ***********************************************************************/

    virtual EcmErr Refer() = 0;


	/***********************************************************************
	 *
	 * FUNCTION:	Release
	 *
	 * DESCRIPTION:	Releases the interface from a reference, when the reference count is 0,
	 *					the component can be destroyed.
	 *
	 * PARAMETERS:	none
	 *
	 * RETURNED:	ECMErrNone
	 *
	 *
	 ***********************************************************************/

    virtual EcmErr Release() = 0;
};



// ===========================================================================
//		IEcmContainer
// ===========================================================================

const EcmIfName kEcmContainerIfn = "container.i.ecm";

ecm_interface IEcmContainer : ecm_extends IEcmComponent
{
};



// ===========================================================================
//		IEcmPackage
// ===========================================================================

const EcmIfName kEcmPackageIfn = "package.i.ecm";

ecm_interface IEcmPackage : ecm_extends IEcmComponent
{
};



// ===========================================================================
//		IEcmEventBase
// ===========================================================================

const EcmIfName kEcmEventBaseIfn = "eventbase.events.i.ecm";

ecm_interface IEcmEventBase : ecm_extends IEcmComponent
{


};



// ===========================================================================
//		IEcmEventListener
// ===========================================================================

const EcmIfName kEcmEventListenerIfn = "listener.events.i.ecm";

/**
 **  Base interface for all event sinks.
 **/
ecm_interface IEcmEventListener
{
	/***********************************************************************
	 *
	 * FUNCTION:	OnEvent
	 *
	 * DESCRIPTION:	Delivers an event to the event listener
	 *
	 * PARAMETERS:	evtIP		[IN   ] Interface to the event being sent.
	 *
	 * RETURNED:	ECMErrNone
	 *				ECMErrBadEventType
	 *
	 *
	 ***********************************************************************/

    EcmErr OnEvent(const IEcmEventBase *evtIP);
};


// ===========================================================================
//		IEcmEvents
// ===========================================================================

const EcmIfName kEcmEventsIfn = "events.i.ecm";

ecm_interface IEcmEvents
{
	/***********************************************************************
	 *
	 * FUNCTION:	RequestListener
	 *
	 * DESCRIPTION:	Obtains an event listener interface (for a "category of events")
	 *
	 * PARAMETERS:	name			[IN   ] Name of the event interface being requested.
	 *				listenerIPP		[OUT  ] Interface to the given event listener.
	 *
	 * RETURNED:	ECMErrNone
	 *				ECMErrInvalidParameter
	 *				ECMErrInvalidName
	 *
	 *
	 ***********************************************************************/

    virtual EcmErr RequestListener(const EcmIfName &name, void **listenerIPP) = 0;


	/***********************************************************************
	 *
	 * FUNCTION:	Subscribe
	 *
	 * DESCRIPTION:	Allows an event "consumer" to subscribe to the named "category of events"
	 *
	 * PARAMETERS:	publisher		[IN   ] Name of the event publisher being requested.
	 *				subscriberIP	[IN   ] Interface to the subscribers event listener interface
	 *				handle			[OUT  ] Unique identifier handle associated with the subscription.
	 *
	 * RETURNED:	ECMErrNone
	 *				ECMErrInvalidParameter
	 *				ECMErrInvalidName
	 *
	 *
	 ***********************************************************************/

    virtual EcmErr Subscribe(const EcmIfName &publisher, const IEcmEventListener *subscriberIP, EcmHandle &handle) = 0;


	/***********************************************************************
	 *
	 * FUNCTION:	Unsubscribe
	 *
	 * DESCRIPTION:	Unsubscribes from an event publisher.
	 *
	 * PARAMETERS:	handle			[IN   ] Unique identifier created by "IEcmComponent::Subscribe".
	 *
	 * RETURNED:	ECMErrNone
	 *				ECMErrInvalidHandle
	 *
	 *
	 ***********************************************************************/

    virtual EcmErr Unsubscribe(const EcmHandle handle) = 0;

};


#endif // EcmIf_h

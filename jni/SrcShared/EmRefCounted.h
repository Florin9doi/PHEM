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

#ifndef EmRefCounted_h
#define EmRefCounted_h

/*
	EmRefCounted is a base class for any other class that you wish to be
	reference counted. Simply descend from this guy; you don't have to
	override any members.
	
	EmRefCounter is a class for managing RefCounted objects. It takes care
	of the tedious details of adding and removing refcounts, cloning the
	object if necessary (if the object is marked unshareable).

	See EmRegion's implementation for an example of this class's use. In
	general, it's pretty simple:
	
	{
		EmRefCounter	owner1(CreateRefCountedObject());	// One owner
		EmRefCounter	owner2 = owner1;					// Two owners
		{
			EmRefCounter	owner3 = owner1;				// Three owners
		}
		// Back down to two owners here
	}
	// No more owners, the internal object is destroyed.

	This class is based on the discussion by Scott Meyers in one of his
	"More Effective C++" books.
*/

class EmRefCounted
{
	public:
								EmRefCounted	(void);
								EmRefCounted	(const EmRefCounted& copy);
		virtual					~EmRefCounted	(void);

		EmRefCounted&			operator=		(const EmRefCounted& other);
			// Override of the assignment operator to prevent the refcount
			// from being transferred from the rhs object.

		void					addReference	(void);
		void					removeReference	(void);
			// Add or remove a reference to the object. Attempting to
			// add a reference to an unshareable object is an error; an
			// assertion will be thrown. Removing the last reference to
			// an object will cause that object to delete itself.

		void					markUnshareable	(void);
		Bool					isShareable		(void) const;
		Bool					isShared		(void) const;
			// Marking an object unshareable is a signal to the client/owner
			// of the RefCounted class that this object should not have
			// addReference() called on it. Instead, the object should be
			// cloned (EmRefCounter respects this convention). Call isShareable
			// to see if the object can be shared. Call IsShared to see if
			// more than one client is pointing to this object.

	private:
		long					fRefCount;
};

template <class T>
class EmRefCounter
{
	public:
								EmRefCounter	(T* p);
								EmRefCounter	(const EmRefCounter& other);
								~EmRefCounter	(void);

		EmRefCounter<T>&		operator=		(const EmRefCounter& other);
		EmRefCounter<T>&		operator=		(T* p);
			// Methods for assuming (shared) ownership of a refcounted
			// object. If the object does not descend from EmRefCounted,
			// you'll get a compiler error.
		
		int						operator==		(const EmRefCounter& other) const;
			// See if the internal objects are equal to each other.

		T*						operator->		(void) const;
		T&						operator*		(void) const;
		T*						get				(void) const;
			// Ways of getting to the internal object.

	private:
								EmRefCounter	(void);
		void					Init			(void);
		void					Replace			(T*);

		T*						fObject;
};

// ----------------------------------------------------------------------

inline
EmRefCounted::EmRefCounted (void) :
	fRefCount (0)
{
}

inline
EmRefCounted::EmRefCounted (const EmRefCounted&) :
	fRefCount (0)
{
}

inline
EmRefCounted::~EmRefCounted (void)
{
}

inline
EmRefCounted&
EmRefCounted::operator= (const EmRefCounted&)
{
	return *this;
}

inline
void
EmRefCounted::addReference (void)
{
	EmAssert(fRefCount >= 0);	// Can't add a reference to an unshareable item

	fRefCount++;
}

inline
void
EmRefCounted::removeReference (void)
{
	if (--fRefCount <= 0)
		delete this;
}

inline
void
EmRefCounted::markUnshareable (void)
{
	fRefCount = -1;
}

inline
Bool
EmRefCounted::isShareable (void) const
{
	return fRefCount >= 0;
}

inline
Bool
EmRefCounted::isShared (void) const
{
	return fRefCount > 0;
}

// ----------------------------------------------------------------------

template <class T>
EmRefCounter<T>::EmRefCounter (void) :
	fObject (NULL)
{
	this->Init ();
}

template <class T>
EmRefCounter<T>::EmRefCounter (T* p) :
	fObject (p)
{
	this->Init ();
}

template <class T>
EmRefCounter<T>::EmRefCounter (const EmRefCounter& other) :
	fObject (other.fObject)
{
	this->Init ();
}

template <class T>
EmRefCounter<T>::~EmRefCounter ()
{
	if (fObject)
		fObject->removeReference ();
}

template <class T>
EmRefCounter<T>&
EmRefCounter<T>::operator= (const EmRefCounter& other)
{
	this->Replace (other.fObject);
	return *this;
}

template <class T>
EmRefCounter<T>&
EmRefCounter<T>::operator= (T* p)
{
	this->Replace (p);
	return *this;
}

template <class T>
int
EmRefCounter<T>::operator== (const EmRefCounter& other) const
{
	if (this == &other)
		return true;
	
	if (this->fObject == other.fObject)
		return true;
	
	return *(this->fObject) == *(other.fObject);
}

template <class T>
T*
EmRefCounter<T>::operator-> (void) const
{
	return fObject;
}

template <class T>
T&
EmRefCounter<T>::operator* (void) const
{
	return *fObject;
}

template <class T>
T*
EmRefCounter<T>::get (void) const
{
	return fObject;
}

template <class T>
void
EmRefCounter<T>::Init ()
{
	if (fObject)
	{
		if (!fObject->isShareable ())
		{
			fObject = new T (*fObject);
		}
		
		fObject->addReference ();
	}
}

template <class T>
void
EmRefCounter<T>::Replace (T* p)
{
	if (fObject != p)
	{
		if (fObject)
			fObject->removeReference ();
		
		fObject = p;
		Init ();
	}
}

#endif	// EmRefCounted_h

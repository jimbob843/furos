/*
 * Furos v2.0   Copyright Marlet Limited 2012
 *
 * File:			object.c
 * Description:		Kernel library objects
 * Author:			James Smith
 * Created:			20-Sep-2012
 *
 */

#include "kernel.h"

static DWORD gNextHandle = 1;
static SPINLOCK gHandleSpinLock = 0;

static KERNELOBJECT *GlobalObjectList = NULL;


KRESULT obj_GetObjectFromHandle( HANDLE h, KERNELOBJECT **obj )
{
	// Search the object list for the correct handle
	KERNELOBJECT *current = GlobalObjectList;

	while (current != NULL)
	{
		if (current->ObjectHandle == h)
		{
			// Found it!
			break;
		}
		current = current->next;
	}

	*obj = current;
	if (current == NULL)
	{
		return KRESULT_BAD_HANDLE;
	}
	else
	{
		return KRESULT_SUCCESS;
	}
}



static void obj_AddObject( KERNELOBJECT *o ) {

	if (GlobalObjectList == NULL) {
		// This is the first object
		GlobalObjectList = o;
	} else {
		// Add to the start of the list
		o->next = GlobalObjectList;
		GlobalObjectList = o;
	}
}

static void obj_RemoveObject( KERNELOBJECT *o )
{
	KERNELOBJECT *current = GlobalObjectList;
	KERNELOBJECT *prev = NULL;
	BOOL done = FALSE;

	while ((current != NULL) && (done == FALSE))
	{
		if (current == o)
		{
			// Found it. Remove.
			if (prev == NULL)
			{
				// This is at the head of the list
				GlobalObjectList = current->next;
			}
			else
			{
				// Object is in the middle of the list
				prev->next = current->next;
			}
			done = TRUE;
		}
		else
		{
			prev = current;
			current = current->next;
		}
	}
}

DWORD obj_CreateHandle( HANDLE *handle )
{
	SPINLOCK_WAIT( &gHandleSpinLock );
	*handle = gNextHandle++;
	SPINLOCK_SIGNAL( &gHandleSpinLock );
	return KRESULT_SUCCESS;
}


DWORD obj_ReleaseHandle( HANDLE handle )
{
	KERNELOBJECT *obj = NULL;
	DWORD result = obj_GetObjectFromHandle( handle, &obj );
	if (result != KRESULT_SUCCESS)
	{
		return result;
	}

	obj_RemoveObject( obj );
	kfree( obj );

	return KRESULT_SUCCESS;
}



HANDLE obj_CreateEvent( void )
{
	EVENTOBJECT *obj = (EVENTOBJECT*)kmalloc(sizeof(EVENTOBJECT));
	obj->KObject.ObjectType = OBJECT_EVENT;
	obj->KObject.next = NULL;
	obj_CreateHandle( &(obj->KObject.ObjectHandle) );
	obj->Value = 0;

	obj_AddObject( (KERNELOBJECT*)obj );

	return obj->KObject.ObjectHandle;
}




// Simple generic vector library with macros for eased typed initialization & access.

#ifndef VECTOR_INCLUDED
#define VECTOR_INCLUDED

#include "core.h"

struct Vector
{
	ui64 Size;

	ui64 _Capacity;
	ui8* _Mem;
	ui16 _ItemSize;
};

// Changes the capacity of the vector and reallocates its memory.
void Vector_SetCapacity(struct Vector* Vec, ui64 Capacity)
{
	ASSERT(Vec != NULL);
	ASSERT(Vec->_ItemSize > 0);

	if (Vec->_Capacity == Capacity) return;

	if (Capacity == 0)
	{
		if (Vec->_Mem != NULL)
		{
			free(Vec->_Mem);
			Vec->_Mem = NULL;
		}
		Vec->_Capacity = 0;
		return;
	}

	// Allocate new memory block.
	ui8* NewMem = (ui8*)malloc(Capacity * Vec->_ItemSize);
	ASSERT(NewMem != NULL);

	// Copy previously allocated & used memory into new memory and free it, if any.
	if (Vec->_Mem != NULL && Vec->Size > 0)
	{
		memcpy(NewMem, Vec->_Mem, (Vec->Size > Capacity ? Capacity : Vec->Size) * Vec->_ItemSize);

		free(Vec->_Mem);
		Vec->_Mem = NULL;
		Vec->_Capacity = 0;
	}

	Vec->_Mem = NewMem;
	Vec->_Capacity = Capacity;
}

// Creates a new vector with the provided start capacity and item size. Both parameters must be greater than 0 !
struct Vector Vector_New(ui64 StartCapacity, ui16 ItemSize)
{
	ASSERT(ItemSize > 0);

	struct Vector NewVec = { 0 };
	NewVec._ItemSize = ItemSize;

	Vector_SetCapacity(&NewVec, StartCapacity);

	return NewVec;
}

// Clears passed vector completely, freeing its memory.
void Vector_Destroy(struct Vector* Vec)
{
	ASSERT(Vec != NULL);

	if (Vec->_Mem != NULL)
	{
		free(Vec->_Mem);
		Vec->_Mem = NULL;
	}

	Vec->_Capacity = 0;
	Vec->Size = 0;
}

// Returns pointer to element at provided index in the vector.
void* Vector_GetPtr(struct Vector* Vec, ui64 Index)
{
	ASSERT(Vec != NULL);
	ASSERT(Vec->Size > Index);

	return Vec->_Mem + Vec->_ItemSize * Index;
}

// Returns pointer to last element in the vector. The vector must be non-empty !
void* Vector_GetLastPtr(struct Vector* Vec)
{
	ASSERT(Vec != NULL);
	ASSERT(Vec->Size > 0);

	return Vec->_Mem + Vec->_ItemSize * (Vec->Size - 1);
}

// Adds a new element to the tip of the vector. Re-allocates vector if necessary.
void Vector_PushPtr(struct Vector* Vec, void* NewItemPtr)
{
	ASSERT(Vec != NULL);
	ASSERT(Vec->_ItemSize > 0);

	if (Vec->Size >= Vec->_Capacity)
	{
		// Re-allocate vector, doubling its capacity.
		Vector_SetCapacity(Vec, max(1, Vec->_Capacity * 2));
	}

	// Add element by copying the new item ptr, assuming its actual size is correct.
	memcpy(Vec->_Mem + Vec->Size * Vec->_ItemSize, NewItemPtr, Vec->_ItemSize);
	Vec->Size++;
}

// Removes the last element of the vector.
void Vector_Pop(struct Vector* Vec)
{
	ASSERT(Vec != NULL);
	ASSERT(Vec->_Mem != NULL);
	ASSERT(Vec->Size > 0);

	// Just decrement size and pretend the popped item doesn't exist anymore.
	Vec->Size--;
}

#define Vector_Create(Type, StartCapacity) (Vector_New(StartCapacity, sizeof(Type)))

#define Vector_GetValueAt(Vec, Type, Index) (*(Type*)Vector_GetPtr(&Vec, Index))
#define Vector_GetPtrAt(Vec, Type, Index) ((Type*)Vector_GetPtr(&Vec, Index))

#define Vector_Push(Vec, Type, Val) {	\
		Type ValWrap = Val;					\
		Vector_PushPtr(&Vec, &ValWrap);		\
	}

#define Vector_PopVal(Vec, Type, Dest) {			\
		*Dest = *(Type*)Vector_GetLastPtr(&Vec);	\
		Vector_Pop(&Vec);							\
	}
	
#endif // VECTOR_INCLUDED

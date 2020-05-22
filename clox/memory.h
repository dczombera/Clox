#ifndef clox_memory_h
#define clox_memory_h

#include "object.h"

#define ALLOCATE(type, count) \
	(type*)reallocate(NULL, 0, sizeof(type) * count)

#define GROW_CAPACITY(capacity) \
	((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(previous, type, oldCapacity, capacity) \
	(type*)reallocate(previous, sizeof(type)* (oldCapacity), sizeof(type)* (capacity))

#define FREE(type, pointer) \
	reallocate(pointer, sizeof(type), 0)

#define FREE_ARRAY(type, pointer, oldCapacity) \
	reallocate(pointer, sizeof(type)* (oldCapacity), 0)

void collectGarbage();
void freeObjects();
void markObject(Obj* object);
void markValue(Value value);
void* reallocate(void* previous, size_t oldSize, size_t newSize);

#endif // !clox_memory_h

#ifndef clox_memory_h
#define clox_memory_h

#define GROW_CAPACITY(capacity) \
	((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(previous, type, oldCapacity, capacity) \
	(type*)reallocate(previous, sizeof(type)* (oldCapacity), sizeof(type)* (capacity))

#define FREE_ARRAY(type, pointer, oldCapacity) \
	reallocate(pointer, sizeof(type)* (oldCapacity), 0)

void* reallocate(void* previous, size_t oldSize, size_t newSize);

#endif // !clox_memory_h

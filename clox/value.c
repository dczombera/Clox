#include <stdio.h>

#include "memory.h"
#include "value.h"

void freeValueArray(ValueArray* array) {
	FREE_ARRAY(Value, array->values, array->capacity);
	initValueArray(array);
}

void initValueArray(ValueArray* array) {
	array->capacity = 0;
	array->count = 0;
	array->values = NULL;
}

void printValue(Value value) {
	printf("%g", value);
}

void writeValueArray(ValueArray* array, Value value) {
	if (array->capacity < array->count + 1) {
		int oldCapacity = array->capacity;
		array->capacity = GROW_CAPACITY(oldCapacity);
		array->values = GROW_ARRAY(array->values, Value, oldCapacity, array->capacity);
	}

	array->values[array->count] = value;
	array->count++;
}

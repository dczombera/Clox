#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
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
#ifdef NAN_BOXING
	if (IS_BOOL(value)) {
		printf(AS_BOOL(value) ? "true" : "false");
	}
	else if (IS_NIL(value)) {
		printf("nil");
	}
	else if (IS_NUMBER(value)) {
		printf("%g", AS_NUMBER(value));
	}
	else if (IS_OBJ(value)) {
		printObject(value);
	}
#else

	switch (value.type) {
	case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
	case VAL_NIL: printf("nil"); break;
	case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
	case VAL_OBJ: printObject(value); break;
	}
#endif // NAN_BOXING
}

bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
	// To be fully IEEE 754 compliant, we check for numeric values and cast them to doubles.
	// This makes sure that NaN values are not equal to themselves.
	// But to be honest, who really cares if NaN is not equal to itself?
	if (IS_NUMBER(a) && IS_NUMBER(b)) return AS_NUMBER(a) == AS_NUMBER(b);
	return a == b;
#else

	if (a.type != b.type) return false;

	switch (a.type) {
	case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b); break;
	case VAL_NIL: return true; break;
	case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b); break;
	case VAL_OBJ:  return AS_OBJ(a) == AS_OBJ(b); break;
	}
#endif
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

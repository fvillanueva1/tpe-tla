#include "Value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char * _durationNames[] = {
	[DURATION_WHOLE] = "whole",
	[DURATION_HALF] = "half",
	[DURATION_QUARTER] = "quarter",
	[DURATION_EIGHTH] = "eighth",
	[DURATION_SIXTEENTH] = "sixteenth"
};

bool durationFromLexeme(const char * lexeme, Duration * duration) {
	for (size_t k = 0; k < sizeof(_durationNames) / sizeof(_durationNames[0]); ++k) {
		if (strcmp(lexeme, _durationNames[k]) == 0) {
			*duration = (Duration) k;
			return true;
		}
	}
	return false;
}

const char * durationName(Duration duration) {
	return _durationNames[duration];
}

bool copyValue(const Value * source, Value * target) {
	*target = *source;
	if (source->isVector) {
		target->vector.items = NULL;
		target->vector.count = 0;
		if (source->vector.count == 0) return true;
		Value * items = calloc(source->vector.count, sizeof(Value));
		if (items == NULL) return false;
		for (size_t k = 0; k < source->vector.count; ++k) {
			if (!copyValue(&source->vector.items[k], &items[k])) {
				for (size_t j = 0; j < k; ++j) destroyValue(&items[j]);
				free(items);
				return false;
			}
		}
		target->vector.items = items;
		target->vector.count = source->vector.count;
		return true;
	}
	if (source->type == TYPE_STRING) {
		target->string = strdup(source->string);
		return target->string != NULL;
	}
	return true;
}

void destroyValue(Value * value) {
	if (value->isVector) {
		for (size_t k = 0; k < value->vector.count; ++k) destroyValue(&value->vector.items[k]);
		free(value->vector.items);
		value->vector.items = NULL;
		value->vector.count = 0;
	}
	else if (value->type == TYPE_STRING) {
		free(value->string);
		value->string = NULL;
	}
}

bool appendText(TextBuffer * buffer, const char * text, size_t length) {
	if (buffer->length + length + 1 > buffer->capacity) {
		size_t capacity = buffer->capacity ? buffer->capacity : 32;
		while (buffer->length + length + 1 > capacity) capacity *= 2;
		char * grown = realloc(buffer->text, capacity);
		if (grown == NULL) return false;
		buffer->text = grown;
		buffer->capacity = capacity;
	}
	memcpy(buffer->text + buffer->length, text, length);
	buffer->length += length;
	buffer->text[buffer->length] = '\0';
	return true;
}

bool appendValue(TextBuffer * buffer, const Value * value) {
	if (value->isVector) {
		if (!appendText(buffer, "[", 1)) return false;
		for (size_t k = 0; k < value->vector.count; ++k) {
			if (k > 0 && !appendText(buffer, ", ", 2)) return false;
			if (!appendValue(buffer, &value->vector.items[k])) return false;
		}
		return appendText(buffer, "]", 1);
	}
	char number[16];
	const char * text;
	switch (value->type) {
		case TYPE_INTEGER:
			snprintf(number, sizeof(number), "%d", value->integer);
			text = number;
			break;
		case TYPE_BOOLEAN:
			text = value->boolean ? "true" : "false";
			break;
		case TYPE_DURATION:
			text = durationName(value->duration);
			break;
		case TYPE_STRING:
			text = value->string;
			break;
		default:
			text = "?";
			break;
	}
	return appendText(buffer, text, strlen(text));
}

bool bindValue(RuntimeScope * scope, const char * name, Value * value) {
	Binding * binding = malloc(sizeof(Binding));
	if (binding == NULL) return false;
	binding->name = name;
	binding->value = *value;
	binding->next = scope->bindings;
	scope->bindings = binding;
	return true;
}

Binding * findBinding(RuntimeScope * scope, const char * name) {
	for (; scope != NULL; scope = scope->parent) {
		for (Binding * binding = scope->bindings; binding != NULL; binding = binding->next) {
			if (strcmp(binding->name, name) == 0) return binding;
		}
	}
	return NULL;
}

void clearScope(RuntimeScope * scope) {
	while (scope->bindings != NULL) {
		Binding * binding = scope->bindings;
		scope->bindings = binding->next;
		destroyValue(&binding->value);
		free(binding);
	}
}

#ifndef VALUE_HEADER
#define VALUE_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * Runtime values. "type" is the DataType of the value or, for a vector, the
 * DataType of its elements. The musical types are added as the music library
 * (backend/music/Music.h) provides their representation.
 */

/** Note lengths, ordered from the longest to the shortest. */
typedef enum {
	DURATION_WHOLE,
	DURATION_HALF,
	DURATION_QUARTER,
	DURATION_EIGHTH,
	DURATION_SIXTEENTH
} Duration;

typedef struct Value Value;

struct Value {
	DataType type;
	bool isVector;
	union {
		int integer;
		bool boolean;
		Duration duration;
		/* Owned, NUL-terminated and already decoded (escapes and interpolations). */
		char * string;
		struct {
			Value * items;
			size_t count;
		} vector;
	};
};

/** Parses a duration lexeme (e.g., "eighth"); returns false when unknown. */
bool durationFromLexeme(const char * lexeme, Duration * duration);
const char * durationName(Duration duration);

/** Deep copy; returns false (and leaves "target" empty) when out of memory. */
bool copyValue(const Value * source, Value * target);
/** Releases what the value owns; the value can be destroyed more than once. */
void destroyValue(Value * value);

/**
 * Appends the textual form of the value, as "log" prints it, to a growing
 * buffer. Returns false when out of memory.
 */
typedef struct {
	char * text;
	size_t length;
	size_t capacity;
} TextBuffer;

bool appendText(TextBuffer * buffer, const char * text, size_t length);
bool appendValue(TextBuffer * buffer, const Value * value);

/**
 * A lexical scope of runtime variables. Lookups go from the innermost scope
 * to the global one, so an inner declaration hides an outer one.
 */
typedef struct Binding Binding;
typedef struct RuntimeScope RuntimeScope;

struct Binding {
	const char * name;
	Value value;
	Binding * next;
};

struct RuntimeScope {
	Binding * bindings;
	RuntimeScope * parent;
};

/** Takes ownership of "value" on success; returns false when out of memory. */
bool bindValue(RuntimeScope * scope, const char * name, Value * value);
Binding * findBinding(RuntimeScope * scope, const char * name);
void clearScope(RuntimeScope * scope);

#endif

#include "Runtime.h"
#include "Value.h"
#include "../output/Output.h"
#include "../semantic-analysis/Semantic.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

/**
 * A tree-walking interpreter over the AST. The semantic analysis already
 * guarantees names, types, arities and returns, so the runtime only checks
 * values: division by zero, overflow, indices and call depth. It also stops
 * a program that nests too deeply for the native stack, instead of crashing.
 */

typedef enum {
	FLOW_NORMAL,
	FLOW_RETURN,
	FLOW_ERROR
} Flow;

typedef struct Procedure Procedure;

struct Procedure {
	ProcedureDefinition * definition;
	Procedure * next;
};

typedef struct {
	OutputScore score;
	OutputFormat format;
	char * path;
} PendingExport;

typedef struct {
	RuntimeScope global;
	Procedure * procedures;
	CompilationStatus status;
	unsigned int depth;
	Value returned;
	bool hasReturned;
	const char * stackBase;
	size_t stackBudget;
	PendingExport * exports;
	size_t exportCount;
	size_t exportCapacity;
} Runtime;

/* ERRORS */

static bool _error(Runtime * runtime, const char * format, ...) {
	va_list arguments;
	va_start(arguments, format);
	fflush(stdout);
	fputs("Runtime error: ", stderr);
	vfprintf(stderr, format, arguments);
	fputs(".\n", stderr);
	va_end(arguments);
	runtime->status = FAILED;
	return false;
}

static bool _outOfMemory(Runtime * runtime) {
	_error(runtime, "out of memory");
	runtime->status = OUT_OF_MEMORY;
	return false;
}

static bool _musicStatus(Runtime * runtime, MusicStatus status, const char * operation) {
	if (status == MUSIC_OK) return true;
	if (status == MUSIC_NO_MEMORY) return _outOfMemory(runtime);
	switch (status) {
		case MUSIC_MISSING_OCTAVE: return _error(runtime, "note requires an octave");
		case MUSIC_OUT_OF_RANGE: return _error(runtime, "%s is out of range", operation);
		case MUSIC_TOO_MANY_NOTES: return _error(runtime, "%s has too many notes", operation);
		case MUSIC_NO_REALIZATION: return _error(runtime, "voice has no SATB realization within the tessituras");
		case MUSIC_NOT_FOUR_VOICES: return _error(runtime, "explicit voicing chords need four notes");
		case MUSIC_MISSING_KEY: return _error(runtime, "cannot modulate a progression without a known key");
		default: return _error(runtime, "invalid value for %s", operation);
	}
}

/* NATIVE STACK */

/**
 * The interpreter recurses once per nested call, block and expression, so a
 * program within the call-depth limit could still exhaust the native stack.
 * Half of the stack limit is available to the program; the rest is a margin
 * for the C library and the sanitizer.
 */
static void _measureStack(Runtime * runtime) {
	struct rlimit limit;
	size_t size = 8u * 1024u * 1024u;
	if (getrlimit(RLIMIT_STACK, &limit) == 0 && limit.rlim_cur != RLIM_INFINITY && limit.rlim_cur < size) {
		size = (size_t) limit.rlim_cur;
	}
	runtime->stackBase = __builtin_frame_address(0);
	runtime->stackBudget = size / 2;
}

static bool _checkStack(Runtime * runtime) {
	const char * here = __builtin_frame_address(0);
	size_t used = runtime->stackBase > here ? (size_t) (runtime->stackBase - here) : (size_t) (here - runtime->stackBase);
	if (used > runtime->stackBudget) return _error(runtime, "the program nests too deeply for the available stack");
	return true;
}

/* EXPRESSIONS */

static bool _evaluate(Runtime * runtime, RuntimeScope * scope, const Expression * expression, Value * result);
static bool _validateStrict(Runtime * runtime, RuntimeScope * scope, const Value * value);
static bool _call(Runtime * runtime, RuntimeScope * scope, const ProcedureCall * call, Value * result);
static Flow _executeStatements(Runtime * runtime, RuntimeScope * scope, const StatementList * list);

/** Appends a string fragment, decoding the escapes kept by the lexer. */
static bool _appendDecoded(TextBuffer * buffer, const char * text) {
	for (const char * p = text; *p != '\0'; ++p) {
		char character = *p;
		if (character == '\\' && p[1] != '\0') {
			++p;
			character = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p;
		}
		if (!appendText(buffer, &character, 1)) return false;
	}
	return true;
}

/** Builds the text of a string or "log" message; "text" is owned by the caller. */
static bool _interpolate(Runtime * runtime, RuntimeScope * scope, const StringPart * part, char ** text) {
	TextBuffer buffer = {0};
	if (!appendText(&buffer, "", 0)) return _outOfMemory(runtime);
	for (; part != NULL; part = part->next) {
		bool ok;
		if (part->type == STRING_PART_TEXT) {
			ok = _appendDecoded(&buffer, part->text);
		}
		else {
			Binding * binding = findBinding(scope, part->text);
			if (binding == NULL) {
				free(buffer.text);
				return _error(runtime, "variable '%s' has no value", part->text);
			}
			/* Interpolation reads the binding just like a variable expression. */
			if (!_validateStrict(runtime, scope, &binding->value)) {
				free(buffer.text);
				return false;
			}
			ok = appendValue(&buffer, &binding->value);
		}
		if (!ok) {
			free(buffer.text);
			return _outOfMemory(runtime);
		}
	}
	*text = buffer.text;
	return true;
}

static bool _evaluateInteger(Runtime * runtime, RuntimeScope * scope, const Expression * expression, int * result) {
	Value value;
	if (!_evaluate(runtime, scope, expression, &value)) return false;
	*result = value.integer;
	return true;
}

static bool _evaluateBoolean(Runtime * runtime, RuntimeScope * scope, const Expression * expression, bool * result) {
	Value value;
	if (!_evaluate(runtime, scope, expression, &value)) return false;
	*result = value.boolean;
	return true;
}

static bool _arithmetic(Runtime * runtime, ExpressionType type, int left, int right, int * result) {
	bool overflow;
	const char * operator;
	switch (type) {
		case ADDITION:
			overflow = __builtin_add_overflow(left, right, result);
			operator = "+";
			break;
		case SUBTRACTION:
			overflow = __builtin_sub_overflow(left, right, result);
			operator = "-";
			break;
		case MULTIPLICATION:
			overflow = __builtin_mul_overflow(left, right, result);
			operator = "*";
			break;
		default:
			if (right == 0) return _error(runtime, "division by zero");
			overflow = left == INT_MIN && right == -1;
			if (!overflow) *result = left / right;
			operator = "/";
			break;
	}
	if (overflow) return _error(runtime, "integer overflow in %d %s %d", left, operator, right);
	return true;
}

static bool _evaluateList(Runtime * runtime, RuntimeScope * scope, const ExpressionList * list, Value * result) {
	size_t count = 0;
	for (const ExpressionList * item = list; item != NULL; item = item->next) ++count;
	*result = (Value) {.type = TYPE_INTEGER, .isVector = true};
	if (count == 0) return true;
	Value * items = calloc(count, sizeof(Value));
	if (items == NULL) return _outOfMemory(runtime);
	size_t k = 0;
	for (const ExpressionList * item = list; item != NULL; item = item->next, ++k) {
		if (!_evaluate(runtime, scope, item->expression, &items[k])) {
			for (size_t j = 0; j < k; ++j) destroyValue(&items[j]);
			free(items);
			return false;
		}
	}
	result->type = items[0].type;
	result->vector.items = items;
	result->vector.count = count;
	return true;
}

static bool _reportVoicing(Runtime * runtime, const MusicVoicing * voicing, unsigned int rules) {
	size_t failed = 0;
	MusicValidationResult result = musicCheckVoicingValue(voicing, rules, &failed);
	if (result == MUSIC_VALID) return true;
	const char * reason = result == MUSIC_TESSITURA_ERROR ? "tessitura" :
		result == MUSIC_VOICE_CROSSING_ERROR ? "voice crossing" :
		result == MUSIC_PARALLEL_FIFTHS_ERROR ? "parallel fifths" :
		result == MUSIC_PARALLEL_OCTAVES_ERROR ? "parallel octaves" : "invalid voicing";
	return _error(runtime, "%s at chord %zu", reason, failed + 1);
}

/* A chord list acquires its declared type only at an assignment, argument or return. */
static bool _coerce(Runtime * runtime, Value * value, int type, bool vector) {
	if (vector) { value->type = type; return true; }
	if (!value->isVector) return type != TYPE_VOICING || _reportVoicing(runtime, &value->voicing, 0);
	if (type != TYPE_PROGRESSION && type != TYPE_VOICING) return true;
	size_t count = value->vector.count;
	MusicChord * chords = count ? calloc(count, sizeof(MusicChord)) : NULL;
	if (count && !chords) return _outOfMemory(runtime);
	for (size_t k = 0; k < count; ++k) chords[k] = *value->vector.items[k].chord;
	Value converted = {.type = type};
	MusicStatus status = type == TYPE_PROGRESSION ? musicChordProgression(chords, count, &converted.progression) :
		musicExplicitVoicing(chords, count, &converted.voicing);
	free(chords);
	if (!_musicStatus(runtime, status, type == TYPE_PROGRESSION ? "progression" : "voicing")) { destroyValue(&converted); return false; }
	if (type == TYPE_VOICING && !_reportVoicing(runtime, &converted.voicing, 0)) { destroyValue(&converted); return false; }
	destroyValue(value);
	*value = converted;
	return true;
}

static bool _evaluateAs(Runtime * runtime, RuntimeScope * scope, const Expression * expression, int type, bool vector, Value * result) {
	if (!_evaluate(runtime, scope, expression, result)) return false;
	if (_coerce(runtime, result, type, vector)) return true;
	destroyValue(result);
	return false;
}

/* Only chords and scales exceed a few pointers; their values own the payload. */
static bool _allocateMusic(Runtime * runtime, Value * value, int type) {
	value->type = type;
	if (type == TYPE_CHORD) {
		value->chord = calloc(1, sizeof(MusicChord));
		return value->chord != NULL || _outOfMemory(runtime);
	}
	value->scale = calloc(1, sizeof(MusicScale));
	return value->scale != NULL || _outOfMemory(runtime);
}

/* Read only the selected element; the caller owns the independent result. */
static bool _readIndex(Runtime * runtime, const Value * vector, int index, Value * result) {
	if (!vector->isVector && vector->type == TYPE_SCALE) {
		result->type = TYPE_NOTE;
		MusicStatus status = musicScaleNote(vector->scale, index, &result->note);
		if (status != MUSIC_OK) return _error(runtime, "index %d is out of range for a scale of length 7", index);
		return true;
	}
	if (!vector->isVector && vector->type == TYPE_PROGRESSION) {
		size_t count = vector->progression.count;
		if (index < 0 || (size_t) index >= count)
			return _error(runtime, "index %d is out of range for a progression of length %zu", index, count);
		if (!_allocateMusic(runtime, result, TYPE_CHORD)) return false;
		*result->chord = vector->progression.steps[index].chord;
		return true;
	}
	if (index < 0 || (size_t) index >= vector->vector.count)
		return _error(runtime, "index %d is out of range for a vector of length %zu", index, vector->vector.count);
	return copyValue(&vector->vector.items[index], result) || _outOfMemory(runtime);
}

/* Do not reserve an indexed container in unrelated recursive expressions. */
static __attribute__((noinline)) bool _evaluateIndex(Runtime * runtime, RuntimeScope * scope, const Expression * expression, Value * result) {
	int index;
	if (expression->leftExpression->type == VARIABLE) {
		/* An index call may replace globals: evaluate it before borrowing the binding. */
		if (!_evaluateInteger(runtime, scope, expression->rightExpression, &index)) return false;
		Binding * binding = findBinding(scope, expression->leftExpression->text);
		if (!binding) return _error(runtime, "variable '%s' has no value", expression->leftExpression->text);
		return _readIndex(runtime, &binding->value, index, result);
	}
	Value vector;
	if (!_evaluate(runtime, scope, expression->leftExpression, &vector)) return false;
	bool ok = _evaluateInteger(runtime, scope, expression->rightExpression, &index)
		&& _readIndex(runtime, &vector, index, result);
	destroyValue(&vector);
	return ok;
}

/* Both operands are evaluated once, from left to right, and always released. */
static bool _evaluateOperation(Runtime * runtime, RuntimeScope * scope, const Expression * expression, Value * result) {
	Value left, right;
	if (!_evaluate(runtime, scope, expression->leftExpression, &left)) return false;
	if (!_evaluate(runtime, scope, expression->rightExpression, &right)) { destroyValue(&left); return false; }
	MusicStatus status = MUSIC_OK;
	const char * operation = "musical operation";
	bool ok = true;
	ExpressionType type = expression->type;
	if (type == IN_EXPRESSION) {
		if (right.type == TYPE_KEY) {
			size_t count = left.vector.count;
			int * degrees = count ? calloc(count, sizeof(int)) : NULL;
			if (count && !degrees) ok = _outOfMemory(runtime);
			else {
				for (size_t k = 0; k < count; ++k) degrees[k] = left.vector.items[k].integer - 1;
				result->type = TYPE_PROGRESSION;
				status = musicDegreeProgression(right.key, degrees, count, &result->progression);
				operation = "progression degree";
			}
			free(degrees);
		} else {
			result->type = TYPE_BOOLEAN;
			result->boolean = right.type == TYPE_SCALE ? musicNoteInScale(left.note, right.scale) : musicNoteInChord(left.note, right.chord);
		}
	} else if (type == INVERSION) {
		ok = _allocateMusic(runtime, result, TYPE_CHORD);
		if (ok) status = musicInvertChord(left.chord, right.integer, result->chord);
		operation = "inversion";
	} else if (type == ARPEGGIATION) {
		result->type = TYPE_PROGRESSION;
		status = musicArpeggiate(left.chord, (MusicDuration) right.duration, &result->progression);
		operation = "arpeggiation";
	} else if (type == MODULATION) {
		ok = _coerce(runtime, &left, TYPE_PROGRESSION, false);
		if (ok) {
			result->type = TYPE_PROGRESSION;
			status = musicModulate(&left.progression, right.key, &result->progression);
			operation = "modulation";
		}
	} else if (!left.isVector && left.type == TYPE_INTEGER) {
		ok = _arithmetic(runtime, type, left.integer, right.integer, &result->integer);
	} else {
		if (left.isVector) ok = _coerce(runtime, &left, TYPE_PROGRESSION, false);
		if (ok && right.isVector) ok = _coerce(runtime, &right, TYPE_PROGRESSION, false);
		if (ok && right.type == TYPE_INTERVAL) {
			bool down = type == SUBTRACTION;
			result->type = left.type;
			operation = "transposition";
			if (left.type == TYPE_NOTE) status = musicTransposeNote(left.note, right.interval, down, &result->note);
			else if (left.type == TYPE_CHORD) {
				ok = _allocateMusic(runtime, result, TYPE_CHORD);
				if (ok) status = musicTransposeChord(left.chord, right.interval, down, result->chord);
			}
			else status = musicTransposeProgression(&left.progression, right.interval, down, &result->progression);
		} else if (ok) {
			result->type = TYPE_PROGRESSION;
			if (type == MULTIPLICATION) {
				if (right.integer < 0) ok = _error(runtime, "repetition count cannot be negative (%d)", right.integer);
				else status = musicRepeat(&left.progression, right.integer, &result->progression);
				operation = "repetition";
			} else if (right.type == TYPE_CHORD) status = musicAppendChord(&left.progression, right.chord, &result->progression);
			else status = musicConcatenate(&left.progression, &right.progression, &result->progression);
		}
	}
	destroyValue(&left);
	destroyValue(&right);
	return ok && _musicStatus(runtime, status, operation);
}

/* Musical literals and operations with no context-dependent list conversion. */
static bool _evaluateMusic(Runtime * runtime, RuntimeScope * scope, const Expression * expression, Value * result) {
	MusicStatus status = MUSIC_OK;
	const char * operation = "musical expression";
	switch (expression->type) {
		case NOTE_LITERAL:
			result->type = TYPE_NOTE;
			status = musicParseNote(expression->text, &result->note);
			operation = "note";
			break;
		case INTERVAL_LITERAL:
			result->type = TYPE_INTERVAL;
			status = musicParseInterval(expression->text, &result->interval);
			operation = "interval";
			break;
		case DEGREE_LITERAL:
			status = musicParseDegree(expression->text, &result->integer);
			++result->integer; /* Integer degrees in the language are one-based. */
			operation = "degree";
			break;
		case DIMINISHED_LITERAL:
			result->type = VALUE_QUALITY;
			result->quality = MUSIC_QUALITY_DIMINISHED;
			break;
		case KEY_LITERAL:
			result->type = TYPE_KEY;
			result->key.mode = (MusicMode) expression->key.mode;
			result->key.strict = expression->key.strict;
			status = musicParseSpelling(expression->key.tonic, &result->key.tonic);
			operation = "key";
			break;
		case SCALE_OF_KEY:
		case DOMINANT_OF_KEY:
		case SUBDOMINANT_OF_KEY:
		case RELATIVE_MINOR_OF_KEY: {
			Value key;
			if (!_evaluate(runtime, scope, expression->operand, &key)) return false;
			result->type = expression->type == SCALE_OF_KEY ? TYPE_SCALE : TYPE_KEY;
			if (expression->type == SCALE_OF_KEY) {
				if (!_allocateMusic(runtime, result, TYPE_SCALE)) { destroyValue(&key); return false; }
				status = musicKeyScale(key.key, result->scale);
			}
			else if (expression->type == DOMINANT_OF_KEY) status = musicDominantKey(key.key, &result->key);
			else if (expression->type == SUBDOMINANT_OF_KEY) status = musicSubdominantKey(key.key, &result->key);
			else status = musicRelativeMinorKey(key.key, &result->key);
			destroyValue(&key);
			operation = "key relation";
			break;
		}
		case NOTES_CHORD: {
			MusicNote notes[MUSIC_MAX_CHORD_NOTES];
			size_t count = 0;
			for (const ExpressionList * item = expression->elements; item; item = item->next) {
				if (count == MUSIC_MAX_CHORD_NOTES) return _error(runtime, "chord has too many notes (maximum %d)", MUSIC_MAX_CHORD_NOTES);
				Value note;
				if (!_evaluate(runtime, scope, item->expression, &note)) return false;
				notes[count++] = note.note;
				destroyValue(&note);
			}
			if (!_allocateMusic(runtime, result, TYPE_CHORD)) return false;
			status = musicChordFromNotes(notes, count, result->chord);
			operation = "chord";
			break;
		}
		case TRIAD_CHORD:
		case SEVENTH_CHORD:
		case NINTH_CHORD: {
			int degree;
			Value key;
			if (!_evaluateInteger(runtime, scope, expression->leftExpression, &degree)) return false;
			if (!_evaluate(runtime, scope, expression->rightExpression, &key)) return false;
			if (degree < 1 || degree > 7) {
				destroyValue(&key);
				return _error(runtime, "chord degree %d must be between 1 and 7", degree);
			}
			MusicChordSize size = expression->type == TRIAD_CHORD ? MUSIC_TRIAD : expression->type == SEVENTH_CHORD ? MUSIC_SEVENTH : MUSIC_NINTH;
			if (!_allocateMusic(runtime, result, TYPE_CHORD)) { destroyValue(&key); return false; }
			status = musicDegreeChord(key.key, degree - 1, size, result->chord);
			destroyValue(&key);
			operation = "degree chord";
			break;
		}
		case PROPERTY_ACCESS: {
			Value owned = {0};
			const Value * object;
			if (expression->property.object->type == VARIABLE) {
				Binding * binding = findBinding(scope, expression->property.object->text);
				if (!binding) return _error(runtime, "variable '%s' has no value", expression->property.object->text);
				object = &binding->value;
				if (!_validateStrict(runtime, scope, object)) return false;
			} else {
				if (!_evaluate(runtime, scope, expression->property.object, &owned)) return false;
				object = &owned;
			}
			const char * name = expression->property.name;
			operation = name;
			if (!strcmp(name, "octave")) result->integer = musicNoteOctave(object->note);
			else if (!strcmp(name, "root")) {
				result->type = TYPE_NOTE;
				status = musicChordRoot(object->chord, &result->note);
			} else if (!strcmp(name, "quality")) {
				result->type = VALUE_QUALITY;
				result->quality = musicChordQuality(object->chord);
			} else if (!strcmp(name, "tonic")) {
				result->type = TYPE_NOTE;
				status = musicKeyTonic(object->key, &result->note);
			} else if (!strcmp(name, "mode")) {
				result->type = VALUE_MODE;
				result->mode = musicKeyMode(object->key);
			} else if (!strcmp(name, "length")) {
				if (object->progression.count > INT_MAX) {
					destroyValue(&owned);
					return _error(runtime, "progression length exceeds the integer range");
				}
				result->integer = (int) object->progression.count;
			}
			destroyValue(&owned);
			break;
		}
		case VOICE_EXPRESSION: {
			Value progression;
			if (!_evaluateAs(runtime, scope, expression->voice->progression, TYPE_PROGRESSION, false, &progression)) return false;
			result->type = TYPE_VOICING;
			status = musicVoiceProgression(&progression.progression, &result->voicing);
			destroyValue(&progression);
			operation = "voice";
			break;
		}
		case IN_EXPRESSION:
		case INVERSION:
		case ARPEGGIATION:
		case MODULATION:
			return _evaluateOperation(runtime, scope, expression, result);
		default: return _error(runtime, "expression is not supported yet");
	}
	return _musicStatus(runtime, status, operation);
}

/* Comparisons use pitch, semitones, quality or mode according to the type. */
static bool _comparable(Runtime * runtime, RuntimeScope * scope, const Expression * expression, int * number) {
	Value value;
	if (!_evaluate(runtime, scope, expression, &value)) return false;
	switch (value.type) {
		case TYPE_NOTE: *number = value.note.pitch; break;
		case TYPE_INTERVAL: *number = value.interval.semitones; break;
		case VALUE_QUALITY: *number = value.quality; break;
		case VALUE_MODE: *number = value.mode; break;
		default: *number = value.integer; break;
	}
	destroyValue(&value);
	return true;
}

static bool _evaluateUnchecked(Runtime * runtime, RuntimeScope * scope, const Expression * expression, Value * result) {
	*result = (Value) {.type = TYPE_INTEGER};
	if (!_checkStack(runtime)) return false;
	switch (expression->type) {
		case INTEGER_LITERAL:
			result->integer = expression->integer;
			return true;
		case BOOLEAN_LITERAL:
			result->type = TYPE_BOOLEAN;
			result->boolean = expression->boolean;
			return true;
		case DURATION_LITERAL:
			result->type = TYPE_DURATION;
			if (!durationFromLexeme(expression->text, &result->duration)) return _error(runtime, "unknown duration '%s'", expression->text);
			return true;
		case STRING_LITERAL:
			result->type = TYPE_STRING;
			return _interpolate(runtime, scope, expression->stringParts, &result->string);
		case VARIABLE: {
			Binding * binding = findBinding(scope, expression->text);
			if (binding == NULL) return _error(runtime, "variable '%s' has no value", expression->text);
			if (!copyValue(&binding->value, result)) return _outOfMemory(runtime);
			return true;
		}
		case LIST_LITERAL:
			return _evaluateList(runtime, scope, expression->elements, result);
		case INDEX_ACCESS:
			return _evaluateIndex(runtime, scope, expression, result);
		case PROCEDURE_CALL:
			return _call(runtime, scope, expression->call, result);
		case NEGATION:
			result->type = TYPE_BOOLEAN;
			if (!_evaluateBoolean(runtime, scope, expression->operand, &result->boolean)) return false;
			result->boolean = !result->boolean;
			return true;
		case CONJUNCTION:
		case DISJUNCTION: {
			/* Short-circuit: the right operand runs only when it decides the result. */
			bool value;
			result->type = TYPE_BOOLEAN;
			if (!_evaluateBoolean(runtime, scope, expression->leftExpression, &value)) return false;
			if (value == (expression->type == DISJUNCTION)) {
				result->boolean = value;
				return true;
			}
			return _evaluateBoolean(runtime, scope, expression->rightExpression, &result->boolean);
		}
		case EQUALITY:
		case INEQUALITY:
		case LESS_THAN:
		case LESS_THAN_OR_EQUAL:
		case GREATER_THAN:
		case GREATER_THAN_OR_EQUAL: {
			int left, right;
			if (!_comparable(runtime, scope, expression->leftExpression, &left)
				|| !_comparable(runtime, scope, expression->rightExpression, &right)) return false;
			result->type = TYPE_BOOLEAN;
			switch (expression->type) {
				case EQUALITY: result->boolean = left == right; break;
				case INEQUALITY: result->boolean = left != right; break;
				case LESS_THAN: result->boolean = left < right; break;
				case LESS_THAN_OR_EQUAL: result->boolean = left <= right; break;
				case GREATER_THAN: result->boolean = left > right; break;
				default: result->boolean = left >= right; break;
			}
			return true;
		}
		case ADDITION:
		case SUBTRACTION:
		case MULTIPLICATION:
		case DIVISION:
			return _evaluateOperation(runtime, scope, expression, result);
		default:
			return _evaluateMusic(runtime, scope, expression, result);
	}
}

static bool _inStrictKey(const Value * value, MusicKey key) {
	if (value->isVector) {
		for (size_t k = 0; k < value->vector.count; ++k)
			if (!_inStrictKey(&value->vector.items[k], key)) return false;
	} else if (value->type == TYPE_CHORD) {
		return musicChordInKey(value->chord, key);
	} else if (value->type == TYPE_PROGRESSION) {
		for (size_t k = 0; k < value->progression.count; ++k)
			if (!musicChordInKey(&value->progression.steps[k].chord, key)) return false;
	} else if (value->type == TYPE_VOICING) {
		for (size_t k = 0; k < value->voicing.count; ++k) {
			for (size_t v = 0; v < VOICE_COUNT; ++v) {
				MusicNote note;
				if (musicNoteFromPitch(value->voicing.chords[k][v], &note) != MUSIC_OK || !musicNoteInKey(note, key)) return false;
			}
		}
	}
	return true;
}

static bool _validateStrict(Runtime * runtime, RuntimeScope * scope, const Value * value) {
	for (; scope; scope = scope->parent) {
		if (!scope->keyContext) continue;
		MusicKey key = scope->keyContext->value.key;
		if (key.strict && !_inStrictKey(value, key)) return _error(runtime, "chord contains a note outside the strict key");
		break; /* A non-strict key also replaces an outer context. */
	}
	return true;
}

static bool _evaluate(Runtime * runtime, RuntimeScope * scope, const Expression * expression, Value * result) {
	bool ok = _evaluateUnchecked(runtime, scope, expression, result);
	if (ok) ok = _validateStrict(runtime, scope, result);
	if (!ok) destroyValue(result);
	return ok;
}

/* PROCEDURES */

static ProcedureDefinition * _findProcedure(Runtime * runtime, const char * name) {
	for (Procedure * procedure = runtime->procedures; procedure != NULL; procedure = procedure->next) {
		if (strcmp(procedure->definition->name, name) == 0) return procedure->definition;
	}
	return NULL;
}

/**
 * Arguments are evaluated in the caller's scope and copied (also vectors).
 * The body runs in a new scope whose parent is the global one: the semantic
 * analysis guarantees it only reads globals declared before the definition.
 * "result" is NULL when the call is a statement.
 */
static bool _call(Runtime * runtime, RuntimeScope * scope, const ProcedureCall * call, Value * result) {
	ProcedureDefinition * procedure = _findProcedure(runtime, call->name);
	if (procedure == NULL) return _error(runtime, "procedure '%s' is not defined", call->name);
	if (runtime->depth >= RUNTIME_MAX_CALL_DEPTH) {
		return _error(runtime, "procedure '%s' exceeds the maximum call depth of %d", call->name, RUNTIME_MAX_CALL_DEPTH);
	}
	RuntimeScope local = {.parent = &runtime->global};
	const ParameterList * parameter = procedure->parameters;
	const ExpressionList * argument = call->arguments;
	bool ok = true;
	for (; ok && parameter != NULL && argument != NULL; parameter = parameter->next, argument = argument->next) {
		Value value;
		ok = _evaluateAs(runtime, scope, argument->expression, parameter->parameter->dataType, parameter->parameter->isVector, &value);
		if (!ok) break;
		if (!bindValue(&local, parameter->parameter->name, &value)) {
			destroyValue(&value);
			ok = _outOfMemory(runtime);
		}
	}
	if (ok) {
		++runtime->depth;
		Flow flow = _executeStatements(runtime, &local, procedure->body);
		--runtime->depth;
		ok = flow != FLOW_ERROR;
	}
	if (ok && runtime->hasReturned) {
		runtime->hasReturned = false;
		Value returned = runtime->returned;
		runtime->returned = (Value) {.type = TYPE_INTEGER};
		ok = _coerce(runtime, &returned, procedure->returnType, procedure->returnsVector);
		if (ok && result != NULL) *result = returned;
		else destroyValue(&returned);
	}
	clearScope(&local);
	return ok;
}

/* STATEMENTS */

static Flow _executeBlock(Runtime * runtime, RuntimeScope * parent, const StatementList * list) {
	RuntimeScope local = {.parent = parent};
	Flow flow = _executeStatements(runtime, &local, list);
	clearScope(&local);
	return flow;
}

static Flow _bind(Runtime * runtime, RuntimeScope * scope, const char * name, Value * value) {
	if (bindValue(scope, name, value)) return FLOW_NORMAL;
	destroyValue(value);
	_outOfMemory(runtime);
	return FLOW_ERROR;
}

/* Only for loops need a counter value and an iteration scope on the stack. */
static __attribute__((noinline)) Flow _executeFor(Runtime * runtime, RuntimeScope * scope, const For * loop) {
	int from, to;
	if (!_evaluateInteger(runtime, scope, loop->from, &from) || !_evaluateInteger(runtime, scope, loop->to, &to)) return FLOW_ERROR;
	/* Both bounds are inclusive and evaluated once; the counter is not the variable. */
	for (long long counter = from; counter <= to; ++counter) {
		RuntimeScope local = {.parent = scope};
		Value value = {.type = TYPE_INTEGER, .integer = (int) counter};
		Flow flow = _bind(runtime, &local, loop->variable, &value);
		if (flow == FLOW_NORMAL) flow = _executeStatements(runtime, &local, loop->body);
		clearScope(&local);
		if (flow != FLOW_NORMAL) return flow;
	}
	return FLOW_NORMAL;
}

/* Exports capture a score now; no writer runs until execution succeeds. */
static bool _outputStatus(Runtime * runtime, OutputStatus status, const char * path) {
	if (status == OUTPUT_OK) return true;
	if (status == OUTPUT_NO_MEMORY) return _outOfMemory(runtime);
	return _error(runtime, "cannot write '%s'%s", path, status == OUTPUT_FILE_EXISTS ? " (destination already exists or is repeated)" : "");
}

/* Keep the score and path temporaries out of every recursive statement frame. */
static __attribute__((noinline)) bool _queueExport(Runtime * runtime, RuntimeScope * scope, const Export * output) {
	Value value;
	if (!_evaluate(runtime, scope, output->value, &value)) return false;
	if (value.isVector && !_coerce(runtime, &value, TYPE_PROGRESSION, false)) { destroyValue(&value); return false; }
	int tempo = OUTPUT_DEFAULT_TEMPO;
	if (output->tempo && !_evaluateInteger(runtime, scope, output->tempo, &tempo)) { destroyValue(&value); return false; }
	if (tempo < (output->format == EXPORT_MIDI ? 4 : 1) || (output->format == EXPORT_MIDI && tempo > 60000000)) {
		destroyValue(&value);
		return _error(runtime, "tempo %d is outside the export format's range", tempo);
	}
	int instrument = output->instrument ? semanticInstrumentProgram(output->instrument) : OUTPUT_DEFAULT_INSTRUMENT;
	TextBuffer path = {0};
	if (!appendText(&path, "", 0) || !_appendDecoded(&path, output->path)) {
		free(path.text);
		destroyValue(&value);
		return _outOfMemory(runtime);
	}
	PendingExport pending = {.path = path.text, .format = output->format == EXPORT_MIDI ? OUTPUT_FORMAT_MIDI : OUTPUT_FORMAT_LILYPOND};
	OutputStatus status = outputScoreInit(&pending.score, tempo, instrument);
	if (status == OUTPUT_OK) {
		switch (value.type) {
			case TYPE_NOTE: status = outputAddNote(&pending.score, value.note); break;
			case TYPE_SCALE: status = outputAddScale(&pending.score, value.scale); break;
			case TYPE_CHORD: status = outputAddChord(&pending.score, value.chord); break;
			case TYPE_PROGRESSION: status = outputAddProgression(&pending.score, &value.progression); break;
			case TYPE_VOICING: status = outputAddVoicing(&pending.score, &value.voicing); break;
			default: status = OUTPUT_INVALID_ARGUMENT; break;
		}
	}
	destroyValue(&value);
	bool ok = _outputStatus(runtime, status, pending.path);
	if (ok && runtime->exportCount == runtime->exportCapacity) {
		size_t capacity = runtime->exportCapacity ? runtime->exportCapacity * 2 : 8;
		if (capacity < runtime->exportCapacity || capacity > SIZE_MAX / sizeof(PendingExport)) ok = _outOfMemory(runtime);
		else {
			PendingExport * grown = realloc(runtime->exports, capacity * sizeof(PendingExport));
			if (!grown) ok = _outOfMemory(runtime);
			else { runtime->exports = grown; runtime->exportCapacity = capacity; }
		}
	}
	if (ok) runtime->exports[runtime->exportCount++] = pending;
	else { outputScoreDestroy(&pending.score); free(pending.path); }
	return ok;
}

static bool _writeExports(Runtime * runtime) {
	if (!runtime->exportCount) return true;
	OutputFile * files = calloc(runtime->exportCount, sizeof(OutputFile));
	if (!files) return _outOfMemory(runtime);
	for (size_t k = 0; k < runtime->exportCount; ++k) {
		PendingExport * pending = &runtime->exports[k];
		files[k] = (OutputFile) {&pending->score, pending->format, pending->path};
	}
	size_t failed = 0;
	OutputStatus status = outputWriteFiles(files, runtime->exportCount, &failed);
	free(files);
	return _outputStatus(runtime, status, runtime->exports[failed].path);
}

static Flow _execute(Runtime * runtime, RuntimeScope * scope, const Statement * statement) {
	if (!_checkStack(runtime)) return FLOW_ERROR;
	switch (statement->type) {
		case DECLARATION: {
			Value value;
			if (!_evaluateAs(runtime, scope, statement->expression, statement->dataType, statement->isVector, &value)) return FLOW_ERROR;
			return _bind(runtime, scope, statement->name, &value);
		}
		case ASSIGNMENT: {
			Value value;
			if (!_evaluate(runtime, scope, statement->expression, &value)) return FLOW_ERROR;
			Binding * binding = findBinding(scope, statement->name);
			if (binding == NULL) {
				destroyValue(&value);
				_error(runtime, "variable '%s' is not declared", statement->name);
				return FLOW_ERROR;
			}
			if (!_coerce(runtime, &value, binding->value.type, binding->value.isVector)) {
				destroyValue(&value);
				return FLOW_ERROR;
			}
			destroyValue(&binding->value);
			binding->value = value;
			return FLOW_NORMAL;
		}
		case IF_STATEMENT: {
			bool condition;
			if (!_evaluateBoolean(runtime, scope, statement->ifStatement->condition, &condition)) return FLOW_ERROR;
			return _executeBlock(runtime, scope, condition ? statement->ifStatement->thenBlock : statement->ifStatement->elseBlock);
		}
		case WHILE_STATEMENT:
			while (true) {
				bool condition;
				if (!_evaluateBoolean(runtime, scope, statement->whileStatement->condition, &condition)) return FLOW_ERROR;
				if (!condition) return FLOW_NORMAL;
				Flow flow = _executeBlock(runtime, scope, statement->whileStatement->body);
				if (flow != FLOW_NORMAL) return flow;
			}
		case FOR_STATEMENT:
			return _executeFor(runtime, scope, statement->forStatement);
		case PROCEDURE_DEFINITION: {
			/* Registered when reached: a procedure is only callable after its definition. */
			Procedure * procedure = malloc(sizeof(Procedure));
			if (procedure == NULL) {
				_outOfMemory(runtime);
				return FLOW_ERROR;
			}
			procedure->definition = statement->procedureDefinition;
			procedure->next = runtime->procedures;
			runtime->procedures = procedure;
			return FLOW_NORMAL;
		}
		case PROCEDURE_CALL_STATEMENT:
			return _call(runtime, scope, statement->procedureCall, NULL) ? FLOW_NORMAL : FLOW_ERROR;
		case RETURN_STATEMENT:
			if (statement->returnStatement->value != NULL) {
				/* Calls inside the expression use runtime->returned, so it is filled at the end. */
				Value value;
				if (!_evaluate(runtime, scope, statement->returnStatement->value, &value)) return FLOW_ERROR;
				runtime->returned = value;
				runtime->hasReturned = true;
			}
			return FLOW_RETURN;
		case EXPORT_STATEMENT:
			return _queueExport(runtime, scope, statement->export) ? FLOW_NORMAL : FLOW_ERROR;
		case CHECK_STATEMENT: {
			Value voicing;
			if (!_evaluateAs(runtime, scope, statement->check->voicing, TYPE_VOICING, false, &voicing)) return FLOW_ERROR;
			bool ok = _reportVoicing(runtime, &voicing.voicing, statement->check->rules);
			destroyValue(&voicing);
			return ok ? FLOW_NORMAL : FLOW_ERROR;
		}
		case LOG_STATEMENT: {
			char * text;
			if (!_interpolate(runtime, scope, statement->log->message, &text)) return FLOW_ERROR;
			bool written = fputs(text, stdout) != EOF && fputc('\n', stdout) != EOF && fflush(stdout) == 0;
			free(text);
			if (!written) {
				_error(runtime, "cannot write the log output");
				return FLOW_ERROR;
			}
			return FLOW_NORMAL;
		}
		default:
			_error(runtime, "statement is not supported yet");
			return FLOW_ERROR;
	}
}

static Flow _executeStatements(Runtime * runtime, RuntimeScope * scope, const StatementList * list) {
	for (; list != NULL; list = list->next) {
		Flow flow = _execute(runtime, scope, list->statement);
		if (flow != FLOW_NORMAL) return flow;
	}
	return FLOW_NORMAL;
}

CompilationStatus executeProgram(Program * program) {
	Runtime runtime = {.status = SUCCEEDED};
	_measureStack(&runtime);
	Flow flow = _executeStatements(&runtime, &runtime.global, program->statements);
	if (flow != FLOW_ERROR && !_writeExports(&runtime)) flow = FLOW_ERROR;
	for (size_t k = 0; k < runtime.exportCount; ++k) {
		outputScoreDestroy(&runtime.exports[k].score);
		free(runtime.exports[k].path);
	}
	free(runtime.exports);
	if (runtime.hasReturned) destroyValue(&runtime.returned);
	clearScope(&runtime.global);
	while (runtime.procedures != NULL) {
		Procedure * procedure = runtime.procedures;
		runtime.procedures = procedure->next;
		free(procedure);
	}
	fflush(stdout);
	return flow == FLOW_ERROR ? runtime.status : SUCCEEDED;
}

#include "Runtime.h"
#include "Value.h"
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
	RuntimeScope global;
	Procedure * procedures;
	CompilationStatus status;
	unsigned int depth;
	Value returned;
	bool hasReturned;
	const char * stackBase;
	size_t stackBudget;
} Runtime;

/* SUPPORTED CONSTRUCTS */

static bool _supportedType(DataType type) {
	return type == TYPE_INTEGER || type == TYPE_BOOLEAN || type == TYPE_STRING || type == TYPE_DURATION;
}

static bool _supportsExpression(const Expression * expression);
static bool _supportsStatements(const StatementList * list);

static bool _supportsExpressions(const ExpressionList * list) {
	for (; list != NULL; list = list->next) {
		if (!_supportsExpression(list->expression)) return false;
	}
	return true;
}

static bool _supportsExpression(const Expression * expression) {
	if (expression == NULL) return true;
	switch (expression->type) {
		case BOOLEAN_LITERAL:
		case DURATION_LITERAL:
		case INTEGER_LITERAL:
		case STRING_LITERAL:
		case VARIABLE:
			return true;
		case LIST_LITERAL:
			return _supportsExpressions(expression->elements);
		case PROCEDURE_CALL:
			return _supportsExpressions(expression->call->arguments);
		case NEGATION:
			return _supportsExpression(expression->operand);
		case ADDITION:
		case CONJUNCTION:
		case DISJUNCTION:
		case DIVISION:
		case EQUALITY:
		case GREATER_THAN:
		case GREATER_THAN_OR_EQUAL:
		case INDEX_ACCESS:
		case INEQUALITY:
		case LESS_THAN:
		case LESS_THAN_OR_EQUAL:
		case MULTIPLICATION:
		case SUBTRACTION:
			return _supportsExpression(expression->leftExpression) && _supportsExpression(expression->rightExpression);
		default:
			return false;
	}
}

static bool _supportsStatement(const Statement * statement) {
	switch (statement->type) {
		case DECLARATION:
			return _supportedType(statement->dataType) && _supportsExpression(statement->expression);
		case ASSIGNMENT:
			return _supportsExpression(statement->expression);
		case LOG_STATEMENT:
			return true;
		case IF_STATEMENT:
			return _supportsExpression(statement->ifStatement->condition)
				&& _supportsStatements(statement->ifStatement->thenBlock)
				&& _supportsStatements(statement->ifStatement->elseBlock);
		case WHILE_STATEMENT:
			return _supportsExpression(statement->whileStatement->condition)
				&& _supportsStatements(statement->whileStatement->body);
		case FOR_STATEMENT:
			return _supportsExpression(statement->forStatement->from)
				&& _supportsExpression(statement->forStatement->to)
				&& _supportsStatements(statement->forStatement->body);
		case PROCEDURE_CALL_STATEMENT:
			return _supportsExpressions(statement->procedureCall->arguments);
		case RETURN_STATEMENT:
			return _supportsExpression(statement->returnStatement->value);
		case PROCEDURE_DEFINITION: {
			const ProcedureDefinition * procedure = statement->procedureDefinition;
			if (procedure->hasReturnType && !_supportedType(procedure->returnType)) return false;
			for (const ParameterList * item = procedure->parameters; item != NULL; item = item->next) {
				if (!_supportedType(item->parameter->dataType)) return false;
			}
			return _supportsStatements(procedure->body);
		}
		default:
			return false;
	}
}

static bool _supportsStatements(const StatementList * list) {
	for (; list != NULL; list = list->next) {
		if (!_supportsStatement(list->statement)) return false;
	}
	return true;
}

bool runtimeSupports(const Program * program) {
	return _supportsStatements(program->statements);
}

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

static bool _evaluateIndex(Runtime * runtime, RuntimeScope * scope, const Expression * expression, Value * result) {
	Value vector;
	int index;
	if (!_evaluate(runtime, scope, expression->leftExpression, &vector)) return false;
	if (!_evaluateInteger(runtime, scope, expression->rightExpression, &index)) {
		destroyValue(&vector);
		return false;
	}
	if (!vector.isVector && vector.type == TYPE_SCALE) {
		result->type = TYPE_NOTE;
		MusicStatus status = musicScaleNote(&vector.scale, index, &result->note);
		destroyValue(&vector);
		if (status != MUSIC_OK) return _error(runtime, "index %d is out of range for a scale of length 7", index);
		return true;
	}
	if (index < 0 || (size_t) index >= vector.vector.count) {
		size_t length = vector.vector.count;
		destroyValue(&vector);
		return _error(runtime, "index %d is out of range for a vector of length %zu", index, length);
	}
	*result = vector.vector.items[index];
	vector.vector.items[index] = (Value) {.type = TYPE_INTEGER};
	destroyValue(&vector);
	return true;
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
			if (expression->type == SCALE_OF_KEY) status = musicKeyScale(key.key, &result->scale);
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
			result->type = TYPE_CHORD;
			status = musicChordFromNotes(notes, count, &result->chord);
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
			result->type = TYPE_CHORD;
			status = musicDegreeChord(key.key, degree - 1, size, &result->chord);
			destroyValue(&key);
			operation = "degree chord";
			break;
		}
		case PROPERTY_ACCESS: {
			Value object;
			if (!_evaluate(runtime, scope, expression->property.object, &object)) return false;
			const char * name = expression->property.name;
			operation = name;
			if (!strcmp(name, "octave")) result->integer = musicNoteOctave(object.note);
			else if (!strcmp(name, "root")) {
				result->type = TYPE_NOTE;
				status = musicChordRoot(&object.chord, &result->note);
			} else if (!strcmp(name, "quality")) {
				result->type = VALUE_QUALITY;
				result->quality = musicChordQuality(&object.chord);
			} else if (!strcmp(name, "tonic")) {
				result->type = TYPE_NOTE;
				status = musicKeyTonic(object.key, &result->note);
			} else if (!strcmp(name, "mode")) {
				result->type = VALUE_MODE;
				result->mode = musicKeyMode(object.key);
			} else if (!strcmp(name, "length")) {
				if (object.progression.count > INT_MAX) {
					destroyValue(&object);
					return _error(runtime, "progression length exceeds the integer range");
				}
				result->integer = (int) object.progression.count;
			}
			destroyValue(&object);
			break;
		}
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

static bool _evaluate(Runtime * runtime, RuntimeScope * scope, const Expression * expression, Value * result) {
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
		case DIVISION: {
			int left, right;
			if (!_evaluateInteger(runtime, scope, expression->leftExpression, &left)
				|| !_evaluateInteger(runtime, scope, expression->rightExpression, &right)) return false;
			return _arithmetic(runtime, expression->type, left, right, &result->integer);
		}
		default:
			return _evaluateMusic(runtime, scope, expression, result);
	}
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
		ok = _evaluate(runtime, scope, argument->expression, &value);
		if (!ok) break;
		if (parameter->parameter->isVector) value.type = parameter->parameter->dataType;
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
		if (procedure->returnsVector) runtime->returned.type = procedure->returnType;
		if (result != NULL) *result = runtime->returned;
		else destroyValue(&runtime->returned);
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

static Flow _executeFor(Runtime * runtime, RuntimeScope * scope, const For * loop) {
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

static Flow _execute(Runtime * runtime, RuntimeScope * scope, const Statement * statement) {
	if (!_checkStack(runtime)) return FLOW_ERROR;
	switch (statement->type) {
		case DECLARATION: {
			Value value;
			if (!_evaluate(runtime, scope, statement->expression, &value)) return FLOW_ERROR;
			if (statement->isVector) value.type = statement->dataType;
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
			if (binding->value.isVector) value.type = binding->value.type;
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

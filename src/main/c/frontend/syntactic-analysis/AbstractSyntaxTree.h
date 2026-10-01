#ifndef ABSTRACT_SYNTAX_TREE_HEADER
#define ABSTRACT_SYNTAX_TREE_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeAbstractSyntaxTreeModule();

/**
 * This type definitions allows self-referencing types (e.g., an expression
 * that is made of another expressions, such as talking about you in 3rd
 * person, but without the madness).
 */

typedef enum DataType DataType;
typedef enum ExpressionType ExpressionType;
typedef enum Mode Mode;
typedef enum StatementType StatementType;

typedef struct Expression Expression;
typedef struct ExpressionList ExpressionList;
typedef struct Program Program;
typedef struct Statement Statement;
typedef struct StatementList StatementList;

/**
 * Node types for the Abstract Syntax Tree (AST).
 */

enum DataType {
	TYPE_BOOLEAN,
	TYPE_CHORD,
	TYPE_INTEGER,
	TYPE_INTERVAL,
	TYPE_KEY,
	TYPE_NOTE,
	TYPE_PROGRESSION,
	TYPE_SCALE,
	TYPE_STRING,
	TYPE_VOICING
};

enum ExpressionType {
	ADDITION,
	ARPEGGIATION,
	BOOLEAN_LITERAL,
	CONJUNCTION,
	DEGREE_LITERAL,
	DIMINISHED_LITERAL,
	DISJUNCTION,
	DIVISION,
	DOMINANT_OF_KEY,
	DURATION_LITERAL,
	EQUALITY,
	GREATER_THAN,
	GREATER_THAN_OR_EQUAL,
	INDEX_ACCESS,
	INEQUALITY,
	IN_EXPRESSION,
	INTEGER_LITERAL,
	INTERVAL_LITERAL,
	INVERSION,
	KEY_LITERAL,
	LESS_THAN,
	LESS_THAN_OR_EQUAL,
	LIST_LITERAL,
	MODULATION,
	MULTIPLICATION,
	NEGATION,
	NINTH_CHORD,
	NOTE_LITERAL,
	NOTES_CHORD,
	PROPERTY_ACCESS,
	RELATIVE_MINOR_OF_KEY,
	SCALE_OF_KEY,
	SEVENTH_CHORD,
	SUBDOMINANT_OF_KEY,
	SUBTRACTION,
	TRIAD_CHORD,
	VARIABLE
};

enum Mode {
	MODE_DORIAN,
	MODE_LOCRIAN,
	MODE_LYDIAN,
	MODE_MAJOR,
	MODE_MINOR,
	MODE_MIXOLYDIAN,
	MODE_PHRYGIAN
};

enum StatementType {
	ASSIGNMENT,
	DECLARATION
};

/**
 * An expression. The literal notes and intervals keep their lexeme, and the
 * variables keep their name, in "text". The scale and the tonal relations
 * (dominant, subdominant and relative minor) of a key use "operand". The
 * chords built on a degree use the left expression for the degree and the
 * right one for the key, and the chords built on explicit notes use
 * "elements". The lists between brackets also use "elements", which is NULL
 * when the list is empty. The index access uses the left expression for the
 * indexed value and the right one for the index. The operator "in" is the
 * same for a progression of degrees over a key and for the membership of a
 * note, and the semantic analysis tells them apart. The inversion of a
 * chord, its arpeggiation and the modulation of a progression use the left
 * expression for the chord or progression and the right one for the number of
 * inversions, the duration or the target key.
 */
struct Expression {
	union {
		bool boolean;
		char * text;
		int integer;
		Expression * operand;
		ExpressionList * elements;
		struct {
			char * tonic;
			Mode mode;
			bool strict;
		} key;
		struct {
			Expression * object;
			char * name;
		} property;
		struct {
			Expression * leftExpression;
			Expression * rightExpression;
		};
	};
	ExpressionType type;
};

/** A non-empty list of expressions, in program order. */
struct ExpressionList {
	Expression * expression;
	ExpressionList * next;
};

/**
 * A declaration ("dataType" and "isVector" are used) or an assignment to a
 * variable.
 */
struct Statement {
	char * name;
	Expression * expression;
	DataType dataType;
	bool isVector;
	StatementType type;
};

/** A non-empty list of statements, in program order. */
struct StatementList {
	Statement * statement;
	StatementList * next;
};

struct Program {
	StatementList * statements;
};

/**
 * Node recursive super-duper-trambolik-destructors.
 */

void destroyExpression(Expression * expression);
void destroyExpressionList(ExpressionList * expressionList);
void destroyProgram(Program * program);
void destroyStatement(Statement * statement);
void destroyStatementList(StatementList * statementList);

#endif

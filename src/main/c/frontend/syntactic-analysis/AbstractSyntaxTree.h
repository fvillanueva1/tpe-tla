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
typedef enum StatementType StatementType;

typedef struct Expression Expression;
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
	BOOLEAN_LITERAL,
	CONJUNCTION,
	DISJUNCTION,
	DIVISION,
	EQUALITY,
	GREATER_THAN,
	GREATER_THAN_OR_EQUAL,
	INEQUALITY,
	INTEGER_LITERAL,
	INTERVAL_LITERAL,
	LESS_THAN,
	LESS_THAN_OR_EQUAL,
	MULTIPLICATION,
	NEGATION,
	NOTE_LITERAL,
	SUBTRACTION,
	VARIABLE
};

enum StatementType {
	ASSIGNMENT,
	DECLARATION
};

/**
 * An expression. The literal notes and intervals keep their lexeme, and the
 * variables keep their name, in "text".
 */
struct Expression {
	union {
		bool boolean;
		char * text;
		int integer;
		Expression * operand;
		struct {
			Expression * leftExpression;
			Expression * rightExpression;
		};
	};
	ExpressionType type;
};

/** A declaration ("dataType" is used) or an assignment to a variable. */
struct Statement {
	char * name;
	Expression * expression;
	DataType dataType;
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
void destroyProgram(Program * program);
void destroyStatement(Statement * statement);
void destroyStatementList(StatementList * statementList);

#endif

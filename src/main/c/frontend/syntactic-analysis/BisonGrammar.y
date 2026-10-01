%{

#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"

/**
 * The error reporting function for Bison parser.
 *
 * @todo Add location to the grammar and "pushToken" API function.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {}

%}

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
	/** Terminals. */

	signed int integer;
	TokenLabel token;

	/** Non-terminals. */

	Constant * constant;
	Expression * expression;
	Factor * factor;
	Program * program;
}

/**
 * Destructors. This functions are executed after the parsing ends, so if the
 * AST must be used in the following phases of the compiler you shouldn't used
 * this approach for the AST root node ("program" non-terminal, in this
 * grammar), or it will drop the entire tree even if the parsing succeeds.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Destructor-Decl.html
 */
%destructor { destroyConstant($$); } <constant>
%destructor { destroyExpression($$); } <expression>
%destructor { destroyFactor($$); } <factor>

/** Terminals. */
%token <integer> INTEGER
%token <token> ADD
%token <token> AND
%token <token> ARPEGGIATE
%token <token> ARROW
%token <token> AS
%token <token> ASSIGN
%token <token> AT
%token <token> AUGMENTED
%token <token> BOOLEAN_TYPE
%token <token> BPM
%token <token> BY
%token <token> CHECK
%token <token> CHORD
%token <token> CLOSE_BRACE
%token <token> CLOSE_BRACKET
%token <token> CLOSE_COMMENT
%token <token> CLOSE_PARENTHESIS
%token <token> COMMA
%token <token> DEFINE
%token <token> DIMINISHED
%token <token> DIV
%token <token> DOMINANT
%token <token> DORIAN
%token <token> DOT
%token <token> ELSE
%token <token> EQUAL_EQUAL
%token <token> EXPORT
%token <token> FALSE
%token <token> FOR
%token <token> GREATER
%token <token> GREATER_EQUAL
%token <token> IF
%token <token> IN
%token <token> INTEGER_TYPE
%token <token> INTERVAL_TYPE
%token <token> INVERT
%token <token> KEY
%token <token> LESS
%token <token> LESS_EQUAL
%token <token> LOCRIAN
%token <token> LOG
%token <token> LYDIAN
%token <token> MAJOR
%token <token> MIDI
%token <token> MINOR
%token <token> MIXOLYDIAN
%token <token> MODULATE
%token <token> MUL
%token <token> NINTH
%token <token> NOT
%token <token> NOTE_TYPE
%token <token> NOT_EQUAL
%token <token> OF
%token <token> ON
%token <token> OPEN_BRACE
%token <token> OPEN_BRACKET
%token <token> OPEN_COMMENT
%token <token> OPEN_PARENTHESIS
%token <token> OR
%token <token> PARALLEL_FIFTHS
%token <token> PARALLEL_OCTAVES
%token <token> PHRYGIAN
%token <token> PROGRESSION
%token <token> RELATIVE
%token <token> RETURN
%token <token> SATB
%token <token> SCALE
%token <token> SEMICOLON
%token <token> SEVENTH
%token <token> SHEET
%token <token> STRICT
%token <token> STRING_TYPE
%token <token> SUB
%token <token> SUBDOMINANT
%token <token> TO
%token <token> TRIAD
%token <token> TRUE
%token <token> VOICE
%token <token> VOICE_CROSSING
%token <token> VOICING
%token <token> WHILE
%token <token> WITH

%token <token> IGNORED
%token <token> UNKNOWN

/** Non-terminals. */
%type <constant> constant
%type <expression> expression
%type <factor> factor
%type <program> program

/**
 * Precedence and associativity.
 *
 * @see https://en.cppreference.com/w/cpp/language/operator_precedence.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Precedence.html
 */
%left ADD SUB
%left MUL DIV

%%

// IMPORTANT: To use λ in the following grammar, use the %empty symbol.

program: expression											{ $$ = ExpressionProgramSemanticAction($1); }
	;

expression: expression[left] ADD expression[right]			{ $$ = ArithmeticExpressionSemanticAction($left, $right, ADDITION); }
	| expression[left] DIV expression[right]				{ $$ = ArithmeticExpressionSemanticAction($left, $right, DIVISION); }
	| expression[left] MUL expression[right]				{ $$ = ArithmeticExpressionSemanticAction($left, $right, MULTIPLICATION); }
	| expression[left] SUB expression[right]				{ $$ = ArithmeticExpressionSemanticAction($left, $right, SUBTRACTION); }
	| factor												{ $$ = FactorExpressionSemanticAction($1); }
	;

factor: OPEN_PARENTHESIS expression CLOSE_PARENTHESIS		{ $$ = ExpressionFactorSemanticAction($2); }
	| constant												{ $$ = ConstantFactorSemanticAction($1); }
	;

constant: INTEGER											{ $$ = IntegerConstantSemanticAction($1); }
	;

%%

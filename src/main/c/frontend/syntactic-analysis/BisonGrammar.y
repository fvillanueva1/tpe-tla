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

	char * string;
	signed int integer;
	TokenLabel token;

	/** Non-terminals. */

	DataType dataType;
	Expression * expression;
	ExpressionList * expressionList;
	Mode mode;
	Parameter * parameter;
	ParameterList * parameterList;
	Program * program;
	Statement * statement;
	StatementList * statementList;
}

/**
 * Destructors. This functions are executed after the parsing ends, so if the
 * AST must be used in the following phases of the compiler you shouldn't used
 * this approach for the AST root node ("program" non-terminal, in this
 * grammar), or it will drop the entire tree even if the parsing succeeds.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Destructor-Decl.html
 */
%destructor { free($$); } <string>
%destructor { destroyExpression($$); } <expression>
%destructor { destroyExpressionList($$); } <expressionList>
%destructor { destroyParameter($$); } <parameter>
%destructor { destroyParameterList($$); } <parameterList>
%destructor { destroyStatement($$); } <statement>
%destructor { destroyStatementList($$); } <statementList>

/** Terminals. */

// Literals.
%token <integer> INTEGER
%token <string> DEGREE
%token <string> DURATION
%token <string> ID
%token <string> INTERVAL
%token <string> NOTE
%token <string> STRING_TEXT

// Types.
%token <token> BOOLEAN_TYPE
%token <token> CHORD_TYPE
%token <token> INTEGER_TYPE
%token <token> INTERVAL_TYPE
%token <token> KEY_TYPE
%token <token> NOTE_TYPE
%token <token> PROGRESSION_TYPE
%token <token> SCALE_TYPE
%token <token> STRING_TYPE
%token <token> VOICING_TYPE

// Modes and tonal relations.
%token <token> DIMINISHED
%token <token> DOMINANT
%token <token> DORIAN
%token <token> LOCRIAN
%token <token> LYDIAN
%token <token> MAJOR
%token <token> MINOR
%token <token> MIXOLYDIAN
%token <token> PHRYGIAN
%token <token> RELATIVE
%token <token> STRICT
%token <token> SUBDOMINANT

// Harmony.
%token <token> ARPEGGIATE
%token <token> BY
%token <token> INVERT
%token <token> MODULATE
%token <token> NINTH
%token <token> SEVENTH
%token <token> TRIAD

// Voice leading.
%token <token> CHECK
%token <token> PARALLEL_FIFTHS
%token <token> PARALLEL_OCTAVES
%token <token> SATB
%token <token> VOICE
%token <token> VOICE_CROSSING

// Output.
%token <token> BPM
%token <token> EXPORT
%token <token> LOG
%token <token> MIDI
%token <token> SHEET

// Control flow and procedures.
%token <token> DEFINE
%token <token> ELSE
%token <token> FALSE
%token <token> FOR
%token <token> IF
%token <token> RETURN
%token <token> TRUE
%token <token> WHILE

// Words shared by several constructions.
%token <token> AS
%token <token> AT
%token <token> IN
%token <token> OF
%token <token> ON
%token <token> TO
%token <token> WITH

// Operators.
%token <token> ADD
%token <token> AND
%token <token> ASSIGN
%token <token> DIV
%token <token> EQUAL
%token <token> GREATER
%token <token> GREATER_EQUAL
%token <token> LESS
%token <token> LESS_EQUAL
%token <token> MUL
%token <token> NOT
%token <token> NOT_EQUAL
%token <token> OR
%token <token> SUB

// Punctuation.
%token <token> ARROW
%token <token> CLOSE_BRACE
%token <token> CLOSE_BRACKET
%token <token> CLOSE_INTERPOLATION
%token <token> CLOSE_PARENTHESIS
%token <token> CLOSE_STRING
%token <token> COMMA
%token <token> DOT
%token <token> OPEN_BRACE
%token <token> OPEN_BRACKET
%token <token> OPEN_INTERPOLATION
%token <token> OPEN_PARENTHESIS
%token <token> OPEN_STRING
%token <token> SEMICOLON

// Comments and lexical errors.
%token <token> CLOSE_COMMENT
%token <token> OPEN_COMMENT

%token <token> IGNORED
%token <token> UNKNOWN

/** Non-terminals. */
%type <expressionList> arguments
%type <statementList> block
%type <dataType> dataType
%type <expression> expression
%type <expressionList> expressionList
%type <statement> ifStatement
%type <mode> mode
%type <parameter> parameter
%type <parameterList> parameterList
%type <parameterList> parameters
%type <program> program
%type <statement> statement
%type <statementList> statementList

/**
 * Precedence and associativity, from the lowest to the highest. "not" binds
 * weaker than the relational operators, so "not a == b" is "not (a == b)".
 * The relational operators are not associative: "a < b < c" is rejected. "of"
 * binds tighter than any operator, so "scale of k == j" is "(scale of k) == j",
 * and the index and the property access bind even tighter, so "scale of ks[0]"
 * is "scale of (ks[0])". "by", "as" and "to" have the lowest precedence, so
 * what follows them is a whole expression: "invert c by n + 1" is
 * "invert c by (n + 1)".
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Precedence.html
 */
%nonassoc BY AS TO
%left OR
%left AND
%right NOT
%nonassoc EQUAL NOT_EQUAL LESS LESS_EQUAL GREATER GREATER_EQUAL IN
%left ADD SUB
%left MUL DIV
%right OF
%left OPEN_BRACKET DOT

%expect 0

%%

// IMPORTANT: To use λ in the following grammar, use the %empty symbol.

program: statementList										{ $$ = StatementsProgramSemanticAction($1); }
	;

statementList: statement statementList						{ $$ = StatementListSemanticAction($1, $2); }
	| statement												{ $$ = StatementListSemanticAction($1, NULL); }
	;

statement: dataType ID ASSIGN expression SEMICOLON			{ $$ = DeclarationStatementSemanticAction($1, false, $2, $4); }
	| dataType OPEN_BRACKET CLOSE_BRACKET ID ASSIGN expression SEMICOLON	{ $$ = DeclarationStatementSemanticAction($1, true, $4, $6); }
	| ID ASSIGN expression SEMICOLON						{ $$ = AssignmentStatementSemanticAction($1, $3); }
	| ifStatement											{ $$ = $1; }
	| FOR ID ASSIGN expression[from] TO expression[to] block	{ $$ = ForStatementSemanticAction($2, $from, $to, $block); }
	| WHILE OPEN_PARENTHESIS expression CLOSE_PARENTHESIS block	{ $$ = WhileStatementSemanticAction($3, $5); }
	| DEFINE ID OPEN_PARENTHESIS parameters CLOSE_PARENTHESIS block	{ $$ = ProcedureDefinitionSemanticAction($2, $4, false, TYPE_INTEGER, false, $6); }
	| DEFINE ID OPEN_PARENTHESIS parameters CLOSE_PARENTHESIS ARROW dataType block	{ $$ = ProcedureDefinitionSemanticAction($2, $4, true, $7, false, $8); }
	| DEFINE ID OPEN_PARENTHESIS parameters CLOSE_PARENTHESIS ARROW dataType OPEN_BRACKET CLOSE_BRACKET block	{ $$ = ProcedureDefinitionSemanticAction($2, $4, true, $7, true, $10); }
	| RETURN expression SEMICOLON							{ $$ = ReturnStatementSemanticAction($2); }
	| RETURN SEMICOLON										{ $$ = ReturnStatementSemanticAction(NULL); }
	| ID OPEN_PARENTHESIS arguments CLOSE_PARENTHESIS SEMICOLON	{ $$ = ProcedureCallStatementSemanticAction($1, $3); }
	;

// Las llaves son obligatorias, así que no hay ambigüedad con "else" (dangling else).
ifStatement: IF OPEN_PARENTHESIS expression CLOSE_PARENTHESIS block[then]	{ $$ = IfStatementSemanticAction($3, $then, NULL); }
	| IF OPEN_PARENTHESIS expression CLOSE_PARENTHESIS block[then] ELSE block[else]	{ $$ = IfStatementSemanticAction($3, $then, $else); }
	| IF OPEN_PARENTHESIS expression CLOSE_PARENTHESIS block[then] ELSE ifStatement[elseIf]	{ $$ = IfStatementSemanticAction($3, $then, StatementListSemanticAction($elseIf, NULL)); }
	;

block: OPEN_BRACE CLOSE_BRACE								{ $$ = NULL; }
	| OPEN_BRACE statementList CLOSE_BRACE					{ $$ = $2; }
	;

parameters: %empty											{ $$ = NULL; }
	| parameterList											{ $$ = $1; }
	;

parameterList: parameter COMMA parameterList				{ $$ = ParameterListSemanticAction($1, $3); }
	| parameter												{ $$ = ParameterListSemanticAction($1, NULL); }
	;

parameter: dataType ID										{ $$ = ParameterSemanticAction($1, false, $2); }
	| dataType OPEN_BRACKET CLOSE_BRACKET ID				{ $$ = ParameterSemanticAction($1, true, $4); }
	;

arguments: %empty											{ $$ = NULL; }
	| expressionList										{ $$ = $1; }
	;

dataType: BOOLEAN_TYPE										{ $$ = TYPE_BOOLEAN; }
	| CHORD_TYPE											{ $$ = TYPE_CHORD; }
	| INTEGER_TYPE											{ $$ = TYPE_INTEGER; }
	| INTERVAL_TYPE											{ $$ = TYPE_INTERVAL; }
	| KEY_TYPE												{ $$ = TYPE_KEY; }
	| NOTE_TYPE												{ $$ = TYPE_NOTE; }
	| PROGRESSION_TYPE										{ $$ = TYPE_PROGRESSION; }
	| SCALE_TYPE											{ $$ = TYPE_SCALE; }
	| STRING_TYPE											{ $$ = TYPE_STRING; }
	| VOICING_TYPE											{ $$ = TYPE_VOICING; }
	;

expression: expression[left] ADD expression[right]			{ $$ = BinaryExpressionSemanticAction($left, $right, ADDITION); }
	| VOICE expression[progression] AS SATB %prec AS		{ $$ = VoiceExpressionSemanticAction($progression); }
	| expression[left] SUB expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, SUBTRACTION); }
	| expression[left] MUL expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, MULTIPLICATION); }
	| expression[left] DIV expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, DIVISION); }
	| expression[left] EQUAL expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, EQUALITY); }
	| expression[left] NOT_EQUAL expression[right]			{ $$ = BinaryExpressionSemanticAction($left, $right, INEQUALITY); }
	| expression[left] LESS expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, LESS_THAN); }
	| expression[left] LESS_EQUAL expression[right]			{ $$ = BinaryExpressionSemanticAction($left, $right, LESS_THAN_OR_EQUAL); }
	| expression[left] GREATER expression[right]			{ $$ = BinaryExpressionSemanticAction($left, $right, GREATER_THAN); }
	| expression[left] GREATER_EQUAL expression[right]		{ $$ = BinaryExpressionSemanticAction($left, $right, GREATER_THAN_OR_EQUAL); }
	| expression[left] AND expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, CONJUNCTION); }
	| expression[left] OR expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, DISJUNCTION); }
	| NOT expression[operand]								{ $$ = NegationExpressionSemanticAction($operand); }
	| OPEN_PARENTHESIS expression CLOSE_PARENTHESIS			{ $$ = $2; }
	| INTEGER												{ $$ = IntegerLiteralSemanticAction($1); }
	| TRUE													{ $$ = BooleanLiteralSemanticAction(true); }
	| FALSE													{ $$ = BooleanLiteralSemanticAction(false); }
	| NOTE													{ $$ = NoteLiteralSemanticAction($1); }
	| INTERVAL												{ $$ = IntervalLiteralSemanticAction($1); }
	| ID													{ $$ = VariableExpressionSemanticAction($1); }
	| ID OPEN_PARENTHESIS arguments CLOSE_PARENTHESIS		{ $$ = ProcedureCallExpressionSemanticAction($1, $3); }
	| NOTE mode												{ $$ = KeyLiteralSemanticAction($1, $2, false); }
	| NOTE mode STRICT										{ $$ = KeyLiteralSemanticAction($1, $2, true); }
	| SCALE_TYPE OF expression[key]							{ $$ = KeyRelationExpressionSemanticAction($key, SCALE_OF_KEY); }
	| RELATIVE MINOR OF expression[key]						{ $$ = KeyRelationExpressionSemanticAction($key, RELATIVE_MINOR_OF_KEY); }
	| DOMINANT OF expression[key]							{ $$ = KeyRelationExpressionSemanticAction($key, DOMINANT_OF_KEY); }
	| SUBDOMINANT OF expression[key]						{ $$ = KeyRelationExpressionSemanticAction($key, SUBDOMINANT_OF_KEY); }
	| DEGREE												{ $$ = DegreeLiteralSemanticAction($1); }
	| TRIAD ON expression[degree] OF expression[key]		{ $$ = BinaryExpressionSemanticAction($degree, $key, TRIAD_CHORD); }
	| SEVENTH ON expression[degree] OF expression[key]		{ $$ = BinaryExpressionSemanticAction($degree, $key, SEVENTH_CHORD); }
	| NINTH ON expression[degree] OF expression[key]		{ $$ = BinaryExpressionSemanticAction($degree, $key, NINTH_CHORD); }
	| OPEN_BRACE expressionList CLOSE_BRACE					{ $$ = NotesChordExpressionSemanticAction($2); }
	| OPEN_BRACKET CLOSE_BRACKET							{ $$ = ListLiteralSemanticAction(NULL); }
	| OPEN_BRACKET expressionList CLOSE_BRACKET				{ $$ = ListLiteralSemanticAction($2); }
	| expression[left] IN expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, IN_EXPRESSION); }
	| expression[object] OPEN_BRACKET expression[index] CLOSE_BRACKET	{ $$ = BinaryExpressionSemanticAction($object, $index, INDEX_ACCESS); }
	| expression[object] DOT ID								{ $$ = PropertyAccessSemanticAction($object, $3); }
	| DIMINISHED											{ $$ = DiminishedLiteralSemanticAction(); }
	| DURATION												{ $$ = DurationLiteralSemanticAction($1); }
	| INVERT expression[chord] BY expression[count]			{ $$ = BinaryExpressionSemanticAction($chord, $count, INVERSION); }
	| ARPEGGIATE expression[chord] AS expression[duration]	{ $$ = BinaryExpressionSemanticAction($chord, $duration, ARPEGGIATION); }
	| MODULATE expression[progression] TO expression[key]	{ $$ = BinaryExpressionSemanticAction($progression, $key, MODULATION); }
	;

expressionList: expression COMMA expressionList				{ $$ = ExpressionListSemanticAction($1, $3); }
	| expression											{ $$ = ExpressionListSemanticAction($1, NULL); }
	;

mode: DORIAN												{ $$ = MODE_DORIAN; }
	| LOCRIAN												{ $$ = MODE_LOCRIAN; }
	| LYDIAN												{ $$ = MODE_LYDIAN; }
	| MAJOR													{ $$ = MODE_MAJOR; }
	| MINOR													{ $$ = MODE_MINOR; }
	| MIXOLYDIAN											{ $$ = MODE_MIXOLYDIAN; }
	| PHRYGIAN												{ $$ = MODE_PHRYGIAN; }
	;

%%

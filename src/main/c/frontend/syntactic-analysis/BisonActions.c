#include "BisonActions.h"

/* MODULE INTERNAL STATE */

static CompilerState * _compilerState = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownBisonActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: BisonActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_compilerState = NULL;
}

ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState) {
	_compilerState = compilerState;
	_logger = createLogger("BisonActions");
	return _shutdownBisonActionsModule;
}

/* IMPORTED FUNCTIONS */

/* PRIVATE FUNCTIONS */

static void _logSyntacticAnalyzerAction(const char * functionName);

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void _logSyntacticAnalyzerAction(const char * functionName) {
	logDebugging(_logger, "%s", functionName);
}

/* PUBLIC FUNCTIONS */

Expression * BinaryExpressionSemanticAction(Expression * leftExpression, Expression * rightExpression, ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->leftExpression = leftExpression;
	expression->rightExpression = rightExpression;
	expression->type = type;
	return expression;
}

Expression * BooleanLiteralSemanticAction(const bool value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->boolean = value;
	expression->type = BOOLEAN_LITERAL;
	return expression;
}

Expression * DiminishedLiteralSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->type = DIMINISHED_LITERAL;
	return expression;
}

Expression * DegreeLiteralSemanticAction(char * lexeme) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->text = lexeme;
	expression->type = DEGREE_LITERAL;
	return expression;
}

Expression * IntegerLiteralSemanticAction(const int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->integer = value;
	expression->type = INTEGER_LITERAL;
	return expression;
}

Expression * IntervalLiteralSemanticAction(char * lexeme) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->text = lexeme;
	expression->type = INTERVAL_LITERAL;
	return expression;
}

Expression * KeyLiteralSemanticAction(char * tonic, Mode mode, bool strict) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->key.tonic = tonic;
	expression->key.mode = mode;
	expression->key.strict = strict;
	expression->type = KEY_LITERAL;
	return expression;
}

Expression * KeyRelationExpressionSemanticAction(Expression * key, ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->operand = key;
	expression->type = type;
	return expression;
}

Expression * ListLiteralSemanticAction(ExpressionList * elements) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->elements = elements;
	expression->type = LIST_LITERAL;
	return expression;
}

Expression * NegationExpressionSemanticAction(Expression * operand) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->operand = operand;
	expression->type = NEGATION;
	return expression;
}

Expression * NoteLiteralSemanticAction(char * lexeme) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->text = lexeme;
	expression->type = NOTE_LITERAL;
	return expression;
}

Expression * NotesChordExpressionSemanticAction(ExpressionList * elements) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->elements = elements;
	expression->type = NOTES_CHORD;
	return expression;
}

Expression * PropertyAccessSemanticAction(Expression * object, char * name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->property.object = object;
	expression->property.name = name;
	expression->type = PROPERTY_ACCESS;
	return expression;
}

Expression * VariableExpressionSemanticAction(char * name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->text = name;
	expression->type = VARIABLE;
	return expression;
}

ExpressionList * ExpressionListSemanticAction(Expression * expression, ExpressionList * next) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ExpressionList * expressionList = calloc(1, sizeof(ExpressionList));
	expressionList->expression = expression;
	expressionList->next = next;
	return expressionList;
}

Program * StatementsProgramSemanticAction(StatementList * statements) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Program * program = calloc(1, sizeof(Program));
	program->statements = statements;
	_compilerState->abstractSyntaxtTree = program;
	return program;
}

Statement * AssignmentStatementSemanticAction(char * name, Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = calloc(1, sizeof(Statement));
	statement->expression = expression;
	statement->name = name;
	statement->type = ASSIGNMENT;
	return statement;
}

Statement * DeclarationStatementSemanticAction(DataType dataType, const bool isVector, char * name, Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = calloc(1, sizeof(Statement));
	statement->dataType = dataType;
	statement->expression = expression;
	statement->isVector = isVector;
	statement->name = name;
	statement->type = DECLARATION;
	return statement;
}

StatementList * StatementListSemanticAction(Statement * statement, StatementList * next) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	StatementList * statementList = calloc(1, sizeof(StatementList));
	statementList->next = next;
	statementList->statement = statement;
	return statementList;
}

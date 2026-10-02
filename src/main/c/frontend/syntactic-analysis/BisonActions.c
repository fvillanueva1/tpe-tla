#include "BisonActions.h"
#include "../../support/language/String.h"

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
static ProcedureCall * _createProcedureCall(char * name, ExpressionList * arguments);
static Statement * _createStatement(StatementType type);

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void _logSyntacticAnalyzerAction(const char * functionName) {
	logDebugging(_logger, "%s", functionName);
}

/**
 * A procedure call is the same node as a statement and as an expression.
 */
static ProcedureCall * _createProcedureCall(char * name, ExpressionList * arguments) {
	ProcedureCall * procedureCall = calloc(1, sizeof(ProcedureCall));
	procedureCall->arguments = arguments;
	procedureCall->name = name;
	return procedureCall;
}

static Statement * _createStatement(StatementType type) {
	Statement * statement = calloc(1, sizeof(Statement));
	statement->type = type;
	return statement;
}

/* PUBLIC FUNCTIONS */

char * AppendTextSemanticAction(char * text, char * fragment) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	char * result = concatenate(2, text, fragment);
	free(text);
	free(fragment);
	return result;
}

Statement * ExportStatementSemanticAction(Expression * value, ExportFormat format, char * path, Expression * tempo, char * instrument) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Export * export = calloc(1, sizeof(Export));
	export->value = value;
	export->format = format;
	export->path = path;
	export->tempo = tempo;
	export->instrument = instrument;
	Statement * statement = _createStatement(EXPORT_STATEMENT);
	statement->export = export;
	return statement;
}

Statement * CheckStatementSemanticAction(Expression * voicing, unsigned int rules) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Check * check = calloc(1, sizeof(Check));
	check->voicing = voicing;
	check->rules = rules;
	Statement * statement = _createStatement(CHECK_STATEMENT);
	statement->check = check;
	return statement;
}

Expression * VoiceExpressionSemanticAction(Expression * progression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Voice * voice = calloc(1, sizeof(Voice));
	voice->progression = progression;
	Expression * expression = calloc(1, sizeof(Expression));
	expression->voice = voice;
	expression->type = VOICE_EXPRESSION;
	return expression;
}

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

Expression * DurationLiteralSemanticAction(char * lexeme) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->text = lexeme;
	expression->type = DURATION_LITERAL;
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

Expression * ProcedureCallExpressionSemanticAction(char * name, ExpressionList * arguments) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->call = _createProcedureCall(name, arguments);
	expression->type = PROCEDURE_CALL;
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

Parameter * ParameterSemanticAction(DataType dataType, const bool isVector, char * name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Parameter * parameter = calloc(1, sizeof(Parameter));
	parameter->dataType = dataType;
	parameter->isVector = isVector;
	parameter->name = name;
	return parameter;
}

ParameterList * ParameterListSemanticAction(Parameter * parameter, ParameterList * next) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ParameterList * parameterList = calloc(1, sizeof(ParameterList));
	parameterList->next = next;
	parameterList->parameter = parameter;
	return parameterList;
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

Statement * ForStatementSemanticAction(char * variable, Expression * from, Expression * to, StatementList * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(FOR_STATEMENT);
	statement->forStatement = calloc(1, sizeof(For));
	statement->forStatement->body = body;
	statement->forStatement->from = from;
	statement->forStatement->to = to;
	statement->forStatement->variable = variable;
	return statement;
}

Statement * IfStatementSemanticAction(Expression * condition, StatementList * thenBlock, StatementList * elseBlock) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(IF_STATEMENT);
	statement->ifStatement = calloc(1, sizeof(If));
	statement->ifStatement->condition = condition;
	statement->ifStatement->elseBlock = elseBlock;
	statement->ifStatement->thenBlock = thenBlock;
	return statement;
}

Statement * ProcedureCallStatementSemanticAction(char * name, ExpressionList * arguments) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(PROCEDURE_CALL_STATEMENT);
	statement->procedureCall = _createProcedureCall(name, arguments);
	return statement;
}

Statement * ProcedureDefinitionSemanticAction(char * name, ParameterList * parameters, const bool hasReturnType, DataType returnType, const bool returnsVector, StatementList * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(PROCEDURE_DEFINITION);
	statement->procedureDefinition = calloc(1, sizeof(ProcedureDefinition));
	statement->procedureDefinition->body = body;
	statement->procedureDefinition->hasReturnType = hasReturnType;
	statement->procedureDefinition->name = name;
	statement->procedureDefinition->parameters = parameters;
	statement->procedureDefinition->returnType = returnType;
	statement->procedureDefinition->returnsVector = returnsVector;
	return statement;
}

Statement * ReturnStatementSemanticAction(Expression * value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(RETURN_STATEMENT);
	statement->returnStatement = calloc(1, sizeof(Return));
	statement->returnStatement->value = value;
	return statement;
}

Statement * WhileStatementSemanticAction(Expression * condition, StatementList * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(WHILE_STATEMENT);
	statement->whileStatement = calloc(1, sizeof(While));
	statement->whileStatement->body = body;
	statement->whileStatement->condition = condition;
	return statement;
}

StatementList * StatementListSemanticAction(Statement * statement, StatementList * next) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	StatementList * statementList = calloc(1, sizeof(StatementList));
	statementList->next = next;
	statementList->statement = statement;
	return statementList;
}

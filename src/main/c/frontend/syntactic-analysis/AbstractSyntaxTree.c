#include "AbstractSyntaxTree.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownAbstractSyntaxTreeModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: AbstractSyntaxTree...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeAbstractSyntaxTreeModule() {
	_logger = createLogger("AbstractSyntaxTree");
	return _shutdownAbstractSyntaxTreeModule;
}

/* PUBLIC FUNCTIONS */

void destroyExpression(Expression * expression) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (expression != NULL) {
		switch (expression->type) {
			case DEGREE_LITERAL:
			case DURATION_LITERAL:
			case INTERVAL_LITERAL:
			case NOTE_LITERAL:
			case VARIABLE:
				free(expression->text);
				break;
			case KEY_LITERAL:
				free(expression->key.tonic);
				break;
			case LIST_LITERAL:
			case NOTES_CHORD:
				destroyExpressionList(expression->elements);
				break;
			case PROCEDURE_CALL:
				destroyProcedureCall(expression->call);
				break;
			case VOICE_EXPRESSION:
				destroyVoice(expression->voice);
				break;
			case PROPERTY_ACCESS:
				destroyExpression(expression->property.object);
				free(expression->property.name);
				break;
			case DOMINANT_OF_KEY:
			case NEGATION:
			case RELATIVE_MINOR_OF_KEY:
			case SCALE_OF_KEY:
			case SUBDOMINANT_OF_KEY:
				destroyExpression(expression->operand);
				break;
			case ADDITION:
			case ARPEGGIATION:
			case CONJUNCTION:
			case DISJUNCTION:
			case DIVISION:
			case EQUALITY:
			case GREATER_THAN:
			case GREATER_THAN_OR_EQUAL:
			case INDEX_ACCESS:
			case INEQUALITY:
			case INVERSION:
			case IN_EXPRESSION:
			case LESS_THAN:
			case LESS_THAN_OR_EQUAL:
			case MODULATION:
			case MULTIPLICATION:
			case NINTH_CHORD:
			case SEVENTH_CHORD:
			case SUBTRACTION:
			case TRIAD_CHORD:
				destroyExpression(expression->leftExpression);
				destroyExpression(expression->rightExpression);
				break;
			case BOOLEAN_LITERAL:
			case DIMINISHED_LITERAL:
			case INTEGER_LITERAL:
				break;
		}
		free(expression);
	}
}

void destroyExpressionList(ExpressionList * expressionList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (expressionList != NULL) {
		destroyExpression(expressionList->expression);
		destroyExpressionList(expressionList->next);
		free(expressionList);
	}
}

void destroyVoice(Voice * voice) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (voice != NULL) {
		destroyExpression(voice->progression);
		free(voice);
	}
}

void destroyCheck(Check * check) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (check != NULL) {
		destroyExpression(check->voicing);
		free(check);
	}
}

void destroyExport(Export * export) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (export != NULL) {
		destroyExpression(export->value);
		destroyExpression(export->tempo);
		free(export->path);
		free(export->instrument);
		free(export);
	}
}

void destroyFor(For * forStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (forStatement != NULL) {
		free(forStatement->variable);
		destroyExpression(forStatement->from);
		destroyExpression(forStatement->to);
		destroyStatementList(forStatement->body);
		free(forStatement);
	}
}

void destroyIf(If * ifStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (ifStatement != NULL) {
		destroyExpression(ifStatement->condition);
		destroyStatementList(ifStatement->thenBlock);
		destroyStatementList(ifStatement->elseBlock);
		free(ifStatement);
	}
}

void destroyParameter(Parameter * parameter) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (parameter != NULL) {
		free(parameter->name);
		free(parameter);
	}
}

void destroyParameterList(ParameterList * parameterList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (parameterList != NULL) {
		destroyParameter(parameterList->parameter);
		destroyParameterList(parameterList->next);
		free(parameterList);
	}
}

void destroyProcedureCall(ProcedureCall * procedureCall) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (procedureCall != NULL) {
		free(procedureCall->name);
		destroyExpressionList(procedureCall->arguments);
		free(procedureCall);
	}
}

void destroyProcedureDefinition(ProcedureDefinition * procedureDefinition) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (procedureDefinition != NULL) {
		free(procedureDefinition->name);
		destroyParameterList(procedureDefinition->parameters);
		destroyStatementList(procedureDefinition->body);
		free(procedureDefinition);
	}
}

void destroyProgram(Program * program) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (program != NULL) {
		destroyStatementList(program->statements);
		free(program);
	}
}

void destroyReturn(Return * returnStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (returnStatement != NULL) {
		destroyExpression(returnStatement->value);
		free(returnStatement);
	}
}

void destroyStatement(Statement * statement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (statement != NULL) {
		switch (statement->type) {
			case ASSIGNMENT:
			case DECLARATION:
				free(statement->name);
				destroyExpression(statement->expression);
				break;
			case CHECK_STATEMENT:
				destroyCheck(statement->check);
				break;
			case EXPORT_STATEMENT:
				destroyExport(statement->export);
				break;
			case FOR_STATEMENT:
				destroyFor(statement->forStatement);
				break;
			case IF_STATEMENT:
				destroyIf(statement->ifStatement);
				break;
			case PROCEDURE_CALL_STATEMENT:
				destroyProcedureCall(statement->procedureCall);
				break;
			case PROCEDURE_DEFINITION:
				destroyProcedureDefinition(statement->procedureDefinition);
				break;
			case RETURN_STATEMENT:
				destroyReturn(statement->returnStatement);
				break;
			case WHILE_STATEMENT:
				destroyWhile(statement->whileStatement);
				break;
		}
		free(statement);
	}
}

void destroyStatementList(StatementList * statementList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (statementList != NULL) {
		destroyStatement(statementList->statement);
		destroyStatementList(statementList->next);
		free(statementList);
	}
}

void destroyWhile(While * whileStatement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (whileStatement != NULL) {
		destroyExpression(whileStatement->condition);
		destroyStatementList(whileStatement->body);
		free(whileStatement);
	}
}

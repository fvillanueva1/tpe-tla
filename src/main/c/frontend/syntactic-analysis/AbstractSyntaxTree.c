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
			case INTERVAL_LITERAL:
			case NOTE_LITERAL:
			case VARIABLE:
				free(expression->text);
				break;
			case KEY_LITERAL:
				free(expression->key.tonic);
				break;
			case DOMINANT_OF_KEY:
			case NEGATION:
			case RELATIVE_MINOR_OF_KEY:
			case SCALE_OF_KEY:
			case SUBDOMINANT_OF_KEY:
				destroyExpression(expression->operand);
				break;
			case ADDITION:
			case CONJUNCTION:
			case DISJUNCTION:
			case DIVISION:
			case EQUALITY:
			case GREATER_THAN:
			case GREATER_THAN_OR_EQUAL:
			case INEQUALITY:
			case LESS_THAN:
			case LESS_THAN_OR_EQUAL:
			case MULTIPLICATION:
			case SUBTRACTION:
				destroyExpression(expression->leftExpression);
				destroyExpression(expression->rightExpression);
				break;
			case BOOLEAN_LITERAL:
			case INTEGER_LITERAL:
				break;
		}
		free(expression);
	}
}

void destroyProgram(Program * program) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (program != NULL) {
		destroyStatementList(program->statements);
		free(program);
	}
}

void destroyStatement(Statement * statement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (statement != NULL) {
		free(statement->name);
		destroyExpression(statement->expression);
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

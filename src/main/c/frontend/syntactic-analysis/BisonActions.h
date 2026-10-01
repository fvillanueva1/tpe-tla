#ifndef BISON_ACTIONS_HEADER
#define BISON_ACTIONS_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonParser.h"
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeBisonActionsModule();

/**
 * Bison semantic actions.
 */

Expression * BinaryExpressionSemanticAction(Expression * leftExpression, Expression * rightExpression, ExpressionType type);
Expression * BooleanLiteralSemanticAction(const bool value);
Expression * DegreeLiteralSemanticAction(char * lexeme);
Expression * IntegerLiteralSemanticAction(const int value);
Expression * IntervalLiteralSemanticAction(char * lexeme);
Expression * KeyLiteralSemanticAction(char * tonic, Mode mode, bool strict);
Expression * KeyRelationExpressionSemanticAction(Expression * key, ExpressionType type);
Expression * NegationExpressionSemanticAction(Expression * operand);
Expression * NoteLiteralSemanticAction(char * lexeme);
Expression * NotesChordExpressionSemanticAction(ExpressionList * elements);
Expression * VariableExpressionSemanticAction(char * name);
ExpressionList * ExpressionListSemanticAction(Expression * expression, ExpressionList * next);
Program * StatementsProgramSemanticAction(StatementList * statements);
Statement * AssignmentStatementSemanticAction(char * name, Expression * expression);
Statement * DeclarationStatementSemanticAction(DataType dataType, char * name, Expression * expression);
StatementList * StatementListSemanticAction(Statement * statement, StatementList * next);

#endif

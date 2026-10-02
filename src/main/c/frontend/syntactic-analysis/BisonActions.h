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
Expression * DiminishedLiteralSemanticAction();
Expression * DegreeLiteralSemanticAction(char * lexeme);
Expression * DurationLiteralSemanticAction(char * lexeme);
Expression * IntegerLiteralSemanticAction(const int value);
Expression * IntervalLiteralSemanticAction(char * lexeme);
Expression * KeyLiteralSemanticAction(char * tonic, Mode mode, bool strict);
Expression * KeyRelationExpressionSemanticAction(Expression * key, ExpressionType type);
Expression * ListLiteralSemanticAction(ExpressionList * elements);
Expression * NegationExpressionSemanticAction(Expression * operand);
Expression * NoteLiteralSemanticAction(char * lexeme);
Expression * NotesChordExpressionSemanticAction(ExpressionList * elements);
Expression * ProcedureCallExpressionSemanticAction(char * name, ExpressionList * arguments);
Expression * PropertyAccessSemanticAction(Expression * object, char * name);
Expression * VariableExpressionSemanticAction(char * name);
Expression * VoiceExpressionSemanticAction(Expression * progression);
ExpressionList * ExpressionListSemanticAction(Expression * expression, ExpressionList * next);
Parameter * ParameterSemanticAction(DataType dataType, const bool isVector, char * name);
ParameterList * ParameterListSemanticAction(Parameter * parameter, ParameterList * next);
Program * StatementsProgramSemanticAction(StatementList * statements);
Statement * AssignmentStatementSemanticAction(char * name, Expression * expression);
Statement * DeclarationStatementSemanticAction(DataType dataType, const bool isVector, char * name, Expression * expression);
Statement * ForStatementSemanticAction(char * variable, Expression * from, Expression * to, StatementList * body);
Statement * IfStatementSemanticAction(Expression * condition, StatementList * thenBlock, StatementList * elseBlock);
Statement * ProcedureCallStatementSemanticAction(char * name, ExpressionList * arguments);
Statement * ProcedureDefinitionSemanticAction(char * name, ParameterList * parameters, const bool hasReturnType, DataType returnType, const bool returnsVector, StatementList * body);
Statement * ReturnStatementSemanticAction(Expression * value);
Statement * WhileStatementSemanticAction(Expression * condition, StatementList * body);
StatementList * StatementListSemanticAction(Statement * statement, StatementList * next);

#endif

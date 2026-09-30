#ifndef ABSTRACT_SYNTAX_TREE_HEADER
#define ABSTRACT_SYNTAX_TREE_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeAbstractSyntaxTreeModule();

/**
 * This type definitions allows self-referencing types (e.g., an expression
 * that is made of another expressions, such as talking about you in 3rd
 * person, but without the madness).
 */

typedef struct Program Program;

/**
 * Node types for the Abstract Syntax Tree (AST).
 */

/** The root node. It has no children until the grammar defines statements. */
struct Program {};

/**
 * Node recursive super-duper-trambolik-destructors.
 */

void destroyProgram(Program * program);

#endif

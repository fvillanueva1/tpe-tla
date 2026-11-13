#ifndef RUNTIME_HEADER
#define RUNTIME_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/type/CompilationStatus.h"
#include <stdbool.h>

/** Maximum depth of nested procedure calls before a runtime error. */
#define RUNTIME_MAX_CALL_DEPTH 1000

/**
 * Executes a program that passed the semantic analysis. "log" prints to the
 * standard output. A runtime error prints "Runtime error: ..." to the
 * standard error and returns FAILED; a failed allocation returns
 * OUT_OF_MEMORY.
 */
CompilationStatus executeProgram(Program * program);

#endif

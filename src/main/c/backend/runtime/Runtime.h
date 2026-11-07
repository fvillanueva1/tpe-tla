#ifndef RUNTIME_HEADER
#define RUNTIME_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/type/CompilationStatus.h"
#include <stdbool.h>

/**
 * True when every construct of the program can be executed by the runtime.
 * It is checked before executing anything, so an unsupported program never
 * prints a partial "log" output.
 */
bool runtimeSupports(const Program * program);

/**
 * Executes a program that passed the semantic analysis. "log" prints to the
 * standard output. A runtime error prints "Runtime error: ..." to the
 * standard error and returns FAILED; a failed allocation returns
 * OUT_OF_MEMORY.
 */
CompilationStatus executeProgram(Program * program);

#endif

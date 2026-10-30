#ifndef SEMANTIC_HEADER
#define SEMANTIC_HEADER
#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/type/CompilationStatus.h"
CompilationStatus analyzeSemantics(Program *program);
/* Return the zero-based General MIDI program number, or -1 for an unknown name. */
int semanticInstrumentProgram(const char *name);
#endif

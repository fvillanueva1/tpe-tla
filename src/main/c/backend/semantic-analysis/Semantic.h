#ifndef SEMANTIC_HEADER
#define SEMANTIC_HEADER
#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/type/CompilationStatus.h"
CompilationStatus analyzeSemantics(Program *program);
#endif

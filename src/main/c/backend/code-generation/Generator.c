#include "Generator.h"
#include "../runtime/Runtime.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownGeneratorModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Generator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeGeneratorModule() {
	_logger = createLogger("Generator");
	return _shutdownGeneratorModule;
}

/** PUBLIC FUNCTIONS */

CompilationStatus executeGenerator(CompilerState * compilerState) {
    return executeProgram(compilerState->abstractSyntaxtTree);
}

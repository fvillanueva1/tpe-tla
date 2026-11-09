#include "Generator.h"
#include "../music/Music.h"
#include "../output/Output.h"
#include "../semantic-analysis/Semantic.h"
#include <stdio.h>
#include <string.h>

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

static CompilationStatus exportLiteralScale(Program *program, bool *handled) {
    *handled = false;
    StatementList *first = program->statements;
    if (!first || !first->next) return SUCCEEDED;
    Statement *declaration = first->statement;
    StatementList *last = first->next;
    Statement *scale = NULL;
    if (last->next) {
        scale = last->statement;
        last = last->next;
    }
    if (last->next) return SUCCEEDED;
    Statement *output = last->statement;
    if (declaration->type != DECLARATION || declaration->dataType != TYPE_KEY || declaration->isVector ||
        declaration->expression->type != KEY_LITERAL || output->type != EXPORT_STATEMENT) return SUCCEEDED;
    Export *export = output->export;
    Expression *value = export->value;
    if (scale) {
        if (scale->type != DECLARATION || scale->dataType != TYPE_SCALE || scale->isVector ||
            value->type != VARIABLE || strcmp(value->text, scale->name)) return SUCCEEDED;
        value = scale->expression;
    }
    if (export->format != EXPORT_MIDI || value->type != SCALE_OF_KEY ||
        value->operand->type != VARIABLE || strcmp(value->operand->text, declaration->name) ||
        (export->tempo && export->tempo->type != INTEGER_LITERAL) || strchr(export->path, '\\')) return SUCCEEDED;
    *handled = true;
    MusicKey key = {.mode = (MusicMode)declaration->expression->key.mode, .strict = declaration->expression->key.strict};
    MusicScale keyScale;
    if (musicParseSpelling(declaration->expression->key.tonic, &key.tonic) != MUSIC_OK ||
        musicKeyScale(key, &keyScale) != MUSIC_OK) return FAILED;
    int bpm = export->tempo ? export->tempo->integer : OUTPUT_DEFAULT_TEMPO;
    int instrument = export->instrument ? semanticInstrumentProgram(export->instrument) : OUTPUT_DEFAULT_INSTRUMENT;
    OutputScore score;
    OutputStatus status = outputScoreInit(&score, bpm, instrument);
    if (status == OUTPUT_OK) status = outputAddScale(&score, &keyScale);
    if (status == OUTPUT_OK) status = outputWriteMidi(&score, export->path);
    outputScoreDestroy(&score);
    switch (status) {
    case OUTPUT_OK: return SUCCEEDED;
    case OUTPUT_NO_MEMORY: logError(_logger, "Out of memory while writing MIDI output."); return OUT_OF_MEMORY;
    case OUTPUT_FILE_EXISTS: logError(_logger, "Cannot create MIDI output (it already exists)."); return FAILED;
    default: logError(_logger, "Cannot write MIDI output."); return FAILED;
    }
}

CompilationStatus executeGenerator(CompilerState * compilerState) {
    bool handled;
    CompilationStatus status = exportLiteralScale(compilerState->abstractSyntaxtTree, &handled);
    if (!handled) {
        logError(_logger, "Code generation is not implemented for this program.");
        return FAILED;
    }
    return status;
}

#include "Generator.h"
#include "../semantic-analysis/MusicValidation.h"
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
    if (!first || !first->next || first->next->next) return SUCCEEDED;
    Statement *declaration = first->statement, *output = first->next->statement;
    if (declaration->type != DECLARATION || declaration->dataType != TYPE_KEY || declaration->isVector ||
        declaration->expression->type != KEY_LITERAL || output->type != EXPORT_STATEMENT) return SUCCEEDED;
    Export *export = output->export;
    if (export->format != EXPORT_MIDI || export->value->type != SCALE_OF_KEY ||
        export->value->operand->type != VARIABLE || strcmp(export->value->operand->text, declaration->name) ||
        (export->tempo && export->tempo->type != INTEGER_LITERAL) || strchr(export->path, '\\')) return SUCCEEDED;
    *handled = true;
    static const int pitches[] = {9, 11, 0, 2, 4, 5, 7};
    const char *tonic = declaration->expression->key.tonic;
    int root = 60 + pitches[tonic[0] - 'A'];
    if (tonic[1] == '#') ++root;
    else if (tonic[1] == 'b') --root;
    int notes[7];
    if (!musicScale(root, declaration->expression->key.mode, notes)) return FAILED;
    int bpm = export->tempo ? export->tempo->integer : 120;
    unsigned int micros = 60000000u / (unsigned int)bpm;
    int instrument = export->instrument ? semanticInstrumentProgram(export->instrument) : 0;
    unsigned char midi[99] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 1,0xe0,
        'M','T','r','k', 0,0,0,77,
        0,0xff,0x51,3, (unsigned char)(micros >> 16), (unsigned char)(micros >> 8), (unsigned char)micros,
        0,0xc0,(unsigned char)instrument
    };
    size_t pos = 32;
    for (int i = 0; i < 7; ++i) {
        midi[pos++] = 0; midi[pos++] = 0x90; midi[pos++] = (unsigned char)notes[i]; midi[pos++] = 64;
        midi[pos++] = 0x83; midi[pos++] = 0x60; midi[pos++] = 0x80; midi[pos++] = (unsigned char)notes[i]; midi[pos++] = 0;
    }
    midi[pos++] = 0; midi[pos++] = 0xff; midi[pos++] = 0x2f; midi[pos++] = 0;
    FILE *file = fopen(export->path, "wbx");
    if (!file) { logError(_logger, "Cannot create MIDI output (it may already exist)."); return FAILED; }
    bool ok = fwrite(midi, 1, pos, file) == pos;
    if (fclose(file) != 0) ok = false;
    if (!ok) { remove(export->path); logError(_logger, "Cannot write MIDI output."); return FAILED; }
    return SUCCEEDED;
}

CompilationStatus executeGenerator(CompilerState * compilerState) {
    bool handled;
    CompilationStatus status = exportLiteralScale(compilerState->abstractSyntaxtTree, &handled);
    if (!handled) logWarning(_logger, "Code generation is not implemented for this program.");
    return status;
}

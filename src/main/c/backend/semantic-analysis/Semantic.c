#include "Semantic.h"
#include "MusicValidation.h"
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

enum { TYPE_DEGREE = 11, TYPE_QUALITY, TYPE_MODE, TYPE_EMPTY_LIST, TYPE_CHORD_LIST, TYPE_DEGREE_LIST, TYPE_VECTOR = 20, TYPE_INVALID = -2 };

typedef struct Symbol { const char *name; int type; struct Symbol *next; const Expression *literal, *currentLiteral; } Symbol;
typedef struct Scope { Symbol *symbols; struct Scope *parent; Program *program; ProcedureDefinition *procedure; CompilationStatus *failure; const Expression *keyContext; bool hasKeyContext; Symbol *keyOwner; } Scope;

static bool nameAssigned(StatementList *list, const char *name) {
    for (; list; list = list->next) {
        Statement *s = list->statement;
        switch (s->type) {
        case ASSIGNMENT: if (!strcmp(s->name, name)) return true; break;
        case IF_STATEMENT:
            if (nameAssigned(s->ifStatement->thenBlock, name) || nameAssigned(s->ifStatement->elseBlock, name)) return true;
            break;
        case WHILE_STATEMENT: if (nameAssigned(s->whileStatement->body, name)) return true; break;
        case FOR_STATEMENT: if (nameAssigned(s->forStatement->body, name)) return true; break;
        case PROCEDURE_DEFINITION: if (nameAssigned(s->procedureDefinition->body, name)) return true; break;
        default: break;
        }
    }
    return false;
}
static Symbol *findSymbol(Scope *scope, const char *name) {
    for (; scope; scope = scope->parent)
        for (Symbol *s = scope->symbols; s; s = s->next)
            if (!strcmp(s->name, name)) return s;
    return NULL;
}
static const Expression *knownScalar(Scope *scope, const Expression *expression) {
    if (!expression) return NULL;
    if (expression->type == VARIABLE) {
        Symbol *symbol = findSymbol(scope, expression->text);
        return symbol ? symbol->currentLiteral : NULL;
    }
    switch (expression->type) {
    case INTEGER_LITERAL: case NOTE_LITERAL: case INTERVAL_LITERAL: case KEY_LITERAL:
        return expression;
    default: return NULL;
    }
}
static void forgetCurrentValues(Scope *scope) {
    for (; scope; scope = scope->parent)
        for (Symbol *symbol = scope->symbols; symbol; symbol = symbol->next) symbol->currentLiteral = NULL;
}
static bool declare(Scope *scope, const char *name, int type) {
    for (Symbol *s = scope->symbols; s; s = s->next)
        if (!strcmp(s->name, name)) { fprintf(stderr, "Semantic error: '%s' is already declared in this scope.\n", name); return false; }
    Symbol *s = malloc(sizeof(*s));
    if (!s) { fprintf(stderr, "Semantic analysis: out of memory.\n"); *scope->failure = OUT_OF_MEMORY; return false; }
    *s = (Symbol){name, type, scope->symbols, NULL, NULL}; scope->symbols = s; return true;
}
static bool lookup(Scope *scope, const char *name) {
    for (; scope; scope = scope->parent)
        for (Symbol *s = scope->symbols; s; s = s->next)
            if (!strcmp(s->name, name)) return true;
    fprintf(stderr, "Semantic error: variable '%s' is not declared.\n", name); return false;
}
static void clear(Scope *scope) {
    while (scope->symbols) { Symbol *s = scope->symbols; scope->symbols = s->next; free(s); }
}
static bool expression(Scope *, Expression *);
static int typeOf(Scope *, Expression *);
static bool compatible(Scope *, Expression *, int);
static bool statements(Scope *, StatementList *);
static bool parts(Scope *scope, StringPart *part) {
    for (; part; part = part->next)
        if (part->type == STRING_PART_VARIABLE && !lookup(scope, part->text)) return false;
    return true;
}
static bool expressions(Scope *scope, ExpressionList *list) {
    for (; list; list = list->next) if (!expression(scope, list->expression)) return false;
    return true;
}
static ProcedureDefinition *findProcedure(Scope *scope, const char *name) {
    for (StatementList *item = scope->program->statements; item; item = item->next)
        if (item->statement->type == PROCEDURE_DEFINITION && !strcmp(item->statement->procedureDefinition->name, name))
            return item->statement->procedureDefinition;
    return NULL;
}
static void forgetKeyContext(Scope *scope) {
    scope->hasKeyContext = true;
    scope->keyContext = NULL;
    scope->keyOwner = NULL;
}
static bool call(Scope *scope, ProcedureCall *call, bool needsValue) {
    forgetKeyContext(scope);
    forgetCurrentValues(scope);
    ProcedureDefinition *p = findProcedure(scope, call->name);
    if (!p) { fprintf(stderr, "Semantic error: unknown procedure '%s'.\n", call->name); return false; }
    if (needsValue && !p->hasReturnType) { fprintf(stderr, "Semantic error: procedure '%s' has no return value.\n", call->name); return false; }
    ParameterList *param = p->parameters; ExpressionList *arg = call->arguments;
    while (param && arg) {
        if (!expression(scope, arg->expression) || !compatible(scope, arg->expression, param->parameter->dataType + (param->parameter->isVector ? 20 : 0))) return false;
        param = param->next; arg = arg->next;
    }
    if (param || arg) { fprintf(stderr, "Semantic error: incorrect argument count for '%s'.\n", call->name); return false; }
    return true;
}
static bool expression(Scope *scope, Expression *e) {
    if (!e) return true;
    switch (e->type) {
    case VARIABLE: return lookup(scope, e->text);
    case STRING_LITERAL: return parts(scope, e->stringParts);
    case LIST_LITERAL: case NOTES_CHORD: return expressions(scope, e->elements);
    case PROCEDURE_CALL: return call(scope, e->call, true);
    case PROPERTY_ACCESS: return expression(scope, e->property.object);
    case VOICE_EXPRESSION: return expression(scope, e->voice->progression);
    case NEGATION: case DOMINANT_OF_KEY: case RELATIVE_MINOR_OF_KEY:
    case SCALE_OF_KEY: case SUBDOMINANT_OF_KEY: return expression(scope, e->operand);
    case ADDITION: case ARPEGGIATION: case CONJUNCTION: case DISJUNCTION:
    case DIVISION: case EQUALITY: case GREATER_THAN: case GREATER_THAN_OR_EQUAL:
    case INDEX_ACCESS: case INEQUALITY: case IN_EXPRESSION: case INVERSION:
    case LESS_THAN: case LESS_THAN_OR_EQUAL: case MODULATION: case MULTIPLICATION:
    case NINTH_CHORD: case SEVENTH_CHORD: case SUBTRACTION: case TRIAD_CHORD:
        return expression(scope, e->leftExpression) && expression(scope, e->rightExpression);
    default: return true;
    }
}
static bool block(Scope *parent, StatementList *list) {
    Scope local = {NULL, parent, parent->program, parent->procedure, parent->failure}; bool ok = statements(&local, list); clear(&local); return ok;
}
/* List literals keep their ambiguity until an expected type is available. */
static int literalPitch(const char *note) {
    static const int pitch[] = {9, 11, 0, 2, 4, 5, 7};
    int value = pitch[note[0] - 'A'];
    const char *p = note + 1;
    if (*p == '#') { ++value; ++p; }
    else if (*p == 'b') { --value; ++p; }
    if (*p < '0' || *p > '8' || p[1]) return -10000;
    return (*p - '0') * 12 + value;
}
static int literalInterval(const char *interval) {
    static const struct { const char *name; int semitones; } intervals[] = {
        {"P1", 0}, {"m2", 1}, {"M2", 2}, {"m3", 3}, {"M3", 4}, {"P4", 5},
        {"aug4", 6}, {"d5", 6}, {"P5", 7}, {"m6", 8}, {"M6", 9}, {"m7", 10}, {"M7", 11}, {"P8", 12}
    };
    for (size_t i = 0; i < sizeof(intervals) / sizeof(intervals[0]); ++i)
        if (!strcmp(interval, intervals[i].name)) return intervals[i].semitones;
    return -1;
}
/* Evaluate only side-effect-free integer literals; runtime values remain unknown. */
static bool constantInteger(Scope *scope, Expression *e, int *result) {
    if (e->type == VARIABLE) {
        Symbol *symbol = findSymbol(scope, e->text);
        if (!symbol || !symbol->currentLiteral || symbol->currentLiteral->type != INTEGER_LITERAL) return false;
        *result = symbol->currentLiteral->integer; return true;
    }
    if (e->type == INTEGER_LITERAL) { *result = e->integer; return true; }
    if (e->type != ADDITION && e->type != SUBTRACTION && e->type != MULTIPLICATION && e->type != DIVISION) return false;
    int left, right;
    if (!constantInteger(scope, e->leftExpression, &left) || !constantInteger(scope, e->rightExpression, &right)) return false;
    int64_t value;
    switch (e->type) {
    case ADDITION: value = (int64_t)left + right; break;
    case SUBTRACTION: value = (int64_t)left - right; break;
    case MULTIPLICATION: value = (int64_t)left * right; break;
    case DIVISION: if (!right) return false; value = (int64_t)left / right; break;
    default: return false;
    }
    if (value < INT_MIN || value > INT_MAX) return false;
    *result = (int)value; return true;
}
static int typeOf(Scope *scope, Expression *e) {
    if (!e) return -1;
    switch (e->type) {
    case DEGREE_LITERAL: return TYPE_DEGREE;
    case DURATION_LITERAL: return TYPE_DURATION;
    case DIMINISHED_LITERAL: return TYPE_QUALITY;
    case LIST_LITERAL: {
        ExpressionList *item = e->elements;
        if (!item) return TYPE_EMPTY_LIST;
        int element = typeOf(scope, item->expression);
        if (element < 0) return TYPE_INVALID;
        for (item = item->next; item; item = item->next) {
            if (typeOf(scope, item->expression) != element) {
                fprintf(stderr, "Semantic error: inconsistent list element types.\n"); return TYPE_INVALID;
            }
        }
        if (element == TYPE_CHORD) return TYPE_CHORD_LIST;
        if (element == TYPE_DEGREE) return TYPE_DEGREE_LIST;
        if (element >= TYPE_VECTOR || element == TYPE_EMPTY_LIST || element == TYPE_CHORD_LIST || element == TYPE_DEGREE_LIST) {
            fprintf(stderr, "Semantic error: nested lists are not supported.\n"); return TYPE_INVALID;
        }
        return TYPE_VECTOR + element;
    }
    case INTEGER_LITERAL: return TYPE_INTEGER;
    case BOOLEAN_LITERAL: return TYPE_BOOLEAN;
    case NOTE_LITERAL: {
        int pitch = literalPitch(e->text);
        if (pitch == -10000) { fprintf(stderr, "Semantic error: note requires an octave.\n"); return TYPE_INVALID; }
        if (pitch < 0 || pitch > 107) { fprintf(stderr, "Semantic error: note is out of range.\n"); return TYPE_INVALID; }
        return TYPE_NOTE;
    }
    case INTERVAL_LITERAL: return TYPE_INTERVAL;
    case KEY_LITERAL:
        if (literalPitch(e->key.tonic) != -10000) { fprintf(stderr, "Semantic error: key tonic cannot have an octave.\n"); return TYPE_INVALID; }
        return TYPE_KEY;
    case STRING_LITERAL: return TYPE_STRING;
    case NOTES_CHORD: {
        Scope *owner = scope;
        while (owner && !owner->hasKeyContext) owner = owner->parent;
        const Expression *key = owner ? owner->keyContext : NULL;
        static const int pitchClass[] = {9, 11, 0, 2, 4, 5, 7};
        int tonic = 0;
        if (key && key->key.strict) {
            tonic = pitchClass[key->key.tonic[0] - 'A'];
            if (key->key.tonic[1] == '#') ++tonic;
            else if (key->key.tonic[1] == 'b') --tonic;
            tonic = (tonic + 12) % 12;
        }
        for (ExpressionList *item = e->elements; item; item = item->next) {
            if (!compatible(scope, item->expression, TYPE_NOTE)) return TYPE_INVALID;
            const Expression *note = item->expression;
            if (note->type == VARIABLE) {
                Symbol *symbol = findSymbol(scope, note->text);
                note = symbol ? symbol->currentLiteral : NULL;
            }
            if (key && key->key.strict && note && note->type == NOTE_LITERAL) {
                int pitch = literalPitch(note->text) + 12;
                if (!musicInKey(&pitch, 1, tonic, key->key.mode)) {
                    fprintf(stderr, "Semantic error: chord contains a note outside the strict key.\n"); return TYPE_INVALID;
                }
            }
        }
        return TYPE_CHORD;
    }
    case TRIAD_CHORD: case SEVENTH_CHORD: case NINTH_CHORD:
        if (!compatible(scope, e->rightExpression, TYPE_KEY)) return -2;
        int degree = typeOf(scope, e->leftExpression);
        if (degree != TYPE_DEGREE && degree != TYPE_INTEGER) { fprintf(stderr, "Semantic error: chord degree must be a degree or integer.\n"); return TYPE_INVALID; }
        int degreeValue;
        if (degree == TYPE_INTEGER && constantInteger(scope, e->leftExpression, &degreeValue) && (degreeValue < 1 || degreeValue > 7)) {
            fprintf(stderr, "Semantic error: chord degree must be between 1 and 7.\n"); return TYPE_INVALID;
        }
        return TYPE_CHORD;
    case SCALE_OF_KEY:
        return compatible(scope, e->operand, TYPE_KEY) ? TYPE_SCALE : -2;
    case VOICE_EXPRESSION:
        return compatible(scope, e->voice->progression, TYPE_PROGRESSION) ? TYPE_VOICING : -2;
    case INVERSION:
        if (!compatible(scope, e->leftExpression, TYPE_CHORD) || !compatible(scope, e->rightExpression, TYPE_INTEGER)) return TYPE_INVALID;
        int inversions;
        const Expression *chord = e->leftExpression;
        if (chord->type == VARIABLE) { Symbol *symbol = findSymbol(scope, chord->text); chord = symbol ? symbol->literal : NULL; }
        if (chord && chord->type == NOTES_CHORD && constantInteger(scope, e->rightExpression, &inversions)) {
            int count = 0;
            for (ExpressionList *item = chord->elements; item; item = item->next) ++count;
            if (inversions < 0 || inversions > count) {
                fprintf(stderr, "Semantic error: inversion is outside the chord range.\n"); return TYPE_INVALID;
            }
        }
        return TYPE_CHORD;
    case MODULATION:
        return compatible(scope, e->leftExpression, TYPE_PROGRESSION) && compatible(scope, e->rightExpression, TYPE_KEY) ? TYPE_PROGRESSION : -2;
    case ARPEGGIATION:
        return compatible(scope, e->leftExpression, TYPE_CHORD) && compatible(scope, e->rightExpression, TYPE_DURATION) ? TYPE_PROGRESSION : TYPE_INVALID;
    case PROCEDURE_CALL: { ProcedureDefinition *p = findProcedure(scope, e->call->name); return p && p->hasReturnType ? (int)p->returnType + (p->returnsVector ? TYPE_VECTOR : 0) : -1; }
    case INDEX_ACCESS: {
        int object = typeOf(scope, e->leftExpression), index = typeOf(scope, e->rightExpression);
        if (object == -2 || index == -2) return -2;
        if (index != -1 && index != TYPE_INTEGER) { fprintf(stderr, "Semantic error: index must be an integer.\n"); return -2; }
        if (object == -1) return -1;
        int indexValue;
        if (constantInteger(scope, e->rightExpression, &indexValue)) {
            int length = object == TYPE_SCALE ? 7 : -1;
            if (e->leftExpression->type == LIST_LITERAL) {
                length = 0;
                for (ExpressionList *item = e->leftExpression->elements; item; item = item->next) ++length;
            }
            if (indexValue < 0 || (length >= 0 && indexValue >= length)) {
                fprintf(stderr, "Semantic error: index is out of range.\n"); return TYPE_INVALID;
            }
        }
        if (object == TYPE_SCALE) return TYPE_NOTE;
        if (object == TYPE_CHORD_LIST) return TYPE_CHORD;
        if (object == TYPE_DEGREE_LIST) return TYPE_DEGREE;
        if (object >= TYPE_VECTOR && object <= TYPE_VECTOR + TYPE_DURATION) return object - 20;
        fprintf(stderr, "Semantic error: value is not indexable.\n"); return -2;
    }
    case PROPERTY_ACCESS: {
        int object = typeOf(scope, e->property.object);
        if (object == -2 || object == -1) return object;
        if (object == TYPE_NOTE && !strcmp(e->property.name, "octave")) return TYPE_INTEGER;
        if (object == TYPE_CHORD && !strcmp(e->property.name, "root")) return TYPE_NOTE;
        if (object == TYPE_CHORD && !strcmp(e->property.name, "quality")) return TYPE_QUALITY;
        if (object == TYPE_KEY && !strcmp(e->property.name, "tonic")) return TYPE_NOTE;
        if (object == TYPE_KEY && !strcmp(e->property.name, "mode")) return TYPE_MODE;
        if (object == TYPE_PROGRESSION && !strcmp(e->property.name, "length")) return TYPE_INTEGER;
        fprintf(stderr, "Semantic error: unknown property '%s'.\n", e->property.name); return -2;
    }
    case DOMINANT_OF_KEY: case SUBDOMINANT_OF_KEY: case RELATIVE_MINOR_OF_KEY: {
        int t = typeOf(scope, e->operand);
        if (t == -2) return -2;
        if (t != -1 && t != TYPE_KEY) { fprintf(stderr, "Semantic error: tonal relation requires a key.\n"); return -2; }
        return TYPE_KEY;
    }
    case IN_EXPRESSION: {
        int a = typeOf(scope, e->leftExpression), b = typeOf(scope, e->rightExpression);
        if (a == -2 || b == -2) return -2;
        if (a == TYPE_NOTE && (b == TYPE_SCALE || b == TYPE_CHORD)) return TYPE_BOOLEAN;
        if ((a == TYPE_DEGREE_LIST || a == TYPE_EMPTY_LIST) && b == TYPE_KEY) return TYPE_PROGRESSION;
        fprintf(stderr, "Semantic error: invalid operands for in.\n"); return TYPE_INVALID;
    }
    case VARIABLE:
        for (Scope *p = scope; p; p = p->parent)
            for (Symbol *symbol = p->symbols; symbol; symbol = symbol->next)
                if (!strcmp(symbol->name, e->text)) return symbol->type;
        return -1;
    case NEGATION: {
        int t = typeOf(scope, e->operand);
        if (t == -1 || t == -2) return t;
        if (t == TYPE_BOOLEAN) return TYPE_BOOLEAN;
        fprintf(stderr, "Semantic error: not requires a boolean.\n"); return -2;
    }
    case CONJUNCTION: case DISJUNCTION: {
        int a = typeOf(scope, e->leftExpression), b = typeOf(scope, e->rightExpression);
        if (a == -2 || b == -2) return -2;
        if ((a != -1 && a != TYPE_BOOLEAN) || (b != -1 && b != TYPE_BOOLEAN)) {
            fprintf(stderr, "Semantic error: logical operators require booleans.\n"); return -2;
        }
        return TYPE_BOOLEAN;
    }
    case EQUALITY: case INEQUALITY: case GREATER_THAN: case GREATER_THAN_OR_EQUAL:
    case LESS_THAN: case LESS_THAN_OR_EQUAL: {
        int a = typeOf(scope, e->leftExpression), b = typeOf(scope, e->rightExpression);
        if (a == -2 || b == -2) return -2;
        if (a != b || (a != TYPE_INTEGER && a != TYPE_NOTE && a != TYPE_INTERVAL &&
            !((e->type == EQUALITY || e->type == INEQUALITY) && (a == TYPE_QUALITY || a == TYPE_MODE)))) {
            fprintf(stderr, "Semantic error: incompatible comparison types.\n"); return -2;
        }
        return TYPE_BOOLEAN;
    }
    case ADDITION: case SUBTRACTION: case MULTIPLICATION: case DIVISION: {
        int a = typeOf(scope, e->leftExpression), b = typeOf(scope, e->rightExpression);
        if (a == -2 || b == -2) return -2;
        if (a == -1 || b == -1) return -1;
        if (a == TYPE_CHORD_LIST || a == TYPE_EMPTY_LIST) a = TYPE_PROGRESSION;
        if (b == TYPE_CHORD_LIST || b == TYPE_EMPTY_LIST) b = TYPE_PROGRESSION;
        if (a == TYPE_INTEGER && b == TYPE_INTEGER) {
            int divisor;
            if (e->type == DIVISION && constantInteger(scope, e->rightExpression, &divisor) && divisor == 0) {
                fprintf(stderr, "Semantic error: division by zero.\n"); return TYPE_INVALID;
            }
            return TYPE_INTEGER;
        }
        if ((e->type == ADDITION || e->type == SUBTRACTION) && b == TYPE_INTERVAL &&
            (a == TYPE_NOTE || a == TYPE_CHORD || a == TYPE_PROGRESSION)) {
            const Expression *note = e->leftExpression, *intervalExpression = e->rightExpression;
            if (note->type == VARIABLE) { Symbol *symbol = findSymbol(scope, note->text); note = symbol ? symbol->currentLiteral : NULL; }
            if (intervalExpression->type == VARIABLE) { Symbol *symbol = findSymbol(scope, intervalExpression->text); intervalExpression = symbol ? symbol->currentLiteral : NULL; }
            if (a == TYPE_NOTE && note && intervalExpression && note->type == NOTE_LITERAL && intervalExpression->type == INTERVAL_LITERAL) {
                int pitch = literalPitch(note->text), interval = literalInterval(intervalExpression->text);
                if (pitch >= 0 && interval >= 0) {
                    int result = pitch + (e->type == ADDITION ? interval : -interval);
                    if (result < 0 || result > 107) { fprintf(stderr, "Semantic error: transposed note is out of range.\n"); return TYPE_INVALID; }
                }
            }
            return a;
        }
        if (e->type == ADDITION && a == TYPE_PROGRESSION && (b == TYPE_PROGRESSION || b == TYPE_CHORD)) return TYPE_PROGRESSION;
        if (e->type == MULTIPLICATION && a == TYPE_PROGRESSION && b == TYPE_INTEGER) {
            int repetitions;
            if (constantInteger(scope, e->rightExpression, &repetitions) && repetitions < 0) {
                fprintf(stderr, "Semantic error: repetition count cannot be negative.\n"); return TYPE_INVALID;
            }
            return TYPE_PROGRESSION;
        }
        fprintf(stderr, "Semantic error: incompatible operand types.\n"); return -2;
    }
    default: fprintf(stderr, "Semantic error: unsupported expression type.\n"); return TYPE_INVALID;
    }
}
static bool compatible(Scope *scope, Expression *e, int expected) {
    int actual = typeOf(scope, e);
    if (actual == TYPE_INVALID) return false;
    if (actual == TYPE_EMPTY_LIST && (expected >= TYPE_VECTOR || expected == TYPE_PROGRESSION)) return true;
    if (actual == TYPE_CHORD_LIST && (expected == TYPE_VECTOR + TYPE_CHORD || expected == TYPE_PROGRESSION)) return true;
    if (actual != expected) {
        fprintf(stderr, "Semantic error: expression type does not match declaration or assignment.\n"); return false;
    }
    return true;
}
static int symbolType(Scope *scope, const char *name) {
    for (; scope; scope = scope->parent)
        for (Symbol *s = scope->symbols; s; s = s->next)
            if (!strcmp(s->name, name)) return s->type;
    return -1;
}
int semanticInstrumentProgram(const char *name) {
    static const char *names[] = {
        "piano", "bright_acoustic_piano", "electric_grand_piano", "honky_tonk_piano", "electric_piano_1", "electric_piano_2", "harpsichord", "clavinet",
        "celesta", "glockenspiel", "music_box", "vibraphone", "marimba", "xylophone", "tubular_bells", "dulcimer",
        "drawbar_organ", "percussive_organ", "rock_organ", "church_organ", "reed_organ", "accordion", "harmonica", "tango_accordion",
        "acoustic_guitar_nylon", "acoustic_guitar_steel", "electric_guitar_jazz", "electric_guitar_clean", "electric_guitar_muted", "overdriven_guitar", "distortion_guitar", "guitar_harmonics",
        "acoustic_bass", "electric_bass_finger", "electric_bass_pick", "fretless_bass", "slap_bass_1", "slap_bass_2", "synth_bass_1", "synth_bass_2",
        "violin", "viola", "cello", "contrabass", "tremolo_strings", "pizzicato_strings", "orchestral_harp", "timpani",
        "string_ensemble_1", "string_ensemble_2", "synth_strings_1", "synth_strings_2", "choir_aahs", "voice_oohs", "synth_choir", "orchestra_hit",
        "trumpet", "trombone", "tuba", "muted_trumpet", "french_horn", "brass_section", "synth_brass_1", "synth_brass_2",
        "soprano_sax", "alto_sax", "tenor_sax", "baritone_sax", "oboe", "english_horn", "bassoon", "clarinet",
        "piccolo", "flute", "recorder", "pan_flute", "blown_bottle", "shakuhachi", "whistle", "ocarina",
        "lead_1_square", "lead_2_sawtooth", "lead_3_calliope", "lead_4_chiff", "lead_5_charang", "lead_6_voice", "lead_7_fifths", "lead_8_bass_lead",
        "pad_1_new_age", "pad_2_warm", "pad_3_polysynth", "pad_4_choir", "pad_5_bowed", "pad_6_metallic", "pad_7_halo", "pad_8_sweep",
        "fx_1_rain", "fx_2_soundtrack", "fx_3_crystal", "fx_4_atmosphere", "fx_5_brightness", "fx_6_goblins", "fx_7_echoes", "fx_8_scifi",
        "sitar", "banjo", "shamisen", "koto", "kalimba", "bagpipe", "fiddle", "shanai",
        "tinkle_bell", "agogo", "steel_drums", "woodblock", "taiko_drum", "melodic_tom", "synth_drum", "reverse_cymbal",
        "guitar_fret_noise", "breath_noise", "seashore", "bird_tweet", "telephone_ring", "helicopter", "applause", "gunshot"
    };
    if (!name) return -1;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) if (!strcmp(name, names[i])) return (int)i;
    return -1;
}
static bool exportStatement(Scope *scope, Export *export) {
    if (!expression(scope, export->value) || !expression(scope, export->tempo)) return false;
    int type = typeOf(scope, export->value);
    if (type != TYPE_NOTE && type != TYPE_SCALE && type != TYPE_CHORD && type != TYPE_PROGRESSION && type != TYPE_VOICING && type != TYPE_CHORD_LIST && type != TYPE_EMPTY_LIST) {
        fprintf(stderr, "Semantic error: value is not exportable.\n"); return false;
    }
    if (export->tempo && !compatible(scope, export->tempo, TYPE_INTEGER)) return false;
    int tempo;
    if (export->tempo && constantInteger(scope, export->tempo, &tempo) &&
        (tempo < (export->format == EXPORT_MIDI ? 4 : 1) ||
         (export->format == EXPORT_MIDI && tempo > 60000000))) {
        fprintf(stderr, "Semantic error: tempo is outside the export format's range.\n"); return false;
    }
    if (export->instrument && semanticInstrumentProgram(export->instrument) < 0) {
        fprintf(stderr, "Semantic error: unknown instrument '%s'.\n", export->instrument); return false;
    }
    return true;
}
/* Only evaluate immutable, literal voice inputs; other cases belong to runtime. */
static const Expression *knownExpression(Scope *scope, const Expression *e) {
    if (e && e->type == VARIABLE) {
        Symbol *symbol = findSymbol(scope, e->text);
        return symbol ? symbol->literal : NULL;
    }
    return e;
}
static int literalDegree(const char *text) {
    static const char *names[] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    char roman[4]; size_t length = 0;
    while (text[length] && text[length] != 'd' && length < sizeof(roman) - 1) {
        roman[length] = (char)toupper((unsigned char)text[length]); ++length;
    }
    roman[length] = '\0';
    for (int i = 0; i < 7; ++i) if (!strcmp(roman, names[i])) return i;
    return -1;
}
static bool knownScale(Scope *scope, const Expression *expression, int scale[7]) {
    const Expression *key = knownExpression(scope, expression);
    if (!key || key->type != KEY_LITERAL) return false;
    static const int pitchClass[] = {9, 11, 0, 2, 4, 5, 7};
    int tonic = 60 + pitchClass[key->key.tonic[0] - 'A'];
    if (key->key.tonic[1] == '#') ++tonic;
    else if (key->key.tonic[1] == 'b') --tonic;
    return musicScale(tonic, key->key.mode, scale);
}
static bool knownChordMask(Scope *scope, const Expression *expression, unsigned short *mask) {
    const Expression *chord = knownExpression(scope, expression);
    if (!chord) return false;
    if (chord->type == TRIAD_CHORD || chord->type == SEVENTH_CHORD) {
        int scale[7], degree = -1;
        const Expression *value = knownExpression(scope, chord->leftExpression);
        if (!value || !knownScale(scope, chord->rightExpression, scale)) return false;
        if (value->type == DEGREE_LITERAL) degree = literalDegree(value->text);
        else if (value->type == INTEGER_LITERAL) degree = value->integer - 1;
        if (degree < 0 || degree > 6) return false;
        int tones = chord->type == TRIAD_CHORD ? 3 : 4;
        for (int i = 0; i < tones; ++i) *mask |= 1u << (scale[(degree + 2 * i) % 7] % 12);
        return true;
    }
    if (chord->type != NOTES_CHORD) return false;
    for (ExpressionList *note = chord->elements; note; note = note->next) {
        const Expression *value = knownExpression(scope, note->expression);
        if (!value || value->type != NOTE_LITERAL) return false;
        int pitch = literalPitch(value->text);
        if (pitch < 0 || pitch > 107) return false;
        *mask |= 1u << (pitch % 12);
    }
    unsigned int classes = 0;
    for (int bit = 0; bit < 12; ++bit) if (*mask & (1u << bit)) ++classes;
    return classes <= VOICE_COUNT;
}
static bool checkKnownVoicing(Scope *scope, const Expression *expression, unsigned int rules) {
    if (scope->parent) return true;
    expression = knownExpression(scope, expression);
    if (!expression || expression->type != VOICE_EXPRESSION) return true;
    const Expression *progression = knownExpression(scope, expression->voice->progression);
    if (!progression) return true;
    int scale[7]; bool degrees = progression->type == IN_EXPRESSION;
    if (degrees) {
        if (progression->leftExpression->type != LIST_LITERAL || !knownScale(scope, progression->rightExpression, scale)) return true;
        progression = progression->leftExpression;
    }
    if (progression->type != LIST_LITERAL) return true;
    size_t count = 0;
    for (ExpressionList *item = progression->elements; item; item = item->next) ++count;
    if (!count) return true;
    if (count > SIZE_MAX / sizeof(unsigned short) || count > SIZE_MAX / sizeof(int[VOICE_COUNT])) {
        *scope->failure = OUT_OF_MEMORY; return false;
    }
    unsigned short *masks = calloc(count, sizeof(*masks));
    int (*voices)[VOICE_COUNT] = calloc(count, sizeof(*voices));
    if (!masks || !voices) {
        free(masks); free(voices); *scope->failure = OUT_OF_MEMORY;
        fprintf(stderr, "Semantic analysis: out of memory.\n"); return false;
    }
    size_t index = 0;
    bool known = true;
    for (ExpressionList *item = progression->elements; item && known; item = item->next, ++index) {
        const Expression *chord = knownExpression(scope, item->expression);
        if (degrees) {
            if (!chord || chord->type != DEGREE_LITERAL) { known = false; break; }
            int degree = literalDegree(chord->text);
            if (degree < 0) { known = false; break; }
            for (int step = 0; step < 3; ++step) masks[index] |= 1u << (scale[(degree + 2 * step) % 7] % 12);
            continue;
        }
        if (!knownChordMask(scope, chord, &masks[index])) known = false;
    }
    bool ok = true;
    if (known) {
        MusicVoiceResult realization = musicRealizeSATB(masks, count, voices);
        if (realization == MUSIC_VOICE_NO_MEMORY) {
            *scope->failure = OUT_OF_MEMORY; fprintf(stderr, "Semantic analysis: out of memory.\n"); ok = false;
        } else if (realization != MUSIC_VOICE_SUCCESS) {
            fprintf(stderr, "Semantic error: cannot realize progression as SATB.\n"); ok = false;
        } else {
            size_t failed = 0;
            MusicValidationResult result = musicCheckVoicing((const int (*)[VOICE_COUNT])voices, count, rules, &failed);
            if (result != MUSIC_VALID) {
                const char *reason = result == MUSIC_TESSITURA_ERROR ? "tessitura" :
                    result == MUSIC_VOICE_CROSSING_ERROR ? "voice crossing" :
                    result == MUSIC_PARALLEL_FIFTHS_ERROR ? "parallel fifths" :
                    result == MUSIC_PARALLEL_OCTAVES_ERROR ? "parallel octaves" : "invalid voicing";
                fprintf(stderr, "Semantic error: %s at chord %zu.\n", reason, failed + 1); ok = false;
            }
        }
    }
    free(masks); free(voices); return ok;
}
static bool returnsOnEveryPath(StatementList *list) {
    for (; list; list = list->next) {
        Statement *s = list->statement;
        if (s->type == RETURN_STATEMENT) return true;
        if (s->type == IF_STATEMENT && s->ifStatement->elseBlock &&
            returnsOnEveryPath(s->ifStatement->thenBlock) && returnsOnEveryPath(s->ifStatement->elseBlock)) return true;
    }
    return false;
}
static bool statement(Scope *scope, Statement *s) {
    switch (s->type) {
    case DECLARATION: {
        if (!expression(scope, s->expression) || !compatible(scope, s->expression, s->dataType + (s->isVector ? 20 : 0))) return false;
        const Expression *value = s->isVector ? NULL : knownScalar(scope, s->expression);
        if (!declare(scope, s->name, s->dataType + (s->isVector ? 20 : 0))) return false;
        scope->symbols->currentLiteral = value;
        if (!s->isVector && !nameAssigned(scope->program->statements, s->name) &&
            ((s->dataType == TYPE_NOTE && s->expression->type == NOTE_LITERAL) ||
             (s->dataType == TYPE_INTEGER && s->expression->type == INTEGER_LITERAL) ||
             (s->dataType == TYPE_INTERVAL && s->expression->type == INTERVAL_LITERAL) ||
             (s->dataType == TYPE_CHORD && (s->expression->type == NOTES_CHORD || s->expression->type == TRIAD_CHORD || s->expression->type == SEVENTH_CHORD)) ||
             (s->dataType == TYPE_KEY && s->expression->type == KEY_LITERAL) ||
             (s->dataType == TYPE_PROGRESSION && (s->expression->type == LIST_LITERAL || s->expression->type == IN_EXPRESSION)) ||
             (s->dataType == TYPE_VOICING && s->expression->type == VOICE_EXPRESSION))) scope->symbols->literal = s->expression;
        if (s->dataType == TYPE_VOICING && !s->isVector && !checkKnownVoicing(scope, s->expression, 0)) return false;
        if (s->dataType == TYPE_KEY && !s->isVector) {
            scope->hasKeyContext = true;
            scope->keyContext = value && value->type == KEY_LITERAL ? value : NULL;
            scope->keyOwner = scope->symbols;
        }
        return true;
    }
    case ASSIGNMENT: {
        if (!lookup(scope, s->name) || !expression(scope, s->expression) || !compatible(scope, s->expression, symbolType(scope, s->name))) return false;
        Symbol *symbol = findSymbol(scope, s->name);
        symbol->currentLiteral = symbol->type >= TYPE_VECTOR ? NULL : knownScalar(scope, s->expression);
        if (symbol->type == TYPE_KEY) {
            Scope *owner = scope;
            while (owner && !owner->hasKeyContext) owner = owner->parent;
            if (owner && owner->keyOwner == symbol) owner->keyContext = symbol->currentLiteral;
        }
        return true;
    }
    case LOG_STATEMENT: return parts(scope, s->log->message);
    case EXPORT_STATEMENT: return exportStatement(scope, s->export);
    case CHECK_STATEMENT: return expression(scope, s->check->voicing) && compatible(scope, s->check->voicing, TYPE_VOICING) && checkKnownVoicing(scope, s->check->voicing, s->check->rules);
    case PROCEDURE_CALL_STATEMENT: return call(scope, s->procedureCall, false);
    case RETURN_STATEMENT:
        if (!scope->procedure) { fprintf(stderr, "Semantic error: return outside procedure.\n"); return false; }
        if (scope->procedure->hasReturnType != (s->returnStatement->value != NULL)) { fprintf(stderr, "Semantic error: incorrect return value.\n"); return false; }
        return expression(scope, s->returnStatement->value) && (!s->returnStatement->value || compatible(scope, s->returnStatement->value, scope->procedure->returnType + (scope->procedure->returnsVector ? 20 : 0)));
    case IF_STATEMENT: {
        forgetKeyContext(scope); forgetCurrentValues(scope);
        bool ok = expression(scope, s->ifStatement->condition) && compatible(scope, s->ifStatement->condition, TYPE_BOOLEAN) && block(scope, s->ifStatement->thenBlock);
        forgetCurrentValues(scope);
        if (ok) ok = block(scope, s->ifStatement->elseBlock);
        forgetCurrentValues(scope); return ok;
    }
    case WHILE_STATEMENT: {
        forgetKeyContext(scope); forgetCurrentValues(scope);
        bool ok = expression(scope, s->whileStatement->condition) && compatible(scope, s->whileStatement->condition, TYPE_BOOLEAN) && block(scope, s->whileStatement->body);
        forgetCurrentValues(scope); return ok;
    }
    case FOR_STATEMENT: {
        forgetKeyContext(scope); forgetCurrentValues(scope);
        For *f = s->forStatement; Scope local = {NULL, scope, scope->program, scope->procedure, scope->failure};
        bool ok = expression(scope, f->from) && expression(scope, f->to) && compatible(scope, f->from, TYPE_INTEGER) && compatible(scope, f->to, TYPE_INTEGER) && declare(&local, f->variable, TYPE_INTEGER) && statements(&local, f->body);
        clear(&local); forgetCurrentValues(scope); return ok;
    }
    case PROCEDURE_DEFINITION: {
        ProcedureDefinition *p = s->procedureDefinition; Scope local = {NULL, scope, scope->program, p, scope->failure}; bool ok = true;
        forgetKeyContext(&local); forgetCurrentValues(scope);
        if (scope->parent) { fprintf(stderr, "Semantic error: nested procedure definition.\n"); return false; }
        for (StatementList *item = scope->program->statements; item; item = item->next)
            if (item->statement != s && item->statement->type == PROCEDURE_DEFINITION && !strcmp(item->statement->procedureDefinition->name, p->name)) { fprintf(stderr, "Semantic error: duplicate procedure '%s'.\n", p->name); return false; }
        for (ParameterList *item = p->parameters; item && ok; item = item->next) ok = declare(&local, item->parameter->name, item->parameter->dataType + (item->parameter->isVector ? 20 : 0));
        if (ok) ok = statements(&local, p->body);
        if (ok && p->hasReturnType && !returnsOnEveryPath(p->body)) {
            fprintf(stderr, "Semantic error: procedure '%s' can finish without returning a value.\n", p->name); ok = false;
        }
        clear(&local); forgetCurrentValues(scope); return ok;
    }
    }
    return false;
}
static bool statements(Scope *scope, StatementList *list) {
    for (; list; list = list->next) if (!statement(scope, list->statement)) return false;
    return true;
}
CompilationStatus analyzeSemantics(Program *program) {
    CompilationStatus failure = FAILED;
    Scope global = {NULL, NULL, program, NULL, &failure}; bool ok = statements(&global, program->statements); clear(&global);
    return ok ? SUCCEEDED : failure;
}

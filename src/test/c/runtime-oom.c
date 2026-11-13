#include "../../main/c/backend/runtime/Runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
static size_t allocation, failAt = SIZE_MAX;
static bool fail(void) { return allocation++ == failAt; }
void *__wrap_malloc(size_t size) { return fail() ? NULL : __real_malloc(size); }
void *__wrap_calloc(size_t count, size_t size) { return fail() ? NULL : __real_calloc(count, size); }
void *__wrap_realloc(void *pointer, size_t size) { return fail() ? NULL : __real_realloc(pointer, size); }

/* A real runtime program, constructed without the parser so only runtime allocations fail. */
int main(void) {
    char directory[] = "/tmp/cadence-runtime-oom-XXXXXX";
    assert(mkdtemp(directory));
    assert(chdir(directory) == 0);
    Expression key = {.type = KEY_LITERAL, .key = {.tonic = "C", .mode = MODE_MAJOR}};
    Expression keyVariable = {.type = VARIABLE, .text = "tonalidad"};
    Expression tonic = {.type = DEGREE_LITERAL, .text = "I"};
    Expression dominant = {.type = DEGREE_LITERAL, .text = "V"};
    ExpressionList second = {&dominant, NULL}, first = {&tonic, &second};
    Expression degrees = {.type = LIST_LITERAL, .elements = &first};
    Expression progression = {.type = IN_EXPRESSION, .leftExpression = &degrees, .rightExpression = &keyVariable};
    Expression variable = {.type = VARIABLE, .text = "tema"};
    Expression twice = {.type = INTEGER_LITERAL, .integer = 2};
    Expression repeat = {.type = MULTIPLICATION, .leftExpression = &variable, .rightExpression = &twice};
    Statement returned = {.type = RETURN_STATEMENT, .returnStatement = &(Return){&repeat}};
    StatementList body = {&returned, NULL};
    Parameter parameter = {.name = "tema", .dataType = TYPE_PROGRESSION};
    ParameterList parameters = {&parameter, NULL};
    ProcedureDefinition procedure = {.name = "duplicar", .parameters = &parameters, .body = &body,
        .hasReturnType = true, .returnType = TYPE_PROGRESSION};
    ExpressionList argument = {&variable, NULL};
    ProcedureCall call = {.name = "duplicar", .arguments = &argument};
    Expression called = {.type = PROCEDURE_CALL, .call = &call};
    ExpressionList listItem = {&called, NULL};
    Expression list = {.type = LIST_LITERAL, .elements = &listItem};
    Expression voice = {.type = VOICE_EXPRESSION, .voice = &(Voice){&variable}};
    Expression voices = {.type = VARIABLE, .text = "voces"};
    ExpressionList voiceItem = {&voices, NULL};
    Expression voiceList = {.type = LIST_LITERAL, .elements = &voiceItem};
    Expression voiceVector = {.type = VARIABLE, .text = "vectorVoces"};
    Expression zero = {.type = INTEGER_LITERAL, .integer = 0};
    Expression indexed = {.type = INDEX_ACCESS, .leftExpression = &voiceVector, .rightExpression = &zero};
    Expression note = {.type = NOTE_LITERAL, .text = "C4"};
    Export midi = {.value = &indexed, .format = EXPORT_MIDI, .path = "voces.mid"};
    Export sheet = {.value = &variable, .format = EXPORT_SHEET, .path = "tema.ly"};
    Export extra[7];
    char paths[7][20];
    Statement statements[16] = {
        {.type = DECLARATION, .dataType = TYPE_KEY, .name = "tonalidad", .expression = &key},
        {.type = DECLARATION, .dataType = TYPE_PROGRESSION, .name = "tema", .expression = &progression},
        {.type = PROCEDURE_DEFINITION, .procedureDefinition = &procedure},
        {.type = DECLARATION, .dataType = TYPE_PROGRESSION, .isVector = true, .name = "copias", .expression = &list},
        {.type = DECLARATION, .dataType = TYPE_VOICING, .name = "voces", .expression = &voice},
        {.type = DECLARATION, .dataType = TYPE_VOICING, .isVector = true, .name = "vectorVoces", .expression = &voiceList},
        {.type = CHECK_STATEMENT, .check = &(Check){.voicing = &indexed, .rules = CHECK_VOICE_CROSSING}},
        {.type = EXPORT_STATEMENT, .export = &midi},
        {.type = EXPORT_STATEMENT, .export = &sheet}
    };
    StatementList sequence[16];
    for (size_t k = 0; k < 7; ++k) {
        snprintf(paths[k], sizeof(paths[k]), "extra%zu.mid", k);
        extra[k] = (Export){.value = &note, .format = EXPORT_MIDI, .path = paths[k]};
        statements[k + 9] = (Statement){.type = EXPORT_STATEMENT, .export = &extra[k]};
    }
    for (size_t k = 0; k < 16; ++k) sequence[k] = (StatementList){&statements[k], k + 1 < 16 ? &sequence[k + 1] : NULL};
    Program program = {sequence};
    allocation = 0;
    assert(executeProgram(&program) == SUCCEEDED);
    size_t total = allocation;
    assert(total > 50);
    assert(unlink("voces.mid") == 0 && unlink("tema.ly") == 0);
    for (size_t k = 0; k < 7; ++k) assert(unlink(paths[k]) == 0);
    for (failAt = 0; failAt < total; ++failAt) {
        allocation = 0;
        assert(executeProgram(&program) == OUT_OF_MEMORY);
        assert(access("voces.mid", F_OK) != 0 && access("tema.ly", F_OK) != 0);
        for (size_t k = 0; k < 7; ++k) assert(access(paths[k], F_OK) != 0);
    }
    assert(chdir("/") == 0 && rmdir(directory) == 0);
    return 0;
}

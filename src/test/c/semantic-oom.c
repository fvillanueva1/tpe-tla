#include "../../main/c/backend/semantic-analysis/Semantic.h"
#include <assert.h>
#include <stddef.h>
void *__wrap_malloc(size_t size) { (void)size; return NULL; }
int main(void) {
    Expression value = {.integer = 1, .type = INTEGER_LITERAL};
    Statement declaration = {.name = "numero", .expression = &value, .dataType = TYPE_INTEGER, .type = DECLARATION};
    StatementList list = {.statement = &declaration};
    Program program = {.statements = &list};
    assert(analyzeSemantics(&program) == OUT_OF_MEMORY);
    return 0;
}

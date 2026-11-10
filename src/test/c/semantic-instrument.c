#include "../../main/c/backend/semantic-analysis/Semantic.h"
#include <assert.h>
int main(void) {
    assert(semanticInstrumentProgram("piano") == 0);
    assert(semanticInstrumentProgram("violin") == 40);
    assert(semanticInstrumentProgram("gunshot") == 127);
    assert(semanticInstrumentProgram("unknown") == -1);
    assert(semanticInstrumentProgram(NULL) == -1);
    return 0;
}

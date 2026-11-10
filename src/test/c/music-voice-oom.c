#include "../../main/c/backend/semantic-analysis/MusicValidation.h"
#include <assert.h>
#include <stddef.h>
void *__real_calloc(size_t, size_t);
static int failCalloc = 1;
void *__wrap_calloc(size_t n, size_t size) { return failCalloc ? NULL : __real_calloc(n, size); }
void *__wrap_realloc(void *ptr, size_t size) { (void)ptr; (void)size; return NULL; }
int main(void) {
    unsigned short chord = 1;
    int voices[1][VOICE_COUNT] = {{42,42,42,42}};
    assert(musicRealizeSATB(&chord, 1, voices) == MUSIC_VOICE_NO_MEMORY);
    failCalloc = 0;
    assert(musicRealizeSATB(&chord, 1, voices) == MUSIC_VOICE_NO_MEMORY);
    for (int i = 0; i < VOICE_COUNT; ++i) assert(voices[0][i] == 42);
    return 0;
}

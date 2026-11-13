#include "../../main/c/backend/runtime/Value.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    Value original = {.type = TYPE_PROGRESSION, .isVector = true};
    original.vector.count = 1;
    original.vector.items = calloc(1, sizeof(Value));
    assert(original.vector.items);
    Value *item = original.vector.items;
    item->type = TYPE_PROGRESSION;
    assert(musicProgressionInit(&item->progression, 1));
    MusicNote note;
    assert(musicParseNote("C4", &note) == MUSIC_OK);
    assert(musicChordFromNotes(&note, 1, &item->progression.steps[0].chord) == MUSIC_OK);
    item->progression.steps[0].duration = MUSIC_DURATION_EIGHTH;
    Value copy;
    assert(copyValue(&original, &copy));
    assert(copy.vector.items != original.vector.items);
    assert(copy.vector.items[0].progression.steps != item->progression.steps);
    destroyValue(&original);
    destroyValue(&original);
    TextBuffer text = {0};
    assert(appendValue(&text, &copy));
    assert(!strcmp(text.text, "[[{C4} as eighth]]"));
    free(text.text);
    destroyValue(&copy);

    Value voices = {.type = TYPE_VOICING};
    assert(musicVoicingInit(&voices.voicing, 1));
    voices.voicing.chords[0][0] = 72;
    assert(copyValue(&voices, &copy));
    assert(copy.voicing.chords != voices.voicing.chords);
    assert(copy.voicing.durations != voices.voicing.durations);
    destroyValue(&voices);
    assert(copy.voicing.chords[0][0] == 72);
    destroyValue(&copy);
    destroyValue(&copy);
    return 0;
}

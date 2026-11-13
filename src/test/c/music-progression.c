#include "../../main/c/backend/music/Music.h"
#include <assert.h>
#include <string.h>

static MusicKey key(const char *tonic, MusicMode mode) {
    MusicKey result = {.mode = mode};
    assert(musicParseSpelling(tonic, &result.tonic) == MUSIC_OK);
    return result;
}

static MusicNote note(const char *lexeme) {
    MusicNote result;
    assert(musicParseNote(lexeme, &result) == MUSIC_OK);
    return result;
}

static MusicInterval interval(const char *lexeme) {
    MusicInterval result;
    assert(musicParseInterval(lexeme, &result) == MUSIC_OK);
    return result;
}

static MusicChord chord(const char *const *lexemes, size_t count) {
    MusicNote notes[MUSIC_MAX_CHORD_NOTES];
    MusicChord result;
    for (size_t i = 0; i < count; ++i) notes[i] = note(lexemes[i]);
    assert(musicChordFromNotes(notes, count, &result) == MUSIC_OK);
    return result;
}

/* The notes of a chord as text ("C4 E4 G4"). */
static const char *text(const MusicChord *c) {
    static char buffer[MUSIC_MAX_CHORD_NOTES * MUSIC_NOTE_TEXT_SIZE];
    char name[MUSIC_NOTE_TEXT_SIZE];
    buffer[0] = '\0';
    for (size_t i = 0; i < c->count; ++i) {
        musicFormatNote(c->notes[i], name);
        if (i) strcat(buffer, " ");
        strcat(buffer, name);
    }
    return buffer;
}

static bool empty(const MusicProgression *p) { return !p->steps && !p->count; }

static void progressions(void) {
    MusicProgression p, q, r;
    const int degrees[] = {0, 4, 5, 3};
    assert(musicDegreeProgression(key("C", MUSIC_MODE_MAJOR), degrees, 4, &p) == MUSIC_OK && p.count == 4);
    assert(!strcmp(text(&p.steps[1].chord), "G4 B4 D5") && !strcmp(text(&p.steps[2].chord), "A4 C5 E5"));
    assert(p.steps[3].duration == MUSIC_DEFAULT_DURATION && p.steps[2].chord.quality == MUSIC_QUALITY_MINOR);
    assert(p.steps[1].chord.size == MUSIC_TRIAD && p.steps[1].chord.degree == 4 && p.steps[1].chord.hasKey);
    const int bad[] = {0, 7};
    assert(musicDegreeProgression(key("C", MUSIC_MODE_MAJOR), bad, 2, &q) == MUSIC_OUT_OF_RANGE && empty(&q));
    assert(musicDegreeProgression(key("C", MUSIC_MODE_MAJOR), NULL, 0, &q) == MUSIC_OK && empty(&q));

    MusicChord chords[] = {chord((const char *[]){"C4", "E4", "G4"}, 3), chord((const char *[]){"F4", "A4", "C5"}, 3)};
    assert(musicChordProgression(chords, 2, &q) == MUSIC_OK && q.count == 2 && !q.steps[0].chord.hasKey);
    MusicChord none = {.count = 0};
    assert(musicChordProgression(&none, 1, &r) == MUSIC_INVALID_ARGUMENT && empty(&r));

    assert(musicConcatenate(&p, &q, &r) == MUSIC_OK && r.count == 6);
    assert(!strcmp(text(&r.steps[0].chord), "C4 E4 G4") && !strcmp(text(&r.steps[5].chord), "F4 A4 C5"));
    musicProgressionDestroy(&r);
    assert(musicAppendChord(&q, &chords[0], &r) == MUSIC_OK && r.count == 3 && !strcmp(text(&r.steps[2].chord), "C4 E4 G4"));
    musicProgressionDestroy(&r);
    assert(musicRepeat(&q, 3, &r) == MUSIC_OK && r.count == 6 && !strcmp(text(&r.steps[5].chord), "F4 A4 C5"));
    musicProgressionDestroy(&r);
    assert(musicRepeat(&q, 0, &r) == MUSIC_OK && empty(&r));
    assert(musicRepeat(&q, -1, &r) == MUSIC_OUT_OF_RANGE && empty(&r));

    musicProgressionDestroy(&p);
    musicProgressionDestroy(&q);
}

static void transformations(void) {
    MusicChord c = chord((const char *[]){"C4", "E4", "G4"}, 3), result;
    assert(musicTransposeChord(&c, interval("M2"), false, &result) == MUSIC_OK && !strcmp(text(&result), "D4 F#4 A4"));
    assert(musicTransposeChord(&c, interval("m3"), true, &result) == MUSIC_OK && !strcmp(text(&result), "A3 C#4 E4"));
    MusicChord high = chord((const char *[]){"G8", "B8"}, 2);
    result = c;
    assert(musicTransposeChord(&high, interval("M2"), false, &result) == MUSIC_OUT_OF_RANGE && result.notes[0].pitch == 60);

    /* A degree chord moves its key with it. */
    MusicChord five;
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), 4, MUSIC_TRIAD, &five) == MUSIC_OK);
    assert(musicTransposeChord(&five, interval("P5"), false, &result) == MUSIC_OK && !strcmp(text(&result), "D5 F#5 A5"));
    assert(result.key.tonic.letter == 'G' && result.degree == 4);

    MusicKey moved;
    assert(musicTransposeKey(key("Bb", MUSIC_MODE_MINOR), interval("M3"), false, &moved) == MUSIC_OK);
    assert(moved.tonic.letter == 'D' && moved.tonic.accidental == 0 && moved.mode == MUSIC_MODE_MINOR);

    MusicProgression p, q;
    const int degrees[] = {0, 4};
    assert(musicDegreeProgression(key("C", MUSIC_MODE_MAJOR), degrees, 2, &p) == MUSIC_OK);
    assert(musicTransposeProgression(&p, interval("P4"), false, &q) == MUSIC_OK && !strcmp(text(&q.steps[1].chord), "C5 E5 G5"));
    musicProgressionDestroy(&q);

    /* Inversions: the lowest note goes above the highest; root and quality stay. */
    assert(musicInvertChord(&c, 0, &result) == MUSIC_OK && !strcmp(text(&result), "C4 E4 G4") && result.root == 0);
    assert(musicInvertChord(&c, 1, &result) == MUSIC_OK && !strcmp(text(&result), "E4 G4 C5") && result.root == 2);
    assert(result.quality == MUSIC_QUALITY_MAJOR && result.size == 0);
    assert(musicInvertChord(&c, 2, &result) == MUSIC_OK && !strcmp(text(&result), "G4 C5 E5") && result.root == 1);
    assert(musicInvertChord(&c, 3, &result) == MUSIC_OK && !strcmp(text(&result), "C5 E5 G5"));
    MusicChord unordered = chord((const char *[]){"G4", "C4", "E4"}, 3);
    assert(musicInvertChord(&unordered, 1, &result) == MUSIC_OK && !strcmp(text(&result), "E4 G4 C5"));
    MusicChord wide = chord((const char *[]){"C3", "E4"}, 2);
    assert(musicInvertChord(&wide, 1, &result) == MUSIC_OK && !strcmp(text(&result), "E4 C5"));
    assert(musicInvertChord(&c, 4, &result) == MUSIC_OUT_OF_RANGE && musicInvertChord(&c, -1, &result) == MUSIC_OUT_OF_RANGE);
    MusicChord top = chord((const char *[]){"C8", "G8"}, 2);
    assert(musicInvertChord(&top, 1, &result) == MUSIC_OUT_OF_RANGE);

    /* Arpeggio: from the lowest note up. */
    assert(musicArpeggiate(&unordered, MUSIC_DURATION_EIGHTH, &q) == MUSIC_OK && q.count == 3);
    assert(q.steps[0].chord.notes[0].pitch == 60 && q.steps[2].chord.notes[0].pitch == 67);
    assert(q.steps[1].duration == MUSIC_DURATION_EIGHTH && q.steps[1].chord.count == 1);
    musicProgressionDestroy(&q);
    assert(musicArpeggiate(&c, (MusicDuration)9, &q) == MUSIC_INVALID_ARGUMENT && empty(&q));

    /* Modulation keeps the function: [I, V] in C major. */
    assert(musicModulate(&p, key("G", MUSIC_MODE_MAJOR), &q) == MUSIC_OK);
    assert(!strcmp(text(&q.steps[0].chord), "G4 B4 D5") && !strcmp(text(&q.steps[1].chord), "D5 F#5 A5"));
    musicProgressionDestroy(&q);
    MusicKey dominant, relative;
    assert(musicDominantKey(key("C", MUSIC_MODE_MAJOR), &dominant) == MUSIC_OK);
    assert(musicRelativeMinorKey(key("C", MUSIC_MODE_MAJOR), &relative) == MUSIC_OK);
    assert(musicModulate(&p, relative, &q) == MUSIC_OK);
    assert(!strcmp(text(&q.steps[0].chord), "A4 C5 E5") && q.steps[0].chord.quality == MUSIC_QUALITY_MINOR);
    assert(!strcmp(text(&q.steps[1].chord), "E5 G5 B5") && q.steps[1].chord.quality == MUSIC_QUALITY_MINOR);
    musicProgressionDestroy(&q);

    /* An explicit chord moves by the distance between tonics (nearest direction). */
    MusicProgression mixed;
    assert(musicAppendChord(&p, &c, &mixed) == MUSIC_OK);
    mixed.steps[2].duration = MUSIC_DURATION_HALF;
    assert(musicModulate(&mixed, dominant, &q) == MUSIC_OK && q.count == 3);
    assert(!strcmp(text(&q.steps[2].chord), "G3 B3 D4") && q.steps[2].duration == MUSIC_DURATION_HALF);
    musicProgressionDestroy(&q);
    assert(musicModulate(&mixed, key("A", MUSIC_MODE_MAJOR), &q) == MUSIC_OK);
    assert(!strcmp(text(&q.steps[2].chord), "A3 C#4 E4"));
    musicProgressionDestroy(&q);
    musicProgressionDestroy(&mixed);

    MusicProgression keyless;
    assert(musicChordProgression(&c, 1, &keyless) == MUSIC_OK);
    assert(musicModulate(&keyless, dominant, &q) == MUSIC_MISSING_KEY && empty(&q));
    musicProgressionDestroy(&keyless);
    musicProgressionDestroy(&p);
}

static void voicings(void) {
    MusicProgression p;
    MusicVoicing v;
    const int degrees[] = {0, 3, 4, 0};
    assert(musicDegreeProgression(key("C", MUSIC_MODE_MAJOR), degrees, 4, &p) == MUSIC_OK);
    p.steps[3].duration = MUSIC_DURATION_WHOLE;
    assert(musicVoiceProgression(&p, &v) == MUSIC_OK && v.count == 4 && v.durations[3] == MUSIC_DURATION_WHOLE);
    for (size_t i = 0; i < v.count; ++i) {
        assert(musicInTessitura(v.chords[i]) && musicWithoutVoiceCrossing(v.chords[i]));
        unsigned short mask = 0;
        for (int voice = 0; voice < VOICE_COUNT; ++voice) mask |= (unsigned short)(1u << (v.chords[i][voice] % 12));
        assert(mask == musicChordMask(&p.steps[i].chord));
    }
    size_t failed;
    assert(musicCheckVoicingValue(&v, MUSIC_RULE_VOICE_CROSSING, &failed) == MUSIC_VALID);
    musicVoicingDestroy(&v);
    musicProgressionDestroy(&p);

    /* A ninth loses its fifth: root, third, seventh and ninth. */
    MusicChord ninth;
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), 4, MUSIC_NINTH, &ninth) == MUSIC_OK);
    assert(musicChordProgression(&ninth, 1, &p) == MUSIC_OK && musicVoiceProgression(&p, &v) == MUSIC_OK);
    unsigned short mask = 0;
    for (int voice = 0; voice < VOICE_COUNT; ++voice) mask |= (unsigned short)(1u << (v.chords[0][voice] % 12));
    assert(mask == ((1u << 7) | (1u << 11) | (1u << 5) | (1u << 9)));
    musicVoicingDestroy(&v);
    musicProgressionDestroy(&p);

    /* A cluster of six classes cannot be reduced to four. */
    MusicChord cluster = chord((const char *[]){"C4", "C#4", "D4", "D#4", "E4", "F4"}, 6);
    assert(musicChordProgression(&cluster, 1, &p) == MUSIC_OK);
    assert(musicVoiceProgression(&p, &v) == MUSIC_TOO_MANY_NOTES && !v.count && !v.chords);
    musicProgressionDestroy(&p);
    assert(musicVoiceProgression(&(MusicProgression){NULL, 0}, &v) == MUSIC_OK && !v.count);

    /* Explicit voicings are taken as written. */
    MusicChord explicit[] = {chord((const char *[]){"C4", "G4", "E4", "C3"}, 4), chord((const char *[]){"C5", "G4", "E4", "C2"}, 4)};
    assert(musicExplicitVoicing(explicit, 2, &v) == MUSIC_OK && v.chords[0][VOICE_ALTO] == 67 && v.chords[1][VOICE_BASS] == 36);
    assert(musicCheckVoicingValue(&v, MUSIC_RULE_VOICE_CROSSING, &failed) == MUSIC_VOICE_CROSSING_ERROR && failed == 0);
    MusicVoicing second = {v.chords + 1, v.durations + 1, 1};
    assert(musicCheckVoicingValue(&second, 0, &failed) == MUSIC_TESSITURA_ERROR && failed == 0);
    musicVoicingDestroy(&v);
    MusicChord c = chord((const char *[]){"C4", "E4", "G4"}, 3);
    assert(musicExplicitVoicing(&c, 1, &v) == MUSIC_NOT_FOUR_VOICES && !v.count);
    assert(musicCheckVoicingValue(NULL, 0, NULL) == MUSIC_INVALID_INPUT);
}

static void intervals(void) {
    MusicInterval i;
    bool down;
    assert(musicIntervalBetween(note("C4"), note("G4"), &i, &down) == MUSIC_OK && !down && i.semitones == 7 && i.steps == 4);
    assert(musicIntervalBetween(note("C4"), note("A3"), &i, &down) == MUSIC_OK && down && i.semitones == 3 && i.steps == 2);
    assert(musicIntervalBetween(note("C4"), note("D5"), &i, &down) == MUSIC_OK && i.semitones == 14 && i.steps == 8);
    assert(musicIntervalBetween(note("B#3"), note("Cb4"), &i, &down) == MUSIC_INVALID_ARGUMENT);
}

int main(void) {
    progressions();
    transformations();
    voicings();
    intervals();
    return 0;
}

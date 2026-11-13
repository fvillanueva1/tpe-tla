#include "../../main/c/backend/output/Output.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char directory[] = "/tmp/cadence-output-XXXXXX";

static MusicNote note(const char *lexeme) {
    MusicNote result;
    assert(musicParseNote(lexeme, &result) == MUSIC_OK);
    return result;
}

static MusicKey key(const char *tonic, MusicMode mode) {
    MusicKey result = {.mode = mode};
    assert(musicParseSpelling(tonic, &result.tonic) == MUSIC_OK);
    return result;
}

static const char *path(const char *name) {
    static char buffer[4][128];
    static int next = 0;
    char *result = buffer[next++ % 4];
    snprintf(result, sizeof(buffer[0]), "%s/%s", directory, name);
    return result;
}

static size_t entries(void) {
    DIR *dir = opendir(directory);
    assert(dir);
    size_t count = 0;
    for (struct dirent *entry; (entry = readdir(dir));) if (entry->d_name[0] != '.') ++count;
    closedir(dir);
    return count;
}

static bool contains(const OutputBuffer *buffer, const char *text) {
    size_t length = strlen(text);
    for (size_t i = 0; i + length <= buffer->length; ++i) if (!memcmp(buffer->data + i, text, length)) return true;
    return false;
}

static void midi(void) {
    OutputScore score;
    OutputBuffer buffer;
    MusicChord chord;
    assert(outputScoreInit(&score, 120, 0) == OUTPUT_OK);
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), 0, MUSIC_TRIAD, &chord) == MUSIC_OK);
    assert(outputAddChord(&score, &chord) == OUTPUT_OK && outputEncodeMidi(&score, &buffer) == OUTPUT_OK);
    const unsigned char expected[] = {
        'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0, 'M','T','r','k',0,0,0,39,
        0,0xff,0x51,3,0x07,0xa1,0x20, 0,0xc0,0,
        0,0x90,60,64, 0,0x90,64,64, 0,0x90,67,64,
        0x83,0x60,0x80,60,0, 0,0x80,64,0, 0,0x80,67,0,
        0,0xff,0x2f,0
    };
    assert(buffer.length == sizeof(expected) && !memcmp(buffer.data, expected, sizeof(expected)));
    outputBufferDestroy(&buffer);
    outputScoreDestroy(&score);

    /* A voicing: one program change and one channel per voice, then a whole note later. */
    MusicVoicing voicing;
    assert(musicVoicingInit(&voicing, 1));
    const int satb[VOICE_COUNT] = {72, 67, 64, 48};
    for (int v = 0; v < VOICE_COUNT; ++v) voicing.chords[0][v] = satb[v];
    voicing.durations[0] = MUSIC_DURATION_WHOLE;
    assert(outputScoreInit(&score, 60, 52) == OUTPUT_OK && outputAddVoicing(&score, &voicing) == OUTPUT_OK);
    assert(outputEncodeMidi(&score, &buffer) == OUTPUT_OK);
    const unsigned char programs[] = {0,0xc0,52, 0,0xc1,52, 0,0xc2,52, 0,0xc3,52, 0,0x90,72,64, 0,0x91,67,64, 0,0x92,64,64, 0,0x93,48,64,
                                      0x8f,0x00,0x80,72,0};
    assert(!memcmp(buffer.data + 22, (const unsigned char[]){0,0xff,0x51,3,0x0f,0x42,0x40}, 7));
    assert(!memcmp(buffer.data + 29, programs, sizeof(programs)));
    outputBufferDestroy(&buffer);
    outputScoreDestroy(&score);
    musicVoicingDestroy(&voicing);

    /* An empty score is a valid file with only tempo and end of track. */
    assert(outputScoreInit(&score, 120, 0) == OUTPUT_OK && outputEncodeMidi(&score, &buffer) == OUTPUT_OK);
    assert(buffer.length == 22 + 7 + 4 && buffer.data[21] == 11);
    outputBufferDestroy(&buffer);
    score.tempo = 60000001;
    assert(outputEncodeMidi(&score, &buffer) == OUTPUT_INVALID_ARGUMENT && !buffer.data);
    assert(outputEncodeLilyPond(&score, &buffer) == OUTPUT_OK);
    assert(contains(&buffer, "\\tempo 4 = 60000001"));
    outputBufferDestroy(&buffer);
    score.tempo = 3;
    assert(outputEncodeMidi(&score, &buffer) == OUTPUT_INVALID_ARGUMENT && !buffer.data);
    outputScoreDestroy(&score);
}

static void lilypond(void) {
    OutputScore score;
    OutputBuffer buffer;
    MusicScale scale;
    assert(outputScoreInit(&score, 90, 0) == OUTPUT_OK);
    assert(musicKeyScale(key("G", MUSIC_MODE_MAJOR), &scale) == MUSIC_OK && outputAddScale(&score, &scale) == OUTPUT_OK);
    assert(outputEncodeLilyPond(&score, &buffer) == OUTPUT_OK);
    assert(contains(&buffer, "\\version \"2.24.0\"") && contains(&buffer, "\\tempo 4 = 90"));
    assert(contains(&buffer, "\\clef \"treble\"") && contains(&buffer, "g'4 a'4 b'4 c''4 d''4 e''4 fis''4"));
    assert(!contains(&buffer, "instrumentName"));
    outputBufferDestroy(&buffer);
    outputScoreDestroy(&score);

    /* Chords, rests, ties and spellings. */
    assert(outputScoreInit(&score, 120, 0) == OUTPUT_OK);
    MusicChord chord;
    assert(musicDegreeChord(key("F", MUSIC_MODE_MAJOR), 3, MUSIC_TRIAD, &chord) == MUSIC_OK);
    assert(outputAddChord(&score, &chord) == OUTPUT_OK);
    assert(outputScoreAdd(&score, note("Cb4"), 960, 2400, 0) == OUTPUT_OK);
    assert(outputScoreAdd(&score, note("E#3"), 3360, 120, 0) == OUTPUT_OK);
    assert(outputEncodeLilyPond(&score, &buffer) == OUTPUT_OK);
    assert(contains(&buffer, "<bes' d'' f''>4 r4 ces'1~ ces'4 eis16"));
    outputBufferDestroy(&buffer);
    assert(outputScoreAdd(&score, note("C4"), 3400, 120, 0) == OUTPUT_OK);
    assert(outputEncodeLilyPond(&score, &buffer) == OUTPUT_INVALID_ARGUMENT && !buffer.data);
    outputScoreDestroy(&score);

    /* A voicing: four named staves with their clefs. */
    MusicVoicing voicing;
    assert(musicVoicingInit(&voicing, 1));
    const int satb[VOICE_COUNT] = {72, 67, 64, 48};
    for (int v = 0; v < VOICE_COUNT; ++v) voicing.chords[0][v] = satb[v];
    voicing.durations[0] = MUSIC_DURATION_HALF;
    assert(outputScoreInit(&score, 120, 0) == OUTPUT_OK && outputAddVoicing(&score, &voicing) == OUTPUT_OK);
    assert(outputEncodeLilyPond(&score, &buffer) == OUTPUT_OK);
    assert(contains(&buffer, "instrumentName = \"Soprano\"") && contains(&buffer, "instrumentName = \"Bass\""));
    assert(contains(&buffer, "\\clef \"treble_8\"") && contains(&buffer, "\\clef \"bass\""));
    assert(contains(&buffer, "c''2") && contains(&buffer, "c2"));
    outputBufferDestroy(&buffer);
    outputScoreDestroy(&score);
    musicVoicingDestroy(&voicing);

    assert(outputScoreInit(&score, 120, 0) == OUTPUT_OK && outputEncodeLilyPond(&score, &buffer) == OUTPUT_OK);
    assert(contains(&buffer, "\\new Staff {") && contains(&buffer, "\\layout { }"));
    outputBufferDestroy(&buffer);
    outputScoreDestroy(&score);
}

static void files(void) {
    assert(mkdtemp(directory));
    OutputScore score;
    MusicChord chord;
    OutputBuffer expected;
    assert(outputScoreInit(&score, 120, 0) == OUTPUT_OK);
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), 0, MUSIC_TRIAD, &chord) == MUSIC_OK && outputAddChord(&score, &chord) == OUTPUT_OK);
    assert(outputEncodeMidi(&score, &expected) == OUTPUT_OK);

    /* A written file has the encoded contents and the usual permissions. */
    assert(outputWriteMidi(&score, path("a.mid")) == OUTPUT_OK && entries() == 1);
    FILE *file = fopen(path("a.mid"), "rb");
    unsigned char data[256];
    size_t length = fread(data, 1, sizeof(data), file);
    fclose(file);
    assert(length == expected.length && !memcmp(data, expected.data, length));
    mode_t mask = umask(0);
    umask(mask);
    struct stat information;
    assert(stat(path("a.mid"), &information) == 0 && (information.st_mode & 0777) == (0666 & ~mask));

    /* Never overwrites. */
    OutputScore other;
    assert(outputScoreInit(&other, 60, 0) == OUTPUT_OK);
    assert(outputWriteMidi(&other, path("a.mid")) == OUTPUT_FILE_EXISTS && entries() == 1);
    file = fopen(path("a.mid"), "rb");
    assert(fread(data, 1, sizeof(data), file) == expected.length && !memcmp(data, expected.data, expected.length));
    fclose(file);

    /* All or nothing: a file that cannot be written leaves none of the others nor temporaries. */
    size_t failed = 99;
    OutputFile batch[] = {{&score, OUTPUT_FORMAT_MIDI, path("b.mid")}, {&score, OUTPUT_FORMAT_LILYPOND, path("missing/c.ly")}};
    assert(outputWriteFiles(batch, 2, &failed) == OUTPUT_WRITE_ERROR && failed == 1 && entries() == 1);
    OutputFile repeated[] = {{&score, OUTPUT_FORMAT_MIDI, path("d.mid")}, {&score, OUTPUT_FORMAT_MIDI, path("d.mid")}};
    assert(outputWriteFiles(repeated, 2, &failed) == OUTPUT_FILE_EXISTS && failed == 1 && entries() == 1);
    OutputFile existing[] = {{&score, OUTPUT_FORMAT_MIDI, path("g.mid")}, {&score, OUTPUT_FORMAT_MIDI, path("a.mid")}};
    assert(outputWriteFiles(existing, 2, &failed) == OUTPUT_FILE_EXISTS && failed == 1 && entries() == 1);
    score.tempo = 2;
    OutputFile invalid[] = {{&other, OUTPUT_FORMAT_MIDI, path("h.mid")}, {&score, OUTPUT_FORMAT_MIDI, path("i.mid")}};
    assert(outputWriteFiles(invalid, 2, &failed) == OUTPUT_INVALID_ARGUMENT && failed == 1 && entries() == 1);
    score.tempo = 120;

    OutputFile badFormat[] = {{&score, (OutputFormat)99, path("invalid.mid")}};
    assert(outputWriteFiles(badFormat, 1, &failed) == OUTPUT_INVALID_ARGUMENT && failed == 0 && entries() == 1);

    OutputFile both[] = {{&score, OUTPUT_FORMAT_MIDI, path("e.mid")}, {&score, OUTPUT_FORMAT_LILYPOND, path("f.ly")}};
    assert(outputWriteFiles(both, 2, &failed) == OUTPUT_OK && entries() == 3);
    assert(outputWriteLilyPond(&score, path("f.ly")) == OUTPUT_FILE_EXISTS);
    assert(outputWriteFiles(NULL, 0, NULL) == OUTPUT_OK && outputWriteMidi(&score, "") == OUTPUT_INVALID_ARGUMENT);

    outputBufferDestroy(&expected);
    outputScoreDestroy(&score);
    outputScoreDestroy(&other);
    const char *names[] = {"a.mid", "e.mid", "f.ly"};
    for (size_t i = 0; i < 3; ++i) unlink(path(names[i]));
    rmdir(directory);
}

int main(void) {
    midi();
    lilypond();
    files();
    return 0;
}

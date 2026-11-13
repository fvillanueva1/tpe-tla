#! /bin/bash

set -u

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
RED='\033[0;31m'
OFF='\033[0m'
STATUS=0

# A sanitizer report (memory error or leak) must not look like an expected rejection (status 1).
export ASAN_OPTIONS="exitcode=86${ASAN_OPTIONS:+:$ASAN_OPTIONS}"

echo "Compiler should accept (and run, when an expected output exists)..."
echo ""

ROOT_PATH="$PWD"
MUSIC_SOURCES="src/main/c/backend/music/Music.c src/main/c/backend/music/Note.c src/main/c/backend/music/Key.c src/main/c/backend/music/Chord.c src/main/c/backend/music/Property.c src/main/c/backend/music/Progression.c src/main/c/backend/music/Transform.c src/main/c/backend/music/Voicing.c src/main/c/backend/music/MusicValidation.c src/main/c/backend/music/VoiceLeading.c"
OUTPUT_SOURCES="src/main/c/backend/output/Buffer.c src/main/c/backend/output/File.c src/main/c/backend/output/LilyPond.c src/main/c/backend/output/Midi.c src/main/c/backend/output/Score.c"
TEST_DIRECTORY=$(mktemp -d)
trap 'rm -rf "$TEST_DIRECTORY"' EXIT
for test in $(ls src/test/c/accept/); do
    TEST_WORK="$TEST_DIRECTORY/$test"
    mkdir "$TEST_WORK"
    EXPECTED_OUTPUT="$ROOT_PATH/src/test/c/output/${test%.cad}.out"
    if [ -f "$EXPECTED_OUTPUT" ]; then
        (cd "$TEST_WORK" && LOGGING_LEVEL=ERROR "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/$test") > "$TEST_DIRECTORY/$test.out" 2>/dev/null
        RESULT="$?"
        if [ "$RESULT" == "0" ] && ! cmp -s "$TEST_DIRECTORY/$test.out" "$EXPECTED_OUTPUT"; then
            echo -e "    $test, ${RED}but its output differs${OFF}"
            STATUS=1
            continue
        fi
        if [ -n "$(ls -A "$TEST_WORK")" ]; then
            echo -e "    $test, ${RED}but it writes unexpected files${OFF}"
            STATUS=1
            continue
        fi
    else
        (cd "$TEST_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" --check < "$ROOT_PATH/src/test/c/accept/$test") >/dev/null 2>&1
        RESULT="$?"
    fi
	if [ "$RESULT" == "0" ]; then
		echo -e "    $test, ${GREEN}and it does${OFF} (status $RESULT)"
	else
		STATUS=1
		echo -e "    $test, ${RED}but it rejects${OFF} (status $RESULT)"
	fi
done
echo ""

echo "Compiler should reject..."
echo ""

for test in $(ls src/test/c/reject/); do
    TEST_WORK="$TEST_DIRECTORY/reject-$test"
    mkdir "$TEST_WORK"
	OUTPUT=$(cd "$TEST_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/reject/$test" 2>&1)
	RESULT="$?"
	if [[ "$OUTPUT" == *"Sanitizer"* ]]; then
		echo -e "    $test, ${RED}but the sanitizer reports an error${OFF}"
		STATUS=1
		continue
	fi
	case "$test" in
		54-*|55-*|56-*|57-*|58-*|59-*|60-*|61-*|62-*|63-*|log-undeclared-*|scope-outside*|type-*|procedure-*|return-outside*|export-invalid-*)
			if [[ "$OUTPUT" != *"Semantic error:"* || "$OUTPUT" == *"Code generation is not implemented"* ]]; then
				echo -e "    $test, ${RED}missing semantic diagnostic or generation was invoked${OFF}"
				STATUS=1
				continue
			fi ;;
		runtime-*)
			if [[ "$OUTPUT" != *"Runtime error:"* || "$OUTPUT" == *"Semantic error:"* ]]; then
				echo -e "    $test, ${RED}missing runtime diagnostic${OFF}"
				STATUS=1
				continue
			fi ;;
	esac
    case "$test" in
        54-*) EXPECTED="operator + does not accept chord and integer" ;;
        type-declaration.cad) EXPECTED="expected integer, got boolean" ;;
        56-*) EXPECTED="parallel fifths at chord 2" ;;
        57-*) EXPECTED="parallel octaves at chord 2" ;;
        58-*) EXPECTED="voice crossing at chord 1" ;;
        59-*) EXPECTED="tessitura at chord 1" ;;
        runtime-division-by-zero.cad) EXPECTED="Runtime error: division by zero." ;;
        runtime-overflow.cad) EXPECTED="integer overflow in 2147483647 + 1" ;;
        runtime-overflow-subtraction.cad) EXPECTED="integer overflow in -2147483647 - 2" ;;
        runtime-overflow-multiplication.cad) EXPECTED="integer overflow in 65536 * 65536" ;;
        runtime-division-overflow.cad) EXPECTED="integer overflow in -2147483648 / -1" ;;
        runtime-index-range.cad) EXPECTED="index 3 is out of range for a vector of length 1" ;;
        runtime-negative-index.cad) EXPECTED="index -1 is out of range for a vector of length 1" ;;
        runtime-call-depth.cad)
            if [[ "$OUTPUT" == *"the program nests too deeply for the available stack"* ]]; then
                EXPECTED="the program nests too deeply for the available stack"
            else
                EXPECTED="exceeds the maximum call depth of 1000"
            fi ;;
        runtime-stack-depth.cad) EXPECTED="the program nests too deeply for the available stack" ;;
        *) EXPECTED="" ;;
    esac
    if [[ -n "$EXPECTED" && "$OUTPUT" != *"$EXPECTED"* ]]; then
        echo -e "    $test, ${RED}incorrect rule or chord diagnostic${OFF}"
        STATUS=1
        continue
    fi
    if [ -n "$(ls -A "$TEST_WORK")" ]; then
        echo -e "    $test, ${RED}but it leaves files behind${OFF}"
        STATUS=1
        continue
    fi
	if [ "$RESULT" == "1" ]; then
		echo -e "    $test, ${GREEN}and it does${OFF} (status $RESULT)"
	else
		STATUS=1
		echo -e "    $test, ${RED}but it accepts${OFF} (status $RESULT)"
	fi
done
echo ""

echo "Checking expected outputs..."
for expected in src/test/c/output/*.out; do
    name=$(basename "$expected" .out)
    if [ ! -f "src/test/c/accept/$name.cad" ]; then
        echo -e "    $name.out, ${RED}has no accept case${OFF}"
        STATUS=1
    fi
done

echo "Checking export rollback after runtime errors..."
RUNTIME_WORK="$TEST_DIRECTORY/atomic"
mkdir "$RUNTIME_WORK"
ATOMIC_CASES=(
    'integer[] ceros = [0]; integer resultado = 1 / ceros[0];'
    'if (true) { integer[] ceros = [0]; integer resultado = 1 / ceros[0]; }'
    'define fallar() -> integer { integer[] ceros = [0]; return 1 / ceros[0]; } integer resultado = fallar();'
)
for program in "${ATOMIC_CASES[@]}"; do
    printf 'log "antes";\nexport C4 as midi to "antes.mid";\n%s\n' "$program" > "$TEST_DIRECTORY/atomic.cad"
    ATOMIC_STDOUT=$(cd "$RUNTIME_WORK" && LOGGING_LEVEL=ERROR "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$TEST_DIRECTORY/atomic.cad" 2>"$TEST_DIRECTORY/atomic.err")
    ATOMIC_RESULT=$?
    if [[ "$ATOMIC_RESULT" != 1 || "$ATOMIC_STDOUT" != antes || "$(cat "$TEST_DIRECTORY/atomic.err")" != *"Runtime error: division by zero."* || -n "$(ls -A "$RUNTIME_WORK")" ]]; then
        echo -e "    export rollback ($program), ${RED}failed${OFF}"
        STATUS=1
    fi
done

echo "Checking log write failure..."
LOG_FAILURE=$(cd "$RUNTIME_WORK" && LOGGING_LEVEL=ERROR "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/log-text.cad" 2>&1 >/dev/full)
LOG_FAILURE_RESULT=$?
if [[ "$LOG_FAILURE_RESULT" != 1 || "$LOG_FAILURE" != *"Runtime error: cannot write the log output."* ]]; then
    echo -e "    log write failure, ${RED}not reported${OFF}"
    STATUS=1
fi

echo "Checking scale MIDI integration..."
SCALE_WORK="$TEST_DIRECTORY/scale-integration"
mkdir "$SCALE_WORK"
if ! (cd "$SCALE_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/30-export-scale.cad" >/dev/null 2>&1) ||
   ! cc -std=gnu99 src/test/c/scale-midi.c -o "$TEST_DIRECTORY/scale-midi" ||
   ! "$TEST_DIRECTORY/scale-midi" "$SCALE_WORK/escala.mid" C ||
   ! (cd "$SCALE_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/scale-midi-g-major.cad" >/dev/null 2>&1) ||
   ! "$TEST_DIRECTORY/scale-midi" "$SCALE_WORK/sol.mid" G ||
   ! (cd "$SCALE_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/scale-midi-variable.cad" >/dev/null 2>&1) ||
   ! "$TEST_DIRECTORY/scale-midi" "$SCALE_WORK/declarada.mid" C; then
    echo -e "    scale MIDI, ${RED}failed${OFF}"
    STATUS=1
else
    cp "$SCALE_WORK/escala.mid" "$SCALE_WORK/original.mid"
    (cd "$SCALE_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/30-export-scale.cad" >/dev/null 2>&1)
    RESULT=$?
    if [ "$RESULT" != 1 ] || ! cmp -s "$SCALE_WORK/escala.mid" "$SCALE_WORK/original.mid"; then
        echo -e "    existing MIDI protection, ${RED}failed${OFF}"
        STATUS=1
    fi
    printf '%s\n' 'key tonalidad = C major;' 'export scale of tonalidad as midi to "missing/output.mid";' > "$SCALE_WORK/write-failure.cad"
    (cd "$SCALE_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" < write-failure.cad >/dev/null 2>&1)
    if [ "$?" != 1 ]; then
        echo -e "    MIDI write failure, ${RED}not reported${OFF}"
        STATUS=1
    fi
fi

echo "Checking instrument mapping..."
INSTRUMENT_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/semantic-instrument.c src/main/c/backend/semantic-analysis/Semantic.c $MUSIC_SOURCES -o "$INSTRUMENT_TEST" || ! "$INSTRUMENT_TEST"; then
    echo -e "    instrument mapping, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$INSTRUMENT_TEST"
echo "Checking allocation failure..."
OOM_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/semantic-oom.c src/main/c/backend/semantic-analysis/Semantic.c $MUSIC_SOURCES -Wl,--wrap=malloc -o "$OOM_TEST" || ! "$OOM_TEST"; then
    echo -e "    allocation failure, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$OOM_TEST"
echo "Checking music validation..."
MUSIC_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/music-validation.c $MUSIC_SOURCES -o "$MUSIC_TEST" || ! "$MUSIC_TEST"; then
    echo -e "    music validation, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$MUSIC_TEST"
echo "Checking voice allocation failure..."
VOICE_OOM_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/music-voice-oom.c $MUSIC_SOURCES -Wl,--wrap=calloc -Wl,--wrap=realloc -o "$VOICE_OOM_TEST" || ! "$VOICE_OOM_TEST"; then
    echo -e "    voice allocation failure, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$VOICE_OOM_TEST"
echo "Checking runtime values..."
VALUE_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/runtime-value.c src/main/c/backend/runtime/Value.c $MUSIC_SOURCES -o "$VALUE_TEST" || ! "$VALUE_TEST"; then
    echo -e "    runtime values, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$VALUE_TEST"
echo "Checking music model..."
MODEL_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/music-model.c $MUSIC_SOURCES -Wl,--wrap=calloc -o "$MODEL_TEST" || ! "$MODEL_TEST"; then
    echo -e "    music model, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$MODEL_TEST"
echo "Checking notes and intervals..."
NOTE_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/music-note.c $MUSIC_SOURCES -o "$NOTE_TEST" || ! "$NOTE_TEST"; then
    echo -e "    notes and intervals, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$NOTE_TEST"
echo "Checking keys and scales..."
KEY_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/music-key.c $MUSIC_SOURCES -o "$KEY_TEST" || ! "$KEY_TEST"; then
    echo -e "    keys and scales, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$KEY_TEST"
echo "Checking chords..."
CHORD_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/music-chord.c $MUSIC_SOURCES -o "$CHORD_TEST" || ! "$CHORD_TEST"; then
    echo -e "    chords, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$CHORD_TEST"
echo "Checking properties and membership..."
PROPERTY_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/music-property.c $MUSIC_SOURCES -o "$PROPERTY_TEST" || ! "$PROPERTY_TEST"; then
    echo -e "    properties and membership, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$PROPERTY_TEST"
echo "Checking output score..."
SCORE_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/output-score.c $OUTPUT_SOURCES $MUSIC_SOURCES -Wl,--wrap=realloc -o "$SCORE_TEST" || ! "$SCORE_TEST"; then
    echo -e "    output score, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$SCORE_TEST"
echo "Checking progressions, transformations and voicings..."
PROGRESSION_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/music-progression.c $MUSIC_SOURCES -Wl,--wrap=calloc -Wl,--wrap=malloc -o "$PROGRESSION_TEST" || ! "$PROGRESSION_TEST"; then
    echo -e "    progressions, transformations and voicings, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$PROGRESSION_TEST"
echo "Checking MIDI, LilyPond and file writing..."
FILE_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/output-file.c $OUTPUT_SOURCES $MUSIC_SOURCES -Wl,--wrap=link -o "$FILE_TEST" || ! "$FILE_TEST"; then
    echo -e "    MIDI, LilyPond and file writing, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$FILE_TEST"
echo "All done."
exit $STATUS

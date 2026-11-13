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

ACCEPT_TOTAL=0
ACCEPT_PASSED=0
REJECT_TOTAL=0
REJECT_PASSED=0

echo "Compiler should accept and run..."
echo ""

ROOT_PATH="$PWD"
MUSIC_SOURCES="src/main/c/backend/music/Music.c src/main/c/backend/music/Note.c src/main/c/backend/music/Key.c src/main/c/backend/music/Chord.c src/main/c/backend/music/Property.c src/main/c/backend/music/Progression.c src/main/c/backend/music/Transform.c src/main/c/backend/music/Voicing.c src/main/c/backend/music/MusicValidation.c src/main/c/backend/music/VoiceLeading.c"
OUTPUT_SOURCES="src/main/c/backend/output/Buffer.c src/main/c/backend/output/File.c src/main/c/backend/output/LilyPond.c src/main/c/backend/output/Midi.c src/main/c/backend/output/Score.c"
TEST_DIRECTORY=$(mktemp -d)
trap 'rm -rf "$TEST_DIRECTORY"' EXIT
for source in src/test/c/accept/*.cad; do
    test=${source##*/}
    ACCEPT_TOTAL=$((ACCEPT_TOTAL + 1))
    TEST_WORK="$TEST_DIRECTORY/$test"
    mkdir "$TEST_WORK"
    EXPECTED_OUTPUT="$ROOT_PATH/src/test/c/output/${test%.cad}.out"
    EXPECTED_FILES="$ROOT_PATH/src/test/c/artifacts/${test%.cad}"
    (cd "$TEST_WORK" && LOGGING_LEVEL=ERROR "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/$source") > "$TEST_DIRECTORY/$test.out" 2> "$TEST_DIRECTORY/$test.err"
    RESULT=$?
    if [ "$RESULT" != 0 ] || [[ "$(cat "$TEST_DIRECTORY/$test.err")" == *"Sanitizer"* ]]; then
        echo -e "    $test, ${RED}but it rejects or the sanitizer reports an error${OFF} (status $RESULT)"
        cat "$TEST_DIRECTORY/$test.err"
        STATUS=1
        continue
    fi
    if [ ! -f "$EXPECTED_OUTPUT" ]; then EXPECTED_OUTPUT=/dev/null; fi
    if ! cmp -s "$TEST_DIRECTORY/$test.out" "$EXPECTED_OUTPUT"; then
        echo -e "    $test, ${RED}but its output differs${OFF}"
        diff -u "$EXPECTED_OUTPUT" "$TEST_DIRECTORY/$test.out"
        STATUS=1
        continue
    fi
    if [ -d "$EXPECTED_FILES" ]; then
        if ! diff -r "$EXPECTED_FILES" "$TEST_WORK"; then
            echo -e "    $test, ${RED}but its files differ${OFF}"
            STATUS=1
            continue
        fi
    elif [ -n "$(ls -A "$TEST_WORK")" ]; then
        echo -e "    $test, ${RED}but it writes unexpected files${OFF}"
        STATUS=1
        continue
    fi
    ACCEPT_PASSED=$((ACCEPT_PASSED + 1))
    echo -e "    $test, ${GREEN}and it does${OFF} (status $RESULT)"
done
echo ""

echo "Compiler should reject..."
echo ""

for source in src/test/c/reject/*.cad; do
    test=${source##*/}
    REJECT_TOTAL=$((REJECT_TOTAL + 1))
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
        runtime-scale-index.cad) EXPECTED="index 7 is out of range for a scale of length 7" ;;
        runtime-scale-degree.cad) EXPECTED="chord degree 8 must be between 1 and 7" ;;
        runtime-progression-index.cad) EXPECTED="index 2 is out of range for a progression of length 2" ;;
        runtime-strict-*.cad) EXPECTED="chord contains a note outside the strict key" ;;
        runtime-invert-range.cad) EXPECTED="inversion is out of range" ;;
        runtime-repeat-negative.cad) EXPECTED="repetition count cannot be negative" ;;
        runtime-*transpose-range.cad) EXPECTED="transposition is out of range" ;;
        runtime-check-computed.cad) EXPECTED="parallel fifths at chord 2" ;;
        runtime-check-octaves.cad) EXPECTED="parallel octaves at chord 2" ;;
        runtime-check-crossing.cad) EXPECTED="voice crossing at chord 1" ;;
        runtime-voicing-tessitura.cad) EXPECTED="tessitura at chord 1" ;;
        runtime-voicing-count.cad) EXPECTED="explicit voicing chords need four notes" ;;
        runtime-voice-impossible.cad) EXPECTED="voice has too many notes" ;;
        runtime-export-write.cad) EXPECTED="cannot write 'missing/salida.ly'" ;;
        runtime-export-duplicate.cad) EXPECTED="destination already exists or is repeated" ;;
        runtime-tempo*.cad|runtime-sheet-tempo.cad) EXPECTED="outside the export format's range" ;;
        runtime-modulate-key.cad) EXPECTED="cannot modulate a progression without a known key" ;;
        runtime-operand-cleanup.cad) EXPECTED="division by zero" ;;
        runtime-division-by-zero.cad) EXPECTED="Runtime error: division by zero." ;;
        runtime-overflow.cad) EXPECTED="integer overflow in 2147483647 + 1" ;;
        runtime-overflow-subtraction.cad) EXPECTED="integer overflow in -2147483647 - 2" ;;
        runtime-overflow-multiplication.cad) EXPECTED="integer overflow in 65536 * 65536" ;;
        runtime-division-overflow.cad) EXPECTED="integer overflow in -2147483648 / -1" ;;
        runtime-index-range.cad) EXPECTED="index 3 is out of range for a vector of length 1" ;;
        runtime-negative-index.cad) EXPECTED="index -1 is out of range for a vector of length 1" ;;
        runtime-call-depth.cad) EXPECTED="exceeds the maximum call depth of 1000" ;;
        runtime-stack-depth.cad) EXPECTED="the program nests too deeply for the available stack" ;;
        *) EXPECTED="" ;;
    esac
    if [[ "$test" == runtime-strict-discarded-return.cad && "$OUTPUT" == *accepted* ]]; then
        echo -e "    $test, ${RED}continues after the invalid discarded return${OFF}"
        STATUS=1
        continue
    fi
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
        REJECT_PASSED=$((REJECT_PASSED + 1))
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

for artifacts in src/test/c/artifacts/*; do
    name=${artifacts##*/}
    if [ ! -f "src/test/c/accept/$name.cad" ]; then
        echo -e "    $name artifacts, ${RED}has no accept case${OFF}"
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

echo "Checking frontend-only execution..."
CHECK_WORK="$TEST_DIRECTORY/check-only"
mkdir "$CHECK_WORK"
(cd "$CHECK_WORK" && LOGGING_LEVEL=ERROR "$ROOT_PATH/.build/Flex-Bison-Compiler" --check < "$ROOT_PATH/src/test/c/accept/runtime-export-values.cad") > "$TEST_DIRECTORY/check-only.out" 2> "$TEST_DIRECTORY/check-only.err"
if [ "$?" != 0 ] || [ -s "$TEST_DIRECTORY/check-only.out" ] || [ -s "$TEST_DIRECTORY/check-only.err" ] || [ -n "$(ls -A "$CHECK_WORK")" ]; then
    echo -e "    frontend-only execution, ${RED}failed${OFF}"
    STATUS=1
fi

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
echo "Checking runtime allocation failure..."
RUNTIME_OOM_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/runtime-oom.c src/main/c/backend/runtime/Runtime.c src/main/c/backend/runtime/Value.c src/main/c/backend/semantic-analysis/Semantic.c $MUSIC_SOURCES $OUTPUT_SOURCES -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -o "$RUNTIME_OOM_TEST" || ! "$RUNTIME_OOM_TEST" 2>"$TEST_DIRECTORY/runtime-oom.err"; then
    cat "$TEST_DIRECTORY/runtime-oom.err"
    echo -e "    runtime allocation failure, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$RUNTIME_OOM_TEST"
echo "Checking runtime cleanup failure..."
RUNTIME_CLEANUP_TEST=$(mktemp)
if ! cc -std=gnu99 -fsanitize=address src/test/c/runtime-output-cleanup.c src/main/c/backend/runtime/Runtime.c src/main/c/backend/runtime/Value.c src/main/c/backend/semantic-analysis/Semantic.c $MUSIC_SOURCES $OUTPUT_SOURCES -Wl,--wrap=unlink -o "$RUNTIME_CLEANUP_TEST" || ! "$RUNTIME_CLEANUP_TEST"; then
    echo -e "    runtime cleanup failure, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$RUNTIME_CLEANUP_TEST"
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
if ! cc -std=gnu99 -fsanitize=address src/test/c/output-file.c $OUTPUT_SOURCES $MUSIC_SOURCES -Wl,--wrap=link -Wl,--wrap=unlink -o "$FILE_TEST" || ! "$FILE_TEST"; then
    echo -e "    MIDI, LilyPond and file writing, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$FILE_TEST"
echo "Suite: $ACCEPT_PASSED/$ACCEPT_TOTAL accept, $REJECT_PASSED/$REJECT_TOTAL reject; status $STATUS."
echo "All done."
exit $STATUS

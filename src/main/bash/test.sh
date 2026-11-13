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
		54-*|55-*|56-*|57-*|60-*|61-*|62-*|63-*|log-undeclared-*|scope-outside*|type-*|procedure-*|return-outside*|export-invalid-*)
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
        runtime-division-by-zero.cad) EXPECTED="Runtime error: division by zero." ;;
        runtime-overflow.cad) EXPECTED="integer overflow in 2147483647 + 1" ;;
        runtime-division-overflow.cad) EXPECTED="integer overflow in -2147483648 / -1" ;;
        runtime-index-range.cad) EXPECTED="index 3 is out of range for a vector of length 1" ;;
        runtime-negative-index.cad) EXPECTED="index -1 is out of range for a vector of length 1" ;;
        runtime-call-depth.cad) EXPECTED="exceeds the maximum call depth of 1000" ;;
        runtime-stack-depth.cad) EXPECTED="the program nests too deeply for the available stack" ;;
        *) EXPECTED="" ;;
    esac
    if [[ -n "$EXPECTED" && "$OUTPUT" != *"$EXPECTED"* ]]; then
        echo -e "    $test, ${RED}incorrect rule or chord diagnostic${OFF}"
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

echo "Checking unsupported generation..."
UNSUPPORTED_WORK="$TEST_DIRECTORY/unsupported"
mkdir "$UNSUPPORTED_WORK"
UNSUPPORTED_OUTPUT=$(cd "$UNSUPPORTED_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/48-export-midi.cad" 2>&1)
UNSUPPORTED_RESULT=$?
if [[ "$UNSUPPORTED_RESULT" != 1 || "$UNSUPPORTED_OUTPUT" != *"Code generation is not implemented"* || -e "$UNSUPPORTED_WORK/tema.mid" ]]; then
    echo -e "    unsupported generation, ${RED}failed${OFF}"
    STATUS=1
fi

echo "Checking log write failure..."
LOG_FAILURE=$(cd "$UNSUPPORTED_WORK" && LOGGING_LEVEL=ERROR "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/log-text.cad" 2>&1 >/dev/full)
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
if ! cc -std=gnu99 src/test/c/semantic-instrument.c src/main/c/backend/semantic-analysis/Semantic.c src/main/c/backend/semantic-analysis/MusicValidation.c src/main/c/backend/semantic-analysis/VoiceLeading.c -o "$INSTRUMENT_TEST" || ! "$INSTRUMENT_TEST"; then
    echo -e "    instrument mapping, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$INSTRUMENT_TEST"
echo "Checking allocation failure..."
OOM_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/semantic-oom.c src/main/c/backend/semantic-analysis/Semantic.c src/main/c/backend/semantic-analysis/MusicValidation.c src/main/c/backend/semantic-analysis/VoiceLeading.c -Wl,--wrap=malloc -o "$OOM_TEST" || ! "$OOM_TEST"; then
    echo -e "    allocation failure, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$OOM_TEST"
echo "Checking music validation..."
MUSIC_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/music-validation.c src/main/c/backend/semantic-analysis/MusicValidation.c src/main/c/backend/semantic-analysis/VoiceLeading.c -o "$MUSIC_TEST" || ! "$MUSIC_TEST"; then
    echo -e "    music validation, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$MUSIC_TEST"
echo "Checking voice allocation failure..."
VOICE_OOM_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/music-voice-oom.c src/main/c/backend/semantic-analysis/MusicValidation.c src/main/c/backend/semantic-analysis/VoiceLeading.c -Wl,--wrap=calloc -Wl,--wrap=realloc -o "$VOICE_OOM_TEST" || ! "$VOICE_OOM_TEST"; then
    echo -e "    voice allocation failure, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$VOICE_OOM_TEST"
echo "All done."
exit $STATUS

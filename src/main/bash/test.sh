#! /bin/bash

set -u

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
RED='\033[0;31m'
OFF='\033[0m'
STATUS=0

echo "Compiler should accept..."
echo ""

ROOT_PATH="$PWD"
TEST_DIRECTORY=$(mktemp -d)
trap 'rm -rf "$TEST_DIRECTORY"' EXIT
for test in $(ls src/test/c/accept/); do
    TEST_WORK="$TEST_DIRECTORY/$test"
    mkdir "$TEST_WORK"
    (cd "$TEST_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/$test") >/dev/null 2>&1
	RESULT="$?"
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
	OUTPUT=$(".build/Flex-Bison-Compiler" < "src/test/c/reject/$test" 2>&1)
	RESULT="$?"
	case "$test" in
		54-*|55-*|60-*|61-*|62-*|63-*|log-undeclared-*|scope-outside*|type-*|procedure-*|return-outside*|export-invalid-*)
			if [[ "$OUTPUT" != *"Semantic error:"* || "$OUTPUT" == *"Code generation is not implemented"* ]]; then
				echo -e "    $test, ${RED}missing semantic diagnostic or generation was invoked${OFF}"
				STATUS=1
				continue
			fi ;;
	esac
	if [ "$RESULT" == "1" ]; then
		echo -e "    $test, ${GREEN}and it does${OFF} (status $RESULT)"
	else
		STATUS=1
		echo -e "    $test, ${RED}but it accepts${OFF} (status $RESULT)"
	fi
done
echo ""

echo "Checking scale MIDI integration..."
SCALE_WORK="$TEST_DIRECTORY/scale-integration"
mkdir "$SCALE_WORK"
if ! (cd "$SCALE_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/30-export-scale.cad" >/dev/null 2>&1) ||
   ! cc -std=gnu99 src/test/c/scale-midi.c -o "$TEST_DIRECTORY/scale-midi" ||
   ! "$TEST_DIRECTORY/scale-midi" "$SCALE_WORK/escala.mid" C ||
   ! (cd "$SCALE_WORK" && "$ROOT_PATH/.build/Flex-Bison-Compiler" < "$ROOT_PATH/src/test/c/accept/scale-midi-g-major.cad" >/dev/null 2>&1) ||
   ! "$TEST_DIRECTORY/scale-midi" "$SCALE_WORK/sol.mid" G; then
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
if ! cc -std=gnu99 src/test/c/semantic-instrument.c src/main/c/backend/semantic-analysis/Semantic.c src/main/c/backend/semantic-analysis/MusicValidation.c -o "$INSTRUMENT_TEST" || ! "$INSTRUMENT_TEST"; then
    echo -e "    instrument mapping, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$INSTRUMENT_TEST"
echo "Checking allocation failure..."
OOM_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/semantic-oom.c src/main/c/backend/semantic-analysis/Semantic.c src/main/c/backend/semantic-analysis/MusicValidation.c -Wl,--wrap=malloc -o "$OOM_TEST" || ! "$OOM_TEST"; then
    echo -e "    allocation failure, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$OOM_TEST"
echo "Checking music validation..."
MUSIC_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/music-validation.c src/main/c/backend/semantic-analysis/MusicValidation.c -o "$MUSIC_TEST" || ! "$MUSIC_TEST"; then
    echo -e "    music validation, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$MUSIC_TEST"
echo "All done."
exit $STATUS

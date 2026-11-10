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

for test in $(ls src/test/c/accept/); do
	cat "src/test/c/accept/$test" | ".build/Flex-Bison-Compiler" >/dev/null 2>&1
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
		54-*|60-*|61-*|62-*|63-*|log-undeclared-*|scope-outside*|type-*|procedure-*|return-outside*|export-invalid-*)
			if [[ "$OUTPUT" != *"Semantic error:"* || "$OUTPUT" == *"Code generation is not implemented yet."* ]]; then
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

echo "Checking instrument mapping..."
INSTRUMENT_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/semantic-instrument.c src/main/c/backend/semantic-analysis/Semantic.c -o "$INSTRUMENT_TEST" || ! "$INSTRUMENT_TEST"; then
    echo -e "    instrument mapping, ${RED}failed${OFF}"
    STATUS=1
fi
rm -f "$INSTRUMENT_TEST"
echo "Checking allocation failure..."
OOM_TEST=$(mktemp)
if ! cc -std=gnu99 src/test/c/semantic-oom.c src/main/c/backend/semantic-analysis/Semantic.c -Wl,--wrap=malloc -o "$OOM_TEST" || ! "$OOM_TEST"; then
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

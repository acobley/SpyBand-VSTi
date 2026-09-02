#!/bin/sh
#-----------------------------------------------------------------------------
# Build and run every test suite.
#
# None of these need the VST3 SDK except ParamTableTests, which needs only
# its headers - so this runs on any machine with a C++17 compiler, with no
# host and no window server.
#
#   ./run-tests.sh
#-----------------------------------------------------------------------------

set -e
cd "$(dirname "$0")"

SDK="../external/vst3sdk"
if [ ! -d "$SDK" ]; then
	SDK="${VST3_SDK_ROOT:-}"
fi

CXX="${CXX:-c++}"
FLAGS="-std=c++17 -O2 -Wall -Wextra -I../source"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

status=0

run () {
	name="$1"
	shift
	printf '%s\n' "--- $name"
	if "$@"; then
		:
	else
		status=1
	fi
}

$CXX $FLAGS ../source/Vocoder.cpp ../source/Adsr.cpp ../source/WavFile.cpp \
	../source/BandLayout.cpp VocoderTests.cpp -o "$OUT/vocoder-tests"
run "vocoder" "$OUT/vocoder-tests"

$CXX $FLAGS ../source/WavFile.cpp WavFileTests.cpp -o "$OUT/wav-tests"
run "wav" "$OUT/wav-tests"

if [ -n "$SDK" ] && [ -d "$SDK" ]; then
	$CXX $FLAGS -I"$SDK" ../source/SpyBandParams.cpp ../source/BandLayout.cpp \
		ParamTableTests.cpp -o "$OUT/param-tests"
	run "parameters" "$OUT/param-tests"
else
	echo "--- parameters: SKIPPED (no VST3 SDK; set VST3_SDK_ROOT)"
fi

exit $status

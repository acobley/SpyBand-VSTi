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
ROOT="$(cd .. && pwd)"

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

	$CXX $FLAGS -I"$SDK" ../source/SciFiLettering.cpp ../source/SpyBandPresets.cpp \
		../source/SpyBandParams.cpp ../source/BandLayout.cpp \
		PanelTests.cpp -o "$OUT/panel-tests"
	run "panel" "$OUT/panel-tests"
else
	echo "--- parameters, panel: SKIPPED (no VST3 SDK; set VST3_SDK_ROOT)"
fi

#-----------------------------------------------------------------------------
# The release kit: the version and maker's name agree everywhere, and the
# release scripts' own refusals still refuse. None of this needs a Mac.
#-----------------------------------------------------------------------------
printf '\n===== check-versions\n'
python3 "$ROOT/tools/check-versions.py" || status=1
for t in test-build-release test-publish test-guards; do
	printf '\n===== installer/%s\n' "$t"
	if [ -x "$ROOT/installer/$t.sh" ]; then
		"$ROOT/installer/$t.sh" || status=1
	else
		# A script that will not run is not a check that passed.
		echo "NOT EXECUTABLE: installer/$t.sh - chmod +x installer/*.sh"
		status=1
	fi
done

exit $status

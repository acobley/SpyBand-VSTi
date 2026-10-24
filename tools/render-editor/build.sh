#!/bin/sh
#-----------------------------------------------------------------------------
# Build tools/render-editor and draw the editor into PNG files.
#
# Linux only, and needs only a C++17 compiler and cairo's headers
# (libcairo2-dev) - none of the X11, xcb or pango packages a real Linux
# VSTGUI build wants, because nothing here opens a window or draws text.
#
#   tools/render-editor/build.sh [out-dir]      (default: docs/)
#
# The VST3 SDK is looked for in external/vst3sdk, or $VST3_SDK_ROOT.
#-----------------------------------------------------------------------------

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SDK="${VST3_SDK_ROOT:-$ROOT/external/vst3sdk}"
OUT="${1:-$ROOT/docs}"
BUILD="${BUILD_DIR:-$ROOT/build/render-editor}"
CXX="${CXX:-c++}"

if [ ! -d "$SDK/vstgui4" ]; then
	echo "no VST3 SDK at $SDK - set VST3_SDK_ROOT" >&2
	exit 1
fi

mkdir -p "$BUILD" "$OUT"

VG="$SDK/vstgui4/vstgui/lib"
FLAGS="-std=c++17 -O1 -w -DRELEASE=1 -DSMTG_RENDER_PREVIEW=1 \
	-I$HERE/stub -I$ROOT/source -I$SDK -I$SDK/vstgui4 $(pkg-config --cflags cairo 2>/dev/null)"

SOURCES="
$(ls "$VG"/*.cpp | grep -v -e vstguiinit.cpp)
$VG/vstguiinit.cpp
$(ls "$VG"/controls/*.cpp)
$(ls "$VG"/animation/*.cpp)
$(ls "$VG"/viewlayouter/*.cpp)
$VG/platform/common/genericoptionmenu.cpp
$VG/platform/common/generictextedit.cpp
$VG/platform/common/fileresourceinputstream.cpp
$VG/platform/linux/cairobitmap.cpp
$VG/platform/linux/cairographicscontext.cpp
$VG/platform/linux/cairopath.cpp
$VG/platform/linux/cairogradient.cpp
$VG/platform/linux/linuxtaskexecutor.cpp
$(ls "$SDK"/base/source/*.cpp)
$(ls "$SDK"/base/thread/source/*.cpp)
$(ls "$SDK"/pluginterfaces/base/*.cpp)
$SDK/public.sdk/source/common/pluginview.cpp
$SDK/public.sdk/source/common/commoniids.cpp
$SDK/public.sdk/source/vst/vsteditcontroller.cpp
$SDK/public.sdk/source/vst/vstcomponentbase.cpp
$SDK/public.sdk/source/vst/vstparameters.cpp
$SDK/public.sdk/source/vst/vstpresetfile.cpp
$SDK/public.sdk/source/vst/vstguieditor.cpp
$SDK/public.sdk/source/vst/vstinitiids.cpp
$SDK/public.sdk/source/vst/hosting/hostclasses.cpp
$SDK/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
$SDK/public.sdk/source/common/commonstringconvert.cpp
$SDK/public.sdk/source/main/moduleinit.cpp
$SDK/public.sdk/source/vst/utility/stringconvert.cpp
$ROOT/source/SciFiLettering.cpp
$ROOT/source/SpyBandStyle.cpp
$ROOT/source/SpyBandControls.cpp
$ROOT/source/SpyBandEditor.cpp
$ROOT/source/SpyBandController.cpp
$ROOT/source/SpyBandPresets.cpp
$ROOT/source/SpyBandParams.cpp
$ROOT/source/BandLayout.cpp
$ROOT/source/WavFile.cpp
$HERE/render-editor.cpp
"

# One object per source, rebuilt only when the source is newer.
echo "$SOURCES" | grep -v '^$' | while read -r src; do
	obj="$BUILD/$(echo "$src" | sed "s|$SDK/||; s|$ROOT/||; s|/|_|g; s|\.cpp$|.o|")"
	# The plug-in's own files are always rebuilt: they are few, and a
	# changed header would not show in their own dates.
	case "$src" in
		"$ROOT"/source/*|"$HERE"/*) echo "$CXX $FLAGS -c $src -o $obj" ;;
		*) if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then echo "$CXX $FLAGS -c $src -o $obj"; fi ;;
	esac
done > "$BUILD/commands.txt"

if [ -s "$BUILD/commands.txt" ]; then
	echo "compiling $(wc -l < "$BUILD/commands.txt") files"
	xargs -P "$(nproc 2>/dev/null || echo 4)" -I{} sh -c '{} || exit 255' < "$BUILD/commands.txt"
fi

$CXX "$BUILD"/*.o -o "$BUILD/render-editor" $(pkg-config --libs cairo 2>/dev/null || echo -lcairo) -lpthread -ldl
"$BUILD/render-editor" "$OUT"

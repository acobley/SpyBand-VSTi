// A stand-in for pango, which tools/render-editor does not need: the panel
// draws no system text, and this is the only call VSTGUI's cairo context
// makes into it. Never part of the plug-in build.
#pragma once
#include <cairo/cairo.h>
typedef struct _PangoLayout PangoLayout;
static inline void pango_cairo_show_layout (cairo_t*, PangoLayout*) {}

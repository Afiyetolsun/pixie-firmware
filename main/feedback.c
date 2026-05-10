// LED button-press feedback + on-screen button legend.
//
// Each press flashes the LED next to the pressed button briefly at
// low brightness. Uses the firefly-hollows pixels API; symbols are
// forward-declared inline since they live in the component's
// private src/ header. (Same pattern as panel-leds.c.)

#include <stdint.h>
#include <stdio.h>

#include "firefly-color.h"
#include "firefly-fixed.h"
#include "firefly-scene.h"

#include "feedback.h"


typedef void* PixelsContext;
typedef void (*PixelAnimationFunc)(color_ffxt *output, size_t count,
  fixed_ffxt t, void *arg);
extern PixelsContext pixels;
extern void pixels_animatePixel(PixelsContext context, uint32_t pixel,
  PixelAnimationFunc fn, uint32_t duration, uint32_t repeat, void *arg);


#define FEEDBACK_DURATION_MS    (140)


static void flashAnim(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    // First half on, second half fading to off so the LED clears
    // itself if no other animation kicks in afterward.
    if (t < FM_1 / 2) {
        out[0] = ffx_color_rgb(48, 48, 64);
    } else {
        // Linear fade.
        int32_t v = scalarfx(48, FM_1 - t);
        if (v < 0) { v = 0; }
        out[0] = ffx_color_rgb((uint8_t)v, (uint8_t)v, (uint8_t)(v + 16));
    }
}


void feedback_onKey(FfxKeys keys) {
    int led = -1;
    if      (keys & FfxKeyNorth)  { led = 0; }
    else if (keys & FfxKeyOk)     { led = 1; }
    else if (keys & FfxKeyCancel) { led = 2; }
    else if (keys & FfxKeySouth)  { led = 3; }
    if (led < 0) { return; }
    // repeat=0 makes this a one-shot animation; the LED lands on the
    // animation's final color and stays there until something else
    // overrides it.
    pixels_animatePixel(pixels, (uint32_t)led, flashAnim,
      FEEDBACK_DURATION_MS, 0, NULL);
}


void feedback_addButtonLegend(FfxNode panel,
  const char *northText, const char *southText,
  const char *okText, const char *cancelText) {

    if (!northText) { northText = ""; }
    if (!southText) { southText = ""; }
    if (!okText)    { okText = ""; }
    if (!cancelText) { cancelText = ""; }

    FfxScene scene = ffx_sceneNode_getScene(panel);

    // The 240px-wide screen fits about 25 chars of FfxFontMedium and
    // about 32 of FfxFontSmall. Use the small font and compact tokens
    // so the full legend is readable without clipping.
    char buf[48];
    snprintf(buf, sizeof(buf), "<%s >%s OK:%s X:%s",
      northText, southText, okText, cancelText);

    FfxNode label = ffx_scene_createLabel(scene, FfxFontSmall, buf);
    ffx_sceneGroup_appendChild(panel, label);
    ffx_sceneNode_setPosition(label, ffx_point(120, 232));
    ffx_sceneLabel_setAlign(label,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(label, COLOR_BLACK);
}

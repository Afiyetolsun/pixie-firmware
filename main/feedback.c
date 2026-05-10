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
  const char *upText, const char *downText,
  const char *okText, const char *cancelText) {

    if (!upText)     { upText = ""; }
    if (!downText)   { downText = ""; }
    if (!okText)     { okText = ""; }
    if (!cancelText) { cancelText = ""; }

    FfxScene scene = ffx_sceneNode_getScene(panel);

    // FfxFontSmall is ~7-8 px per glyph, so 240 px fits roughly 30
    // characters. Format is the four button names (UP, DN, OK, X)
    // each followed by their action verb.
    char buf[48];
    snprintf(buf, sizeof(buf), "UP:%s DN:%s OK:%s X:%s",
      upText, downText, okText, cancelText);

    FfxNode label = ffx_scene_createLabel(scene, FfxFontSmall, buf);
    ffx_sceneGroup_appendChild(panel, label);
    ffx_sceneNode_setPosition(label, ffx_point(120, 232));
    ffx_sceneLabel_setAlign(label,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(label, COLOR_BLACK);
}


////////////////////////////////
// FPS counter

#include "utils.h"

void feedback_addFpsCounter(FpsCounter *fps, FfxNode panel) {
    FfxScene scene = ffx_sceneNode_getScene(panel);
    fps->label = ffx_scene_createLabel(scene, FfxFontSmall, "");
    ffx_sceneGroup_appendChild(panel, fps->label);
    ffx_sceneNode_setPosition(fps->label, ffx_point(4, 10));
    ffx_sceneLabel_setAlign(fps->label,
      FfxTextAlignLeft | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(fps->label, COLOR_BLACK);
    fps->windowStart = ticks();
    fps->frames = 0;
}

void feedback_tickFps(FpsCounter *fps) {
    if (!fps || !fps->label) { return; }
    fps->frames++;
    uint32_t t = ticks();
    uint32_t window = t - fps->windowStart;
    if (window >= 1000) {
        uint32_t fpsValue = (fps->frames * 1000) / window;
        ffx_sceneLabel_setTextFormat(fps->label, "%lu fps",
          (unsigned long)fpsValue);
        fps->windowStart = t;
        fps->frames = 0;
    }
}

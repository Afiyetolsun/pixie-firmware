// LED mode picker. Drives the 4 WS2812B LEDs via firefly-hollows'
// internal pixels API. Each mode is a PixelAnimationFunc kicked off
// with pixels_animate(); the most recently selected mode keeps
// running after the panel is popped.

#include <stdint.h>
#include <stdio.h>

#include "firefly-color.h"
#include "firefly-fixed.h"
#include "firefly-hollows.h"
#include "firefly-scene.h"

#include "panels.h"
#include "utils.h"


#define MODE_COUNT      (7)
#define LED_COUNT       (4)


// firefly-hollows' pixels API lives in src/ (private to the
// component). Forward-declare what we need; the linker resolves to
// the same symbols task-io.c uses.
typedef void* PixelsContext;
typedef void (*PixelAnimationFunc)(color_ffxt *output, size_t count,
  fixed_ffxt t, void *arg);
extern PixelsContext pixels;
extern void pixels_animate(PixelsContext context, PixelAnimationFunc fn,
  uint32_t duration, uint32_t repeat, void *arg);
extern void pixels_stopAnimation(PixelsContext context, uint32_t final);


typedef struct LedsState {
    FfxScene scene;
    FfxNode bg;
    FfxNode titleLabel;
    FfxNode modeLabel;
    FfxNode descLabel;
    FfxNode dots[MODE_COUNT];
    FfxNode hint;

    int mode;
} LedsState;


static const char *modeNames[MODE_COUNT] = {
    "OFF", "CYAN", "RAINBOW", "PULSE", "STROBE", "POLICE", "MATRIX"
};
static const char *modeDescs[MODE_COUNT] = {
    "all off",
    "solid neon cyan",
    "cycling hues",
    "slow breathing",
    "1 Hz white flash",
    "alternating red/blue",
    "green wave"
};
static const uint32_t modeDurations[MODE_COUNT] = {
    1000, 1000, 4000, 1800, 1000, 800, 1200
};


static void animOff(color_ffxt *out, size_t count, fixed_ffxt t, void *arg) {
    for (size_t i = 0; i < count; i++) { out[i] = COLOR_BLACK; }
}

static void animCyan(color_ffxt *out, size_t count, fixed_ffxt t, void *arg) {
    color_ffxt c = ffx_color_rgb(0, 220, 240);
    for (size_t i = 0; i < count; i++) { out[i] = c; }
}

static void animRainbow(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    int32_t base = scalarfx(3960, t);
    for (size_t i = 0; i < count; i++) {
        int32_t hue = (base + (int32_t)i * 990) % 3960;
        out[i] = ffx_color_hsv(hue, MAX_SAT, MAX_VAL);
    }
}

static void animPulse(color_ffxt *out, size_t count, fixed_ffxt t, void *arg) {
    int32_t v;
    fixed_ffxt half = FM_1 / 2;
    if (t < half) { v = scalarfx(MAX_VAL, mulfx(t, tofx(2))); }
    else          { v = scalarfx(MAX_VAL, mulfx(FM_1 - t, tofx(2))); }
    color_ffxt c = ffx_color_hsv(275, MAX_SAT, v);
    for (size_t i = 0; i < count; i++) { out[i] = c; }
}

static void animStrobe(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    bool on = t < (FM_1 / 4);
    color_ffxt c = on ? ffx_color_rgb(255, 255, 255) : COLOR_BLACK;
    for (size_t i = 0; i < count; i++) { out[i] = c; }
}

static void animPolice(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    bool phase = t < (FM_1 / 2);
    color_ffxt red  = ffx_color_rgb(255, 0, 0);
    color_ffxt blue = ffx_color_rgb(0, 0, 255);
    for (size_t i = 0; i < count; i++) {
        bool side = (i & 1) ^ phase;
        out[i] = side ? red : blue;
    }
}

static void animMatrix(color_ffxt *out, size_t count, fixed_ffxt t,
  void *arg) {
    int32_t head = scalarfx((int32_t)(count * 2), t);
    for (size_t i = 0; i < count; i++) {
        int32_t dist = (head - (int32_t)i + (int32_t)(count * 2))
          % (int32_t)(count * 2);
        int32_t v = (dist < (int32_t)count)
          ? (MAX_VAL - dist * (MAX_VAL / (int32_t)count))
          : 0;
        if (v < 0) { v = 0; }
        out[i] = ffx_color_hsv(180, MAX_SAT, v);
    }
}

static const PixelAnimationFunc modeFuncs[MODE_COUNT] = {
    animOff, animCyan, animRainbow, animPulse,
    animStrobe, animPolice, animMatrix
};


static void applyMode(LedsState *state) {
    pixels_stopAnimation(pixels, 0);
    pixels_animate(pixels, modeFuncs[state->mode], modeDurations[state->mode],
      0, NULL);

    ffx_sceneLabel_setText(state->modeLabel, modeNames[state->mode]);
    ffx_sceneLabel_setText(state->descLabel, modeDescs[state->mode]);
    for (int i = 0; i < MODE_COUNT; i++) {
        ffx_sceneBox_setColor(state->dots[i],
          i == state->mode ? ffx_color_rgb(0, 255, 200)
                           : ffx_color_rgb(40, 40, 60));
    }
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_state) {
    LedsState *state = _state;
    switch (props.keys.down) {
        case FfxKeyCancel:
            ffx_popPanel(0);
            return;
        case FfxKeyNorth:
            state->mode = (state->mode + MODE_COUNT - 1) % MODE_COUNT;
            applyMode(state);
            break;
        case FfxKeySouth:
            state->mode = (state->mode + 1) % MODE_COUNT;
            applyMode(state);
            break;
        case FfxKeyOk:
            applyMode(state);
            break;
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *_state, void *arg) {
    LedsState *state = _state;
    state->scene = scene;
    state->mode = 2;

    state->bg = ffx_scene_createBox(scene, ffx_size(240, 240));
    ffx_sceneBox_setColor(state->bg, ffx_color_rgb(4, 0, 12));
    ffx_sceneGroup_appendChild(panel, state->bg);
    ffx_sceneNode_setPosition(state->bg, ffx_point(0, 0));

    state->titleLabel = ffx_scene_createLabel(scene, FfxFontLargeBold,
      "LED MODE");
    ffx_sceneGroup_appendChild(panel, state->titleLabel);
    ffx_sceneNode_setPosition(state->titleLabel, ffx_point(120, 26));
    ffx_sceneLabel_setAlign(state->titleLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->titleLabel, COLOR_BLACK);

    int dotW = 20;
    int dotH = 6;
    int totalW = MODE_COUNT * dotW + (MODE_COUNT - 1) * 4;
    int x0 = (240 - totalW) / 2;
    for (int i = 0; i < MODE_COUNT; i++) {
        FfxNode dot = ffx_scene_createBox(scene, ffx_size(dotW, dotH));
        ffx_sceneGroup_appendChild(panel, dot);
        ffx_sceneNode_setPosition(dot, ffx_point(x0 + i * (dotW + 4), 56));
        state->dots[i] = dot;
    }

    state->modeLabel = ffx_scene_createLabel(scene, FfxFontLargeBold, "");
    ffx_sceneGroup_appendChild(panel, state->modeLabel);
    ffx_sceneNode_setPosition(state->modeLabel, ffx_point(120, 116));
    ffx_sceneLabel_setAlign(state->modeLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->modeLabel, COLOR_BLACK);

    state->descLabel = ffx_scene_createLabel(scene, FfxFontMedium, "");
    ffx_sceneGroup_appendChild(panel, state->descLabel);
    ffx_sceneNode_setPosition(state->descLabel, ffx_point(120, 156));
    ffx_sceneLabel_setAlign(state->descLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->descLabel, COLOR_BLACK);

    state->hint = ffx_scene_createLabel(scene, FfxFontMedium,
      "N/S:MODE  X:EXIT (mode persists)");
    ffx_sceneGroup_appendChild(panel, state->hint);
    ffx_sceneNode_setPosition(state->hint, ffx_point(120, 230));
    ffx_sceneLabel_setAlign(state->hint,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(state->hint, COLOR_BLACK);

    applyMode(state);

    ffx_onEvent(FfxEventKeys, onKeys, state);

    return 0;
}

int pushPanelLeds() {
    return ffx_pushPanel(initFunc, sizeof(LedsState), NULL);
}

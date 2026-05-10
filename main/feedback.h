// Shared button-feedback helpers used by every panel.

#ifndef __PIXIE_FEEDBACK_H__
#define __PIXIE_FEEDBACK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "firefly-hollows.h"
#include "firefly-scene.h"


// Brief low-brightness flash on the LED that corresponds to the
// pressed button: North=LED0, OK=LED1, Cancel=LED2, South=LED3.
// Safe to call from any panel's onKeys; a no-op if `keys` is 0.
void feedback_onKey(FfxKeys keys);


// Add a small bottom-of-screen legend describing what each of the
// four buttons does in the current panel. The Pixie has only four
// keys: UP, DN, OK, X (back). Each action label is short (a verb).
// Pass NULL for a button with no action.
//
// Example:
//   feedback_addButtonLegend(panel, "up", "dn", "go", "exit");
//
// Renders:
//   UP:up  DN:dn  OK:go  X:exit
void feedback_addButtonLegend(FfxNode panel,
  const char *upText, const char *downText,
  const char *okText, const char *cancelText);


// Lightweight running FPS counter pinned to the top-left of a panel.
// Each panel that wants one keeps the FpsCounter struct in its state,
// calls feedback_addFpsCounter() from its init function, and calls
// feedback_tickFps() once per render frame. The label updates ~once
// per second.
typedef struct FpsCounter {
    FfxNode label;
    uint32_t windowStart;
    uint32_t frames;
} FpsCounter;

void feedback_addFpsCounter(FpsCounter *fps, FfxNode panel);
void feedback_tickFps(FpsCounter *fps);


#ifdef __cplusplus
}
#endif

#endif  /* __PIXIE_FEEDBACK_H__ */

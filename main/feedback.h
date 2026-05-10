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
// four buttons does in the current panel. Each label is short
// (a verb or an arrow). Pass NULL for a button that has no action.
//
// Example:
//   feedback_addButtonLegend(panel, "UP", "DN", "SEL", "EXIT");
//
// Renders:
//   < UP  > DN  OK SEL  X EXIT
void feedback_addButtonLegend(FfxNode panel,
  const char *northText, const char *southText,
  const char *okText, const char *cancelText);


#ifdef __cplusplus
}
#endif

#endif  /* __PIXIE_FEEDBACK_H__ */

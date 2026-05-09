// Single translation unit that pulls in every image-data header.
//
// The headers in main/images/ define their `const uint16_t image_X[] = {…}`
// arrays at file scope without `static`, so including them in two .c files
// produces duplicate-symbol link errors. Including them only here gives
// each array exactly one definition; other panels see them via
// `image-data.h`'s extern declarations.

#include <stdint.h>
#include <stddef.h>


#include "images/image-alien-1.h"
#include "images/image-alien-2.h"
#include "images/image-alien-boom.h"
#include "images/image-arrow.h"
#include "images/image-bullet.h"
#include "images/image-ship.h"
#include "images/image-space.h"

// image-background.h and image-pixie.h are intentionally not included:
// firefly-hollows ships its own copy of those symbols in
// src/demo/background-pixies.c. Including ours would conflict at link
// time, and we don't currently use them from main/.


// Companion size constants, computed in this TU where sizeof() works.
const size_t image_space_len     = sizeof(image_space);
const size_t image_ship_len      = sizeof(image_ship);
const size_t image_bullet_len    = sizeof(image_bullet);
const size_t image_alien1_len    = sizeof(image_alien1);
const size_t image_alien2_len    = sizeof(image_alien2);
const size_t image_alienboom_len = sizeof(image_alienboom);
const size_t image_arrow_len     = sizeof(image_arrow);

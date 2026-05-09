// Le Space - Space Invaders with progressive levels.
// Original mechanics by Richard Moore; multi-level state machine and
// scoring layered on top.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "firefly-scene.h"
#include "firefly-hollows.h"

#include "panels.h"
#include "utils.h"


#include "image-data.h"


#define MAX_ROWS        (6)
#define MAX_COLS        (5)
#define MAX_ALIENS      (MAX_ROWS * MAX_COLS)
#define MAX_BULLETS     (5)
#define MAX_LEVEL       (5)

#define SPLASH_MS       (1500)
#define END_HOLD_MS     (1500)


typedef struct LevelConfig {
    int rows;
    int cols;
    int bulletCap;
    int alienAnimMod;
    int stepDivisor;     // 1 = full speed; higher = slower
} LevelConfig;

static const LevelConfig levels[MAX_LEVEL] = {
    { 4, 3, 5, 6, 2 },  // L1: easy intro
    { 4, 4, 5, 4, 1 },  // L2: original feel
    { 5, 4, 5, 4, 1 },  // L3
    { 5, 5, 4, 3, 1 },  // L4
    { 6, 5, 4, 2, 1 },  // L5: BOSS - max aliens, fastest, fewer bullets
};


typedef enum GameState {
    StateSplash,
    StatePlaying,
    StateLevelClear,
    StateGameOver,
    StateVictory,
} GameState;


typedef struct SpaceState {
    GameState state;
    uint32_t stateEnteredAt;

    int level;
    int rows, cols;
    int bulletCap;
    int alienAnimMod;
    int stepDivisor;

    uint32_t score;
    uint32_t resetTimer;
    uint32_t tick;

    FfxScene scene;
    FfxNode panel;
    FfxNode ship;
    FfxNode aliens;
    FfxNode alien[MAX_ALIENS];
    FfxNode bullet[MAX_BULLETS];
    FfxNode boom[MAX_BULLETS];

    FfxNode hud;
    FfxNode hudLabel;
    FfxNode bigLabel;
    FfxNode subLabel;

    uint8_t boomLife[MAX_BULLETS];
    uint8_t dead[MAX_ALIENS];

    FfxKeys keys;
} SpaceState;


static int alienIdx(SpaceState *s, int r, int c) {
    return r * s->cols + c;
}

static void enterState(SpaceState *s, GameState st) {
    s->state = st;
    s->stateEnteredAt = ticks();
}

static void updateHud(SpaceState *s) {
    ffx_sceneLabel_setTextFormat(s->hudLabel, "L%d  %lu",
      s->level, (unsigned long)s->score);
}

static void hideOverlay(SpaceState *s) {
    ffx_sceneNode_setHidden(s->bigLabel, true);
    ffx_sceneNode_setHidden(s->subLabel, true);
}

static void showOverlay(SpaceState *s, const char *big, const char *sub) {
    ffx_sceneLabel_setText(s->bigLabel, big);
    ffx_sceneLabel_setText(s->subLabel, sub);
    ffx_sceneNode_setHidden(s->bigLabel, false);
    ffx_sceneNode_setHidden(s->subLabel, false);
}

static void resetShipAndBullets(SpaceState *s) {
    ffx_sceneNode_stopAnimations(s->ship, false);
    ffx_sceneNode_setPosition(s->ship, ffx_point(240 - 36, 120 - 19));

    for (int i = 0; i < MAX_BULLETS; i++) {
        ffx_sceneNode_setPosition(s->bullet[i], ffx_point(-10, 0));
        ffx_sceneNode_setPosition(s->boom[i],   ffx_point(300, 0));
        s->boomLife[i] = 0;
    }
}

static void setupLevel(SpaceState *s, int level) {
    LevelConfig cfg = levels[level - 1];

    s->level = level;
    s->rows = cfg.rows;
    s->cols = cfg.cols;
    s->bulletCap = cfg.bulletCap;
    s->alienAnimMod = cfg.alienAnimMod;
    s->stepDivisor = cfg.stepDivisor;

    memset(s->dead, 0, sizeof(s->dead));

    // Hide every alien node first; only the in-use slots get re-shown.
    for (int i = 0; i < MAX_ALIENS; i++) {
        ffx_sceneNode_setHidden(s->alien[i], true);
        ffx_sceneNode_setPosition(s->alien[i], ffx_point(-300, 0));
    }

    ffx_sceneNode_stopAnimations(s->aliens, false);
    ffx_sceneNode_setPosition(s->aliens, ffx_point(0, 0));

    for (int r = 0; r < s->rows; r++) {
        for (int c = 0; c < s->cols; c++) {
            FfxNode alien = s->alien[alienIdx(s, r, c)];
            ffx_sceneNode_setHidden(alien, false);
            ffx_sceneImage_setData(alien, image_alien1, image_alien1_len);
            ffx_sceneNode_setPosition(alien, ffx_point(30 * r, c * 40));
        }
    }

    resetShipAndBullets(s);
    updateHud(s);

    char splash[16];
    snprintf(splash, sizeof(splash), "LEVEL %d", level);
    showOverlay(s, splash, level == MAX_LEVEL ? "FINAL" : "READY");
    enterState(s, StateSplash);
}

static void startNewGame(SpaceState *s) {
    s->score = 0;
    setupLevel(s, 1);
}

static void explodeShip(SpaceState *s) {
    FfxPoint ship = ffx_sceneNode_getPosition(s->ship);
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (s->boomLife[i]) { continue; }
        s->boomLife[i] = 12;
        ffx_sceneNode_setPosition(s->boom[i], ship);
    }
    ship.x = 300;
    ffx_sceneNode_setPosition(s->ship, ship);
}

static void explodeAlien(SpaceState *s, int index) {
    s->dead[index] = 1;
    s->score += 10 * s->level;

    FfxPoint alien = ffx_sceneNode_getPosition(s->alien[index]);
    FfxPoint aliens = ffx_sceneNode_getPosition(s->aliens);

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (s->boomLife[i]) { continue; }
        s->boomLife[i] = 12;
        ffx_sceneNode_setPosition(s->boom[i], (FfxPoint){
            .x = aliens.x + alien.x,
            .y = aliens.y + alien.y
        });
    }

    alien.x = 300;
    ffx_sceneNode_setPosition(s->alien[index], alien);
    updateHud(s);
}

static void onFocus(FfxEvent event, FfxEventProps props, void *_app) { }

static void renderPlaying(SpaceState *s) {
    FfxPoint ship = ffx_sceneNode_getPosition(s->ship);
    FfxPoint aliens = ffx_sceneNode_getPosition(s->aliens);

    if (s->keys == FfxKeyOk && ticks() - s->resetTimer > 3000) {
        ffx_popPanel(GameResultQuit);
        return;
    }

    if (s->keys & FfxKeyNorth) {
        if (ship.y > 0) { ship.y -= 2; }
    } else if (s->keys & FfxKeySouth) {
        if (ship.y < 240 - 38) { ship.y += 2; }
    }
    ffx_sceneNode_setPosition(s->ship, ship);

    s->tick++;

    for (int i = 0; i < MAX_BULLETS; i++) {
        FfxPoint b = ffx_sceneNode_getPosition(s->bullet[i]);
        if (b.x > -10) { b.x -= 2; }
        ffx_sceneNode_setPosition(s->bullet[i], b);

        if (s->boomLife[i]) {
            s->boomLife[i]--;
            if (s->boomLife[i] == 0) {
                ffx_sceneNode_setPosition(s->boom[i], ffx_point(300, 0));
            }
        }
    }

    if ((s->tick % s->alienAnimMod) == 0) {
        int total = s->rows * s->cols;
        if (total > 0) {
            int toggle = (s->tick / s->alienAnimMod) % total;
            if (!s->dead[toggle]) {
                const uint16_t *current =
                  ffx_sceneImage_getData(s->alien[toggle]);
                if (current == image_alien1) {
                    ffx_sceneImage_setData(s->alien[toggle], image_alien2,
                      image_alien2_len);
                } else {
                    ffx_sceneImage_setData(s->alien[toggle], image_alien1,
                      image_alien1_len);
                }
            }
        }
    }

    bool allKill = true;
    int total = s->rows * s->cols;
    for (int i = 0; i < total; i++) {
        if (s->dead[i]) { continue; }
        allKill = false;

        FfxPoint a = ffx_sceneNode_getPosition(s->alien[i]);
        FfxPoint w = { aliens.x + a.x + 10, aliens.y + a.y + 13 };

        for (int j = 0; j < MAX_BULLETS; j++) {
            FfxPoint b = ffx_sceneNode_getPosition(s->bullet[j]);
            if (abs(b.x - w.x) < 10 && abs(b.y - w.y) < 13) {
                explodeAlien(s, i);
                b.x = -10;
                ffx_sceneNode_setPosition(s->bullet[j], b);
                break;
            }
        }
    }

    if (allKill) {
        s->score += 100 * s->level;
        updateHud(s);
        ffx_sceneNode_animatePosition(s->ship, ffx_point(-200, ship.y),
          0, 800, FfxCurveEaseInQuad, NULL, NULL);
        showOverlay(s, "CLEAR!", "");
        enterState(s, StateLevelClear);
        return;
    }

    bool isDead = false;
    int leftMost = 0, rightMost = 240;
    for (int i = 0; i < total; i++) {
        if (s->dead[i]) { continue; }

        FfxPoint a = ffx_sceneNode_getPosition(s->alien[i]);
        if (a.y < rightMost) { rightMost = a.y; }
        if (a.y + 26 > leftMost) { leftMost = a.y + 26; }

        FfxPoint w = { aliens.x + a.x + 10, aliens.y + a.y + 13 };
        if (w.x + 7 > ship.x && abs(ship.y + 19 - w.y) < 20) {
            isDead = true;
            break;
        }
    }

    if (isDead) {
        explodeShip(s);
        ffx_sceneNode_animatePosition(s->aliens, ffx_point(480, aliens.y),
          0, 1000, FfxCurveEaseInBack, NULL, NULL);
        showOverlay(s, "GAME OVER", "OK = AGAIN");
        enterState(s, StateGameOver);
        return;
    }

    // Alien field movement: zig-zag. stepDivisor slows movement on easy levels.
    if ((s->tick % s->stepDivisor) == 0) {
        if ((aliens.x % 8) == 0) {
            if (leftMost + aliens.y < 240) {
                aliens.y += 2;
            } else {
                aliens.x += 4;
            }
        } else {
            if (rightMost + aliens.y > 0) {
                aliens.y -= 2;
            } else {
                aliens.x += 4;
            }
        }
        ffx_sceneNode_setPosition(s->aliens, aliens);
    }
}

static void onRender(FfxEvent event, FfxEventProps props, void *_app) {
    SpaceState *s = _app;
    uint32_t t = ticks();

    switch (s->state) {
        case StateSplash:
            if (t - s->stateEnteredAt > SPLASH_MS) {
                hideOverlay(s);
                enterState(s, StatePlaying);
            }
            break;

        case StatePlaying:
            renderPlaying(s);
            break;

        case StateLevelClear:
            if (t - s->stateEnteredAt > END_HOLD_MS) {
                int next = s->level + 1;
                if (next > MAX_LEVEL) {
                    showOverlay(s, "VICTORY!", "OK = AGAIN");
                    enterState(s, StateVictory);
                } else {
                    setupLevel(s, next);
                }
            }
            break;

        case StateGameOver:
        case StateVictory:
            // Wait for OK / Cancel handled in onKeys.
            break;
    }
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_app) {
    SpaceState *s = _app;
    uint32_t keys = props.keys.down;
    s->keys = keys;

    if (s->state == StateGameOver || s->state == StateVictory) {
        if (keys & FfxKeyOk) {
            startNewGame(s);
            return;
        }
        if (keys & FfxKeyCancel) {
            ffx_popPanel(s->state == StateVictory
              ? GameResultWin : GameResultLose);
            return;
        }
        return;
    }

    if (s->state != StatePlaying) { return; }

    s->resetTimer = (keys == FfxKeyOk) ? ticks() : 0;

    if (keys & FfxKeyCancel) {
        FfxPoint ship = ffx_sceneNode_getPosition(s->ship);
        for (int i = 0; i < s->bulletCap; i++) {
            FfxPoint b = ffx_sceneNode_getPosition(s->bullet[i]);
            if (b.x > -10) { continue; }     // already in flight
            b.y = ship.y + 16;
            b.x = 240 - 32 - 2;
            ffx_sceneNode_setPosition(s->bullet[i], b);
            break;
        }
    }
}

static int initFunc(FfxScene scene, FfxNode panel, void *panelState,
  void *arg) {
    SpaceState *s = panelState;
    s->scene = scene;
    s->panel = panel;

    FfxNode bg = ffx_scene_createImage(scene, image_space, image_space_len);
    ffx_sceneGroup_appendChild(panel, bg);

    for (int i = 0; i < MAX_BULLETS; i++) {
        FfxNode bullet = ffx_scene_createImage(scene, image_bullet,
          image_bullet_len);
        s->bullet[i] = bullet;
        ffx_sceneGroup_appendChild(panel, bullet);
        ffx_sceneNode_setPosition(bullet, ffx_point(-10, 0));

        FfxNode boom = ffx_scene_createImage(scene, image_alienboom,
          image_alienboom_len);
        s->boom[i] = boom;
        ffx_sceneGroup_appendChild(panel, boom);
        ffx_sceneNode_setPosition(boom, ffx_point(300, 0));
    }

    FfxNode ship = ffx_scene_createImage(scene, image_ship, image_ship_len);
    s->ship = ship;
    ffx_sceneGroup_appendChild(panel, ship);

    FfxNode aliens = ffx_scene_createGroup(scene);
    s->aliens = aliens;
    ffx_sceneGroup_appendChild(panel, aliens);

    for (int i = 0; i < MAX_ALIENS; i++) {
        FfxNode alien = ffx_scene_createImage(scene, image_alien1,
          image_alien1_len);
        s->alien[i] = alien;
        ffx_sceneGroup_appendChild(aliens, alien);
        ffx_sceneNode_setHidden(alien, true);
        ffx_sceneNode_setPosition(alien, ffx_point(-300, 0));
    }

    s->hud = ffx_scene_createBox(scene, ffx_size(240, 16));
    ffx_sceneBox_setColor(s->hud, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(panel, s->hud);
    ffx_sceneNode_setPosition(s->hud, ffx_point(0, 0));

    s->hudLabel = ffx_scene_createLabel(scene, FfxFontMedium, "L1  0");
    ffx_sceneGroup_appendChild(panel, s->hudLabel);
    ffx_sceneNode_setPosition(s->hudLabel, ffx_point(120, 8));
    ffx_sceneLabel_setAlign(s->hudLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(s->hudLabel, COLOR_BLACK);

    s->bigLabel = ffx_scene_createLabel(scene, FfxFontLargeBold, "");
    ffx_sceneGroup_appendChild(panel, s->bigLabel);
    ffx_sceneNode_setPosition(s->bigLabel, ffx_point(120, 110));
    ffx_sceneLabel_setAlign(s->bigLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(s->bigLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(s->bigLabel, true);

    s->subLabel = ffx_scene_createLabel(scene, FfxFontMedium, "");
    ffx_sceneGroup_appendChild(panel, s->subLabel);
    ffx_sceneNode_setPosition(s->subLabel, ffx_point(120, 140));
    ffx_sceneLabel_setAlign(s->subLabel,
      FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setOutlineColor(s->subLabel, COLOR_BLACK);
    ffx_sceneNode_setHidden(s->subLabel, true);

    startNewGame(s);

    ffx_onEvent(FfxEventKeys, onKeys, s);
    ffx_onEvent(FfxEventRenderScene, onRender, s);
    ffx_onEvent(FfxEventFocus, onFocus, s);

    return 0;
}

GameResult pushPanelSpace() {
    return ffx_pushPanel(initFunc, sizeof(SpaceState), NULL);
}

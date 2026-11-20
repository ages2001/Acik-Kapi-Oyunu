#include "kit.h"

#define WK_TARGET_ROW(id, code, label, file) {code, label, file},
const WK_TARGET_OPTION WkTargets[WK_TARGET_COUNT] = { WK_WDM_TARGETS(WK_TARGET_ROW) };

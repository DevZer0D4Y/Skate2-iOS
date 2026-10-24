#include "frame_rate.h"

#include <rex/cvar.h>
#include <rex/logging.h>

#include <cstdint>

REXCVAR_DEFINE_STRING(skate2_frame_rate, "original", "Skate 2",
    "Frame rate: original for the console's 30, or 60 (experimental). The game's clock is "
    "unchanged; 60 presents on every vblank instead of every second one.")
    .allowed({"original", "60"})
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace {

// Zero leaves the game's own presentation interval in r11. Set once on the
// startup thread, before any guest thread exists.
uint64_t interval_override = 0;

}  // namespace

void ConfigureSkate2FrameRate() {
  const auto mode = rex::cvar::GetFlagByName("skate2_frame_rate");
  // The original word at 0x817F351C selects the game's presentation interval;
  // one vblank gives 60 FPS.
  interval_override = mode == "60" ? 1 : 0;
  REXLOG_INFO("Skate 2 frame rate: {}", mode == "60" ? "60" : "original");
}

void Skate2PresentationInterval(PPCRegister& r11) {
  if (interval_override != 0) {
    r11.u64 = interval_override;
  }
}

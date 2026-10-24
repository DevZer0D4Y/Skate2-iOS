// skate2 - ReXGlue Recompiled Project (iOS)
//
// Skate 2 (Xbox 360) on the iOS port of ReXGlue. This class adds what iOS
// needs: paths inside the app container, the disc image picker, and the
// settings that suit an iPhone GPU through MoltenVK.

#pragma once

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#if REX_PLATFORM_IOS
#include "ios_screen.h"
#include "touch_layout_skate2.h"
#include "iso_import.h"
#include <rex/chrono/clock.h>
#include <rex/audio/audio_system.h>
#include <rex/ui/windowed_app_context_sdl.h>
#endif

#include "frame_rate.h"
#include "game_check.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#if REX_PLATFORM_MAC
#include <rex/cvar_defaults.h>
#endif

class Skate2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Skate2App>(new Skate2App(ctx, "skate2", PPCImageConfig));
  }

 protected:
  void OnPreSetup(rex::RuntimeConfig& config) override {
    ConfigureSkate2FrameRate();
    if (config.gpu_plugin.empty()) config.gpu_plugin = "xenos";
  }

#if REX_PLATFORM_MAC && !REX_PLATFORM_IOS
  // macOS: the same port running natively, used to test the iPhone build's
  // code paths (Metal through MoltenVK, arm64, 16 KB pages) on a Mac. The game
  // folder comes from --game_data_root as on desktop.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    SetDefaults();
    game_root_ = paths.game_data_root;
  }

  // After OnConfigurePaths, once there is a log to report to.
  void OnPostInitLogging() override {
    const skate2::GameSource game(game_root_);
    if (const std::string problem = skate2::CheckGame(game); !problem.empty()) {
      REXLOG_ERROR("{}: {}", game_root_.string(), problem);
    }
  }

  void OnPostSetup() override {
    SetGuestFrameStats([this] { return SampleFrame(); });
  }
#endif

#if REX_PLATFORM_IOS
  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults, std::function<void(rex::PathConfig)> resume) override {
    std::error_code ec;
    std::string problem;
    if (std::filesystem::is_regular_file(defaults.game_data_root, ec) ||
        std::filesystem::is_regular_file(defaults.game_data_root / "default.xex", ec)) {
      const skate2::GameSource game(defaults.game_data_root);
      problem = skate2::CheckGame(game);
      if (problem.empty()) {
        return defaults;
      }
      REXLOG_ERROR("{}: {}", defaults.game_data_root.string(), problem);
    }
    // No game yet (a first launch), or one that cannot be used: ask for the
    // disc, saying why in the second case.
    Skate2ImportISO(
        [defaults, resume = std::move(resume)](std::filesystem::path image) mutable {
          // The importer has checked it.
          auto paths = defaults;
          paths.game_data_root = std::move(image);
          resume(std::move(paths));
        },
        std::move(problem));
    return std::nullopt;
  }

  // iOS starts the app with no arguments and its bundle is read-only, so
  // everything lives in <container>/Documents, which the Files app shows
  // (UIFileSharingEnabled + LSSupportsOpeningDocumentsInPlace):
  //
  //   Documents/game/          the extracted disc (default.xex, data/)
  //   Documents/userdata/      saves and the profile
  //   Documents/cache/         shader cache
  //   Documents/skate2.toml    settings, editable from Files
  //
  // Defaults go through SetDefaultByName: it ranks below the config file and,
  // unlike SetFlagByName, reaches cvars owned by the GPU plugin, which only
  // registers after this hook runs.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    const auto docs = Documents();
    std::error_code ec;
    if (paths.game_data_root.empty()) {
      paths.game_data_root = docs / "game";
    }
    paths.user_data_root = docs / "userdata";
    paths.cache_root = docs / "cache";
    paths.config_path = docs / "skate2.toml";
    for (const auto& dir : {paths.game_data_root, paths.user_data_root, paths.cache_root}) {
      std::filesystem::create_directories(dir, ec);
    }
    RunCleanupRequest(docs, paths.game_data_root);
    WriteDefaultConfig(paths.config_path);
    SetDefaults();
    // Before the SDK builds its on-screen gamepad from the layout.
    Skate2InstallTouchLayout();
    // Files app import: prefer an explicitly named image over extracted data.
    for (const auto& image : {docs / "game.iso", docs / "game.xiso", docs / "game" / "game.iso"}) {
      if (std::filesystem::is_regular_file(image, ec)) {
        paths.game_data_root = image;
        break;
      }
    }
    REXLOG_INFO("iOS paths: game={} config={}", paths.game_data_root.string(),
                paths.config_path.string());
  }

  void OnPostSetup() override {
    static_cast<rex::ui::SDLWindowedAppContext&>(app_context()).SetLifecycleCallback(
        [this](bool backgrounded) {
          if (backgrounded) {
            static_cast<rex::audio::AudioSystem*>(runtime()->audio_system())->SetBackgrounded(true);
            rex::chrono::Clock::SetPaused(true);
            runtime()->graphics_system()->SetBackgrounded(true);
            REXLOG_INFO("iOS: gameplay suspended; session retained");
            rex::FlushLogging();
          } else {
            rex::chrono::Clock::SetPaused(false);
            static_cast<rex::audio::AudioSystem*>(runtime()->audio_system())->SetBackgrounded(false);
            runtime()->graphics_system()->SetBackgrounded(false);
            has_last_frame_ = false;
            Skate2KeepScreenAwake(true);
            REXLOG_INFO("iOS: gameplay resumed in the existing session");
          }
        });
    Skate2KeepScreenAwake(true);
    // "Guest: N FPS" in the F3 overlay; the SDK only shows it once an app
    // provides frame timing.
    SetGuestFrameStats([this] { return SampleFrame(); });
  }

  void OnShutdown() override {
    static_cast<rex::ui::SDLWindowedAppContext&>(app_context()).SetLifecycleCallback({});
  }
#endif

 private:
#if REX_PLATFORM_IOS
  static std::filesystem::path Documents() {
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home && home[0] ? home : ".") / "Documents";
  }

  // Documents/cleanup.txt: game data paths to delete, one per line, relative to
  // Documents/game (for example "data/movies"). Without USB the phone's files
  // can only be added from a computer, not removed, so a computer places this
  // list and the next launch applies it, once. Nothing outside the game folder
  // can be named.
  static void RunCleanupRequest(const std::filesystem::path& docs,
                                const std::filesystem::path& game_root) {
    const auto request = docs / "cleanup.txt";
    std::error_code ec;
    if (!std::filesystem::exists(request, ec)) {
      return;
    }
    std::ifstream in(request);
    std::string line;
    while (std::getline(in, line)) {
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
      }
      const std::filesystem::path relative(line);
      if (line.empty() || line[0] == '#' || relative.is_absolute() ||
          std::any_of(relative.begin(), relative.end(),
                      [](const std::filesystem::path& part) { return part == ".."; })) {
        continue;
      }
      const auto target = game_root / relative;
      const auto removed = std::filesystem::remove_all(target, ec);
      REXLOG_INFO("cleanup.txt: removed {} ({} entries){}", target.string(),
                  ec ? 0 : removed, ec ? " - " + ec.message() : "");
    }
    in.close();
    std::filesystem::remove(request, ec);
  }

  static void WriteDefaultConfig(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
      return;
    }
    std::ofstream out(path);
    if (!out) {
      return;
    }
    out << "# Skate 2 settings. Edit in the Files app; takes effect on relaunch.\n"
           "gpu_plugin = \"xenos\"\n"
           "\n"
           "# The game renders 16:9 and the panel is wider.\n"
           "#   false -> stretch to fill, true -> keep the aspect with side bars\n"
           "present_letterbox = false\n"
           "\n"
           "# On-screen gamepad: \"auto\" shows it while no controller is connected,\n"
           "# \"on\" always, \"off\" never. touch_controls_opacity (0.1 to 1.0) dims it.\n"
           "touch_controls = \"auto\"\n"
           "\n"
           "# \"original\" is the console's 30 FPS; \"60\" is experimental.\n"
           "skate2_frame_rate = \"original\"\n"
           "\n"
           "# The game's anti-aliasing, rendered single-sampled: much less GPU work\n"
           "# on a phone. false restores it.\n"
           "vulkan_msaa_as_single_sample = true\n"
           "\n"
           "# Make the CPU wait for the GPU to finish each frame. Slower; try true\n"
           "# only if surfaces flicker or turn black.\n"
           "vulkan_wait_until_idleclean_sync = false\n";
  }
#endif

#if REX_PLATFORM_MAC
  static void SetDefaults() {
    using rex::cvar::SetDefaultByName;
    // The Xenos GPU emulation; empty would disable graphics entirely.
    SetDefaultByName("gpu_plugin", "xenos");
    SetDefaultByName("vulkan_log_debug_messages", "false");
    SetDefaultByName("present_letterbox", "false");
    // Games that ask for the profile on the controller's own port refuse to
    // save when only slot 0 is signed in: alias every slot to the one local
    // profile.
    SetDefaultByName("signin_all_user_slots", "true");
    // English: the disc's only audio language. With the console set to
    // Italian the game stopped before its title screen on the Mac build.
    SetDefaultByName("user_language", "1");

    // Skate 2 rewrites vertex buffers in place between draws of one frame,
    // keeping their address and size: check their pages for CPU writes on
    // every draw.
    SetDefaultByName("vulkan_vertex_buffer_check_every_draw", "true");
    // A guest WAIT_UNTIL for an idle GPU between frames can really wait for
    // the submitted work. Off: the GPU command thread spent half the title
    // screen blocked there, CPU and GPU never overlapped, and on an iPhone
    // frames fell from 30 to 20 and 15.
    SetDefaultByName("vulkan_wait_until_idleclean_sync", "false");

    // MoltenVK has no fragment-input coverage mask, and emulating the EDRAM in
    // the pixel shader is slow on this GPU: host render targets let the
    // hardware do coverage and blending.
    SetDefaultByName("fsi_sample_mask_from_sample_id", "true");
    SetDefaultByName("render_target_path_vulkan", "fbo");

    // Performance defaults for iPhone GPUs.
    //
    // Marking every uploaded page stale each frame re-copied all of guest
    // memory the GPU reads, every frame.
    SetDefaultByName("clear_memory_page_state", "false");
    // One queue submission per frame, not one per command buffer end.
    SetDefaultByName("vulkan_submit_on_primary_buffer_end", "false");
    // FIFO only: vsync-locked, evenly paced frames.
    SetDefaultByName("vulkan_allow_present_mode_immediate", "false");
    SetDefaultByName("vulkan_allow_present_mode_mailbox", "false");
    SetDefaultByName("vulkan_allow_present_mode_fifo_relaxed", "false");
    // MSAA targets backed by single-sampled host images: on a phone GPU the
    // samples multiply pixel, depth and EDRAM transfer work for an edge
    // smoothing barely visible on the small screen.
    SetDefaultByName("vulkan_msaa_as_single_sample", "true");
    // Present frames whose new pipelines are still compiling, without those
    // draws, instead of dropping the whole frame. Compiled pipelines are kept
    // in cache/shaders, so each one is compiled once.
    SetDefaultByName("vulkan_async_skip_incomplete_frames", "false");
#if REX_PLATFORM_IOS
    SetDefaultByName("clock_no_scaling", "false");
#endif
    // Clear rectangles drawn with Direct3D's unbounded 8192 scissor: find their
    // real extent by running the vertex shader on the CPU, as Xenia does.
    SetDefaultByName("execute_unclipped_draw_vs_on_cpu", "true");
    // Apple's tile-based GPU loads and stores the whole render area of every
    // render pass: upload rewritten guest pages between passes, and size the
    // passes to what their draws write.
    SetDefaultByName("vulkan_batch_dirty_uploads", "true");
    SetDefaultByName("vulkan_tight_render_area", "true");
    // Unified memory: write CPU-modified pages straight into the GPU's buffer
    // when no queued work reads them, and move the rest to the start of the
    // submission instead of a copy that ends the render pass.
    SetDefaultByName("vulkan_shared_memory_direct_upload", "true");
    SetDefaultByName("vulkan_defer_cpu_uploads", "true");
    // Room for scheduling hiccups on a busy or warm phone.
    SetDefaultByName("audio_maxqframes", "16");
    // A CPU write to memory the GPU reads invalidates only its own 16 KB page,
    // not the surrounding 1 MB.
    SetDefaultByName("shared_memory_invalidation_widen_kb", "16");
    // Shader memory exports are for the GPU's own later draws; reading each
    // one back for the CPU waits for the GPU every time.
    SetDefaultByName("readback_memexport", "false");
#if REX_PLATFORM_IOS
    // iOS pulls 1024 samples per audio request; call the game's mixer once per
    // 256-sample frame as the console does instead of four times back to back.
    SetDefaultByName("audio_pace_callbacks", "true");
#endif
  }

  rex::ui::FrameStats SampleFrame() {
    using Clock = std::chrono::steady_clock;
    const auto now = Clock::now();
    const double dt_ms = std::chrono::duration<double, std::milli>(now - last_frame_).count();
    // Skip the first interval and any absurd one: after the overlay reopens,
    // "previous" covers the whole time it was closed.
    const bool valid = has_last_frame_ && dt_ms > 0.0 && dt_ms < 1000.0;
    last_frame_ = now;
    has_last_frame_ = true;
    if (valid) {
      smoothed_ms_ = smoothed_ms_ <= 0.0 ? dt_ms : smoothed_ms_ * 0.9 + dt_ms * 0.1;
      stats_.frame_time_ms = smoothed_ms_;
      stats_.fps = smoothed_ms_ > 0.0 ? 1000.0 / smoothed_ms_ : 0.0;
      stats_.frame_count = ++frame_count_;
    }
    return stats_;
  }

  std::filesystem::path game_root_;
  rex::ui::FrameStats stats_{};
  std::chrono::steady_clock::time_point last_frame_{};
  double smoothed_ms_ = 0.0;
  uint64_t frame_count_ = 0;
  bool has_last_frame_ = false;
#endif
};

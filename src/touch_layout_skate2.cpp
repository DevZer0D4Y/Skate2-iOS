// Skate 2's on-screen controls: the whole Xbox 360 pad in one scheme, since
// the game uses the same pad layout on and off the board and in its menus.
//
//   Left stick   steer, walk, menu navigation
//   Right stick  Flick-It: tricks are stick gestures (pull back and flick
//                forward to ollie), so this is a real stick that springs back
//                to centre on release, not a swipe-to-look zone
//   LT / RT      grabs, with the left and right hand
//   A B X Y      push, brake and the rest; A selects and B goes back in menus
//   LB / RB, Back, Start across the top; no D-pad
//
// Sizes are fractions of the screen height; a circle's frame width is scaled
// so it is round on an iPhone's ~2.17:1 landscape screen.

#include "touch_layout_skate2.h"

#include <rex/input/touch/touch_layout.h>

namespace {

using namespace rex::input::touch;

constexpr float kHeightOverWidth = 430.0f / 932.0f;

TouchRect Circle(float centre_x, float centre_y, float diameter) {
  const float width = diameter * kHeightOverWidth;
  return {centre_x - width * 0.5f, centre_y - diameter * 0.5f, width, diameter};
}

TouchControlDefinition Button(const char* id, TouchAction action, const char* label,
                              const char* icon, const TouchRect& frame, uint8_t priority) {
  TouchControlDefinition control;
  control.identifier = id;
  control.type = TouchControlType::kActionButton;
  control.shape = TouchControlShape::kCircle;
  control.normalized_frame = frame;
  control.activation_radius = 0.5f;
  control.analog_tuning.activation_radius = control.activation_radius;
  control.visual_opacity = 0.9f;
  control.capture_priority = priority;
  ConfigureTouchControlAction(action, &control);
  control.label = label;
  control.icon = icon;
  return control;
}

TouchControlDefinition Stick(const char* id, TouchAction action, const TouchRect& frame) {
  TouchControlDefinition stick;
  stick.identifier = id;
  stick.type = TouchControlType::kMoveStick;
  stick.action = action;
  stick.shape = TouchControlShape::kCircle;
  stick.normalized_frame = frame;
  stick.deadzone = 0.10f;
  stick.activation_radius = 0.48f;
  stick.analog_tuning.deadzone = stick.deadzone;
  stick.analog_tuning.activation_radius = stick.activation_radius;
  stick.visual_opacity = 0.85f;
  stick.label_hidden = true;
  stick.capture_priority = 220;
  return stick;
}

TouchLayoutTheme Theme() {
  TouchLayoutTheme theme;
  theme.face = 0x26262AE6;
  theme.face_edge = 0x0A0A0CE6;
  theme.rim = 0xF2B705FF;  // grip-tape yellow
  theme.rim_active = 0xFFE15AFF;
  theme.glyph = 0xF4F4F4FF;
  theme.stick_face = 0x8A8A9050;
  theme.knob = 0x141416F2;
  theme.rim_width = 2.5f;
  return theme;
}

TouchLayoutModel Layout() {
  TouchLayoutModel layout;
  layout.layout_id = "skate2";
  layout.display_name = "Skate 2";
  layout.theme = Theme();
  auto& c = layout.controls;

  // Sticks low in both corners, under the thumbs.
  c.push_back(Stick("left_stick", TouchAction::kMove, Circle(0.125f, 0.730f, 0.40f)));
  c.push_back(Stick("right_stick", TouchAction::kLook, Circle(0.700f, 0.760f, 0.40f)));

  // Face buttons right of the Flick-It stick, in the pad's diamond.
  constexpr float kFaceX = 0.905f, kFaceY = 0.560f, kStep = 0.150f, kSize = 0.150f;
  const float face_dx = kStep * kHeightOverWidth;
  c.push_back(Button("a", TouchAction::kButtonA, "A", "", Circle(kFaceX, kFaceY + kStep, kSize), 240));
  c.push_back(Button("b", TouchAction::kButtonB, "B", "", Circle(kFaceX + face_dx, kFaceY, kSize), 240));
  c.push_back(Button("x", TouchAction::kButtonX, "X", "", Circle(kFaceX - face_dx, kFaceY, kSize), 240));
  c.push_back(Button("y", TouchAction::kButtonY, "Y", "", Circle(kFaceX, kFaceY - kStep, kSize), 240));

  // Grabs: held while the thumbs work the sticks, so they sit beside them.
  c.push_back(Button("lt", TouchAction::kLeftTrigger, "LT", "", Circle(0.285f, 0.540f, 0.150f), 238));
  c.push_back(Button("rt", TouchAction::kRightTrigger, "RT", "", Circle(0.540f, 0.540f, 0.150f), 238));

  // Top row: bumpers outside, Back and Start in the middle.
  c.push_back(Button("lb", TouchAction::kLeftBumper, "LB", "", Circle(0.080f, 0.110f, 0.130f), 245));
  c.push_back(Button("rb", TouchAction::kRightBumper, "RB", "", Circle(0.920f, 0.110f, 0.130f), 245));
  c.push_back(Button("back", TouchAction::kBack, "", "sf:arrow.counterclockwise",
                     Circle(0.455f, 0.090f, 0.110f), 250));
  c.push_back(Button("start", TouchAction::kStart, "", "sf:pause.fill",
                     Circle(0.545f, 0.090f, 0.110f), 250));
  return layout;
}

}  // namespace

void Skate2InstallTouchLayout() { GetTouchRuntimeModel().SetLayout(Layout()); }

#pragma once

#include <rex/ppc/context.h>

// Reads skate2_frame_rate once, before the guest starts. The generated
// instruction hook below only reads the resulting value.
void ConfigureSkate2FrameRate();

// C++ linkage and PPCRegister& match ReXGlue's generated mid-assembly hook ABI.
void Skate2PresentationInterval(PPCRegister& r11);

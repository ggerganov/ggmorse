#pragma once

#include <string>

class GGMorse;

// GGMorse helpers

void GGMorse_setDefaultCaptureDeviceName(std::string name);
bool GGMorse_init(const int playbackId, const int captureId, const float sampleRateOffset = 0);
GGMorse *& GGMorse_instance();
bool GGMorse_mainLoop();
bool GGMorse_deinit();

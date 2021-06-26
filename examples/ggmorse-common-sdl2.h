#pragma once

#include <string>
#include <memory>

class GGMorse;

// GGMorse helpers

void GGMorse_setDefaultCaptureDeviceName(std::string name);
bool GGMorse_init(const int playbackId, const int captureId);
std::shared_ptr<GGMorse> GGMorse_instance();
bool GGMorse_mainLoop();
bool GGMorse_deinit();

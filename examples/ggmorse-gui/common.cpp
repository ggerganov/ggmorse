#include "common.h"

#include "ggmorse-common.h"

#include "ggmorse/ggmorse.h"

#include "malloc-count/malloc_count.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <SDL.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(IOS) || defined(ANDROID)
#include "imgui-wrapper/icons_font_awesome.h"
#endif

#ifndef ICON_FA_COGS
#include "icons_font_awesome.h"
#endif

namespace {

std::mutex g_mutex;
char * toTimeString(const std::chrono::system_clock::time_point & tp) {
    std::lock_guard<std::mutex> lock(g_mutex);

    time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm * ptm = std::localtime(&t);
    static char buffer[32];
    std::strftime(buffer, 32, "%H:%M:%S", ptm);
    return buffer;
}

bool ScrollWhenDraggingOnVoid(const ImVec2& delta, ImGuiMouseButton mouse_button) {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* window = g.CurrentWindow;
    bool hovered = false;
    bool held = false;
    bool dragging = false;
    ImGuiButtonFlags button_flags = (mouse_button == 0) ? ImGuiButtonFlags_MouseButtonLeft : (mouse_button == 1) ? ImGuiButtonFlags_MouseButtonRight : ImGuiButtonFlags_MouseButtonMiddle;
    if (g.HoveredId == 0) // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!)
        ImGui::ButtonBehavior(window->Rect(), window->GetID("##scrolldraggingoverlay"), &hovered, &held, button_flags);
    if (held && delta.x != 0.0f) {
        ImGui::SetScrollX(window, window->Scroll.x + delta.x);
    }
    if (held && delta.y != 0.0f) {
        ImGui::SetScrollY(window, window->Scroll.y + delta.y);
        dragging = true;
    }
    return dragging;
}

}

namespace ColorMap {
enum Type : int8_t {
    Gray,
    Jet,
    Hot,
    HSV,
    Green,
    Ggew,
};

static const std::map<Type, const char *> kTypeString = {
    { Gray,     "Gray" },
    { Jet,      "Jet"  },
    { Hot,      "Hot"  },
    { HSV,      "HSV"  },
    { Green,    "Green"  },
    { Ggew,     "Ggew"  },
};

const int kNColors = 5;

const std::map<Type, std::array<float [3], kNColors>> kColorMapColors = {
    {
        Gray,
        { { { 0.00, 0.00, 0.00 }, { 0.25, 0.25, 0.25 }, { 0.50, 0.50, 0.50 }, { 0.75, 0.75, 0.75 }, { 1.00, 1.00, 1.00 } } }
    },
    {
        Jet,
        { { { 0.00, 0.00, 1.00 }, { 0.00, 1.00, 1.00 }, { 0.00, 1.00, 0.00 }, { 1.00, 1.00, 0.00 }, { 1.00, 0.00, 0.00 } } }
    },
    {
        Hot,
        { { { 0.00, 0.00, 0.00 }, { 1.00, 0.00, 0.00 }, { 1.00, 0.50, 0.00 }, { 1.00, 1.00, 0.00 }, { 1.00, 1.00, 1.00 } } }
    },
    {
        HSV,
        { { { 1.00, 0.00, 0.00 }, { 1.00, 1.00, 0.00 }, { 0.00, 1.00, 1.00 }, { 1.00, 0.00, 1.00 }, { 1.00, 0.00, 0.00 } } }
    },
    {
        Green,
        { { { 0.00, 0.00, 0.00 }, { 0.00, 0.25, 0.00 }, { 0.00, 0.50, 0.00 }, { 0.00, 0.75, 0.00 }, { 0.00, 1.00, 0.00 } } }
    },
    {
        Ggew,
        { { { 0.00, 0.00, 0.00 }, { 0.00, 0.50, 0.00 }, { 0.00, 1.00, 0.00 }, { 0.50, 1.00, 0.50 }, { 1.00, 1.00, 1.00 } } }
    },
};

ImVec4 getColor(Type type, float v) {
    ImVec4 c = { 1.0f, 1.0f, 1.0f, 1.0f };

    const std::array<float [3], kNColors> & color = kColorMapColors.at(type);

    int idx1 = 0;
    int idx2 = 0;
    float fractBetween = 0;

    if (v <= 0) {
        idx1 = idx2 = 0;
    } else if (v >= 1) {
        idx1 = idx2 = kNColors-1;
    } else {
        v = v * (kNColors - 1);
        idx1 = (int) (v);
        idx2 = idx1 + 1;
        fractBetween = v - float(idx1);
    }

    c.x = (color[idx2][0] - color[idx1][0])*fractBetween + color[idx1][0];
    c.y = (color[idx2][1] - color[idx1][1])*fractBetween + color[idx1][1];
    c.z = (color[idx2][2] - color[idx1][2])*fractBetween + color[idx1][2];

    return c;
}
}

namespace ImGui {
bool ButtonDisabled(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    {
        auto col = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        col.x *= 0.8;
        col.y *= 0.8;
        col.z *= 0.8;
        PushStyleColor(ImGuiCol_Button, col);
        PushStyleColor(ImGuiCol_ButtonHovered, col);
        PushStyleColor(ImGuiCol_ButtonActive, col);
    }
    {
        auto col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        col.x *= 0.75;
        col.y *= 0.75;
        col.z *= 0.75;
        PushStyleColor(ImGuiCol_Text, col);
    }
    bool result = Button(label, size);
    PopStyleColor(4);
    return result;
}

bool ButtonDisablable(const char* label, const ImVec2& size = ImVec2(0, 0), bool isDisabled = false) {
    if (isDisabled) {
        ButtonDisabled(label, size);
        return false;
    }
    return Button(label, size);
}

bool ButtonSelected(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    auto col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
    PushStyleColor(ImGuiCol_Button, col);
    PushStyleColor(ImGuiCol_ButtonHovered, col);
    bool result = Button(label, size);
    PopStyleColor(2);
    return result;
}

bool ButtonSelectable(const char* label, const ImVec2& size = ImVec2(0, 0), bool isSelected = false) {
    if (isSelected) return ButtonSelected(label, size);
    return Button(label, size);
}

}

struct GGMorseStats {
    int samplesPerFrame;
    float sampleRateInp;
    float sampleRateOut;
    float sampleRateBase;
    int sampleSizeBytesInp;
    int sampleSizeBytesOut;

    GGMorse::Statistics statistics;
};

struct State {
    bool update = false;

    struct Flags {
        bool newStats = false;
        bool newSpectrogram = false;
        bool newRxData = false;
        bool newSignalF = false;

        void clear() { memset(this, 0, sizeof(Flags)); }
    } flags;

    void apply(State & dst) {
        if (update == false) return;

        if (this->flags.newStats) {
            dst.update = true;
            dst.flags.newStats = true;
            dst.stats = std::move(this->stats);
        }

        if (this->flags.newSpectrogram) {
            dst.update = true;
            dst.flags.newSpectrogram = true;
            dst.rxSpectrogram = std::move(this->rxSpectrogram);
        }

        if (this->flags.newRxData) {
            dst.update = true;
            dst.flags.newRxData = true;
            dst.rxData = std::move(this->rxData);
        }

        if (this->flags.newSignalF) {
            dst.update = true;
            dst.flags.newSignalF = true;
            dst.signalF = std::move(this->signalF);
        }

        flags.clear();
        update = false;
    }

    GGMorseStats stats;
    GGMorse::Spectrogram rxSpectrogram;
    GGMorse::TxRx rxData;
    GGMorse::SignalF signalF;
};

struct Input {
    bool update = true;

    struct Flags {
        bool needReinit = false;
        bool newParametersDecode = false;

        void clear() { memset(this, 0, sizeof(Flags)); }
    } flags;

    void apply(Input & dst) {
        if (update == false) return;

        if (this->flags.needReinit) {
            dst.update = true;
            dst.flags.needReinit = true;
            dst.sampleRateOffset = std::move(this->sampleRateOffset);
        }

        if (this->flags.newParametersDecode) {
            dst.update = true;
            dst.flags.newParametersDecode = true;
            dst.parametersDecode = std::move(this->parametersDecode);
        }

        flags.clear();
        update = false;
    }

    // reinit
    float sampleRateOffset = 0.0f;
    GGMorse::ParametersDecode parametersDecode;
};

struct Buffer {
    std::mutex mutex;

    State stateCore;
    Input inputCore;

    State stateUI;
    Input inputUI;
};

std::atomic<bool> g_isRunning;
Buffer g_buffer;

std::thread initMainAndRunCore() {
    initMain();

    return std::thread([&]() {
        while (g_isRunning) {
            updateCore();

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
}

void initMain() {
    g_isRunning = true;
}

void updateCore() {
    static Input inputCurrent;

    static bool isFirstCall = true;
    static auto & ggMorse = GGMorse_instance();
    static int rxDataLengthLast = 0;
    static GGMorse::TxRx rxDataLast;
    static int signalFLengthLast = 0;
    static GGMorse::SignalF signalFLast;

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        g_buffer.inputCore.apply(inputCurrent);
    }

    if (inputCurrent.update) {
        if (inputCurrent.flags.needReinit) {
            static auto sampleRateInpOld = ggMorse->getSampleRateInp();
            static auto sampleRateOutOld = ggMorse->getSampleRateOut();
            GGMorse::SampleFormat sampleFormatInpOld = ggMorse->getSampleFormatInp();
            GGMorse::SampleFormat sampleFormatOutOld = ggMorse->getSampleFormatOut();

            if (ggMorse) delete ggMorse;

            ggMorse = new GGMorse({
                sampleRateInpOld,
                sampleRateOutOld + inputCurrent.sampleRateOffset,
                GGMorse::kDefaultSamplesPerFrame,
                sampleFormatInpOld,
                sampleFormatOutOld});
        }

        if (inputCurrent.flags.newParametersDecode) {
            ggMorse->setParametersDecode(inputCurrent.parametersDecode);
        }

        inputCurrent.flags.clear();
        inputCurrent.update = false;
    }

    GGMorse_mainLoop();

    if (true) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newStats = true;
        g_buffer.stateCore.stats.samplesPerFrame = ggMorse->getSamplesPerFrame();
        g_buffer.stateCore.stats.sampleRateInp = ggMorse->getSampleRateInp();
        g_buffer.stateCore.stats.sampleRateOut = ggMorse->getSampleRateOut();
        g_buffer.stateCore.stats.sampleRateBase = GGMorse::kBaseSampleRate;
        g_buffer.stateCore.stats.sampleSizeBytesInp = ggMorse->getSampleSizeBytesInp();
        g_buffer.stateCore.stats.sampleSizeBytesOut = ggMorse->getSampleSizeBytesOut();
        g_buffer.stateCore.stats.statistics = ggMorse->getStatistics();
    }

    if (ggMorse->lastDecodeResult()) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newSpectrogram = true;
        g_buffer.stateCore.rxSpectrogram = ggMorse->getSpectrogram();
    }

    rxDataLengthLast = ggMorse->takeRxData(rxDataLast);
    if (rxDataLengthLast > 0) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newRxData = true;
        g_buffer.stateCore.rxData = std::move(rxDataLast);
    }

    signalFLengthLast = ggMorse->takeSignalF(signalFLast);
    if (signalFLengthLast > 0) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newSignalF = true;
        g_buffer.stateCore.signalF = std::move(signalFLast);
    }

    if (isFirstCall) {
        g_buffer.stateCore.update = true;

        isFirstCall = false;
    }

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        g_buffer.stateCore.apply(g_buffer.stateUI);
    }
}

void renderMain() {
    auto tFrameStart = std::chrono::high_resolution_clock::now();

    static State stateCurrent;

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        g_buffer.stateUI.apply(stateCurrent);
    }

    enum class WindowId {
        Settings,
        Rx,
        Tx,
    };

    enum class SubWindowIdRx {
        Main,
    };

    struct Settings {
        bool isSampleRateOffset = false;
        float sampleRateOffset = -512.0f;
        float volume = 0.10f;
    };

    static WindowId windowId = WindowId::Rx;
    static WindowId windowIdLast = windowId;
    static SubWindowIdRx subWindowIdRx = SubWindowIdRx::Main;

    static Settings settings;

    const double tHoldContextPopup = 0.2;

    const int kMaxInputSize = 140;
    static char inputBuf[kMaxInputSize];

    static bool doInputFocus = false;
    static bool doSendMessage = false;
    static bool mouseButtonLeftLast = 0;
    static bool isTextInput = false;
    static bool scrollMessagesToBottom = true;
    static bool hasAudioCaptureData = false;
    static bool hasNewMessages = false;
    static bool hasNewSpectrogram = false;
#ifdef __EMSCRIPTEN__
    static bool hasFileSharingSupport = false;
#else
    static bool hasFileSharingSupport = true;
#endif

    static double tStartInput = 0.0;
    static double tEndInput = -100.0;
    static double tStartTx = 0.0;
    static double tLengthTx = 0.0;
    static double tLastFrame = 0.0;

    static std::string rxData = "";

    static GGMorseStats statsCurrent;
    static GGMorse::Spectrogram spectrogramCurrent;
    static GGMorse::SignalF signalFCurrent;

    // keyboard shortcuts:
    if (ImGui::IsKeyPressed(62)) {
        printf("F5 pressed : clear message history\n");
        // todo
    }

    if (ImGui::IsKeyPressed(63)) {
        printf("F6 pressed : delete last message\n");
        // todo
    }

    if (stateCurrent.update) {
        if (stateCurrent.flags.newStats) {
            statsCurrent = std::move(stateCurrent.stats);
        }
        if (stateCurrent.flags.newSpectrogram) {
            spectrogramCurrent = std::move(stateCurrent.rxSpectrogram);
            hasAudioCaptureData = !spectrogramCurrent.empty();
        }
        if (stateCurrent.flags.newRxData) {
            for (const auto & ch : stateCurrent.rxData) {
                rxData += ch;
            }
        }
        if (stateCurrent.flags.newSignalF) {
            signalFCurrent = std::move(stateCurrent.signalF);
        }
        stateCurrent.flags.clear();
        stateCurrent.update = false;
    }

    if (mouseButtonLeftLast == 0 && ImGui::GetIO().MouseDown[0] == 1) {
        ImGui::GetIO().MouseDelta = { 0.0, 0.0 };
    }
    mouseButtonLeftLast = ImGui::GetIO().MouseDown[0];

    const auto& displaySize = ImGui::GetIO().DisplaySize;
    auto& style = ImGui::GetStyle();

    const auto sendButtonText = ICON_FA_PLAY_CIRCLE " Send";
    const double tShowKeyboard = 0.2f;
#if defined(IOS)
    const float statusBarHeight = displaySize.x < displaySize.y ? 2.0f*style.ItemSpacing.y : 0.1f;
#else
    const float statusBarHeight = 0.1f;
#endif
    const float menuButtonHeight = 24.0f + 2.0f*style.ItemSpacing.y;

    const auto & mouse_delta = ImGui::GetIO().MouseDelta;

    ImGui::SetNextWindowPos({ 0, 0, });
    ImGui::SetNextWindowSize(displaySize);
    ImGui::Begin("Main", nullptr,
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoSavedSettings);

    ImGui::InvisibleButton("StatusBar", { ImGui::GetContentRegionAvailWidth(), statusBarHeight });

    if (ImGui::ButtonSelectable(ICON_FA_COGS, { menuButtonHeight, menuButtonHeight }, windowId == WindowId::Settings )) {
        windowId = WindowId::Settings;
    }
    ImGui::SameLine();

    {
        auto posSave = ImGui::GetCursorScreenPos();
        if (ImGui::ButtonSelectable(ICON_FA_MICROPHONE "  Rx", { 0.45f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight }, windowId == WindowId::Rx)) {
            windowId = WindowId::Rx;
        }
        auto radius = 0.3f*ImGui::GetTextLineHeight();
        posSave.x += 2.0f*radius;
        posSave.y += 2.0f*radius;
        ImGui::GetWindowDrawList()->AddCircleFilled(posSave, radius, hasAudioCaptureData ? ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 0.0f, 1.0f }) : ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.0f, 0.0f, 1.0f }), 16);
    }
    ImGui::SameLine();

    {
        auto posSave = ImGui::GetCursorScreenPos();
        if (ImGui::ButtonSelectable(ICON_FA_HEADPHONES "  Tx", { 1.00f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight }, windowId == WindowId::Tx)) {
            windowId = WindowId::Tx;
        }
        auto radius = 0.3f*ImGui::GetTextLineHeight();
        posSave.x += 2.0f*radius;
        posSave.y += 2.0f*radius;
        if (hasNewMessages) {
            ImGui::GetWindowDrawList()->AddCircleFilled(posSave, radius, ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.0f, 0.0f, 1.0f }), 16);
        }
    }

    if ((windowIdLast != windowId) || (hasAudioCaptureData == false)) {
        windowIdLast = windowId;
    }

    if (windowId == WindowId::Settings) {
        ImGui::BeginChild("Settings:main", ImGui::GetContentRegionAvail(), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::Text("GGMorse v0.1.0");
        ImGui::Separator();

        ImGui::Text("%s", "");
        ImGui::Text("Sample rate (capture):  %g, %d B/sample", statsCurrent.sampleRateInp, statsCurrent.sampleSizeBytesInp);
        ImGui::Text("Sample rate (playback): %g, %d B/sample", statsCurrent.sampleRateOut, statsCurrent.sampleSizeBytesOut);

        const float kLabelWidth = ImGui::CalcTextSize("Inp. SR Offset:  ").x;

        // volume
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            if (settings.volume < 0.2f) {
                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 0.5f }, "Normal volume");
            } else if (settings.volume < 0.5f) {
                ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 0.5f }, "Intermediate volume");
            } else {
                ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 0.5f }, "Warning: high volume!");
            }
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Volume: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            auto p0 = ImGui::GetCursorScreenPos();

            {
                auto & cols = ImGui::GetStyle().Colors;
                ImGui::PushStyleColor(ImGuiCol_FrameBg, cols[ImGuiCol_WindowBg]);
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, cols[ImGuiCol_WindowBg]);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, cols[ImGuiCol_WindowBg]);
                ImGui::SliderFloat("##volume", &settings.volume, 0.0f, 1.0f);
                ImGui::PopStyleColor(3);
            }

            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::SameLine();
            auto p1 = ImGui::GetCursorScreenPos();
            p1.x -= ImGui::CalcTextSize(" ").x;
            p1.y += ImGui::GetTextLineHeightWithSpacing() + 0.5f*style.ItemInnerSpacing.y;
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                    p0, { 0.35f*(p0.x + p1.x), p1.y },
                    ImGui::ColorConvertFloat4ToU32({0.0f, 1.0f, 0.0f, 0.5f}),
                    ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 0.0f, 0.3f}),
                    ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 0.0f, 0.3f}),
                    ImGui::ColorConvertFloat4ToU32({0.0f, 1.0f, 0.0f, 0.5f})
                    );
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                    { 0.35f*(p0.x + p1.x), p0.y }, p1,
                    ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 0.0f, 0.3f}),
                    ImGui::ColorConvertFloat4ToU32({1.0f, 0.0f, 0.0f, 0.5f}),
                    ImGui::ColorConvertFloat4ToU32({1.0f, 0.0f, 0.0f, 0.5f}),
                    ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 0.0f, 0.3f})
                    );
            ImGui::SetCursorScreenPos(posSave);
        }

        {
            ImGui::Text("%s", "");
            ImGui::Text("Estimated Pitch:  %6.2f Hz", statsCurrent.statistics.estimatedPitch_Hz);
            ImGui::Text("Estimated Speed:  %6.2f WPM", statsCurrent.statistics.estimatedSpeed_wpm);
            ImGui::Text("Signal threshold: %6.2f", statsCurrent.statistics.signalThreshold);
            ImGui::Text("%s", "");
            ImGui::Text("Time to resample input:    %6.2f ms", statsCurrent.statistics.timeResample_ms);
            ImGui::Text("Time for pitch detection:  %6.2f ms", statsCurrent.statistics.timePitchDetection_ms);
            ImGui::Text("Time for Goertzel filter:  %6.2f ms", statsCurrent.statistics.timeGoertzel_ms);
            ImGui::Text("Time for frame analysis:   %6.2f ms", statsCurrent.statistics.timeFrameAnalysis_ms);
            ImGui::Text("Time to draw last frame:   %6.2f ms", tLastFrame);
            ImGui::Text("%s", "");
            ImGui::Text("Application framerate: %6.2f fps", ImGui::GetIO().Framerate);
        }

        ScrollWhenDraggingOnVoid(ImVec2(0.0f, -mouse_delta.y), ImGuiMouseButton_Left);

        ImGui::EndChild();
    }

    if (windowId == WindowId::Tx) {
        ImGui::BeginChild("Tx:main", ImGui::GetContentRegionAvail(), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::Text("Not implemented yet ...");
        ImGui::EndChild();
    }

    if (windowId == WindowId::Rx) {
        if (hasAudioCaptureData == false) {
            ImGui::Text("%s", "");
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "No capture data available!");
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Please make sure you have allowed microphone access for this app.");
        } else {
            {
                const int nBins = spectrogramCurrent[0].size()/2;
                const float df = 0.5*statsCurrent.sampleRateInp/nBins;

                static int binMin = 100.0f/df;
                static int binMax = 1300.0f/df;

                static float intensityScale = 30.0f;
                static float rxDataHeight = 3.0f;
                static float statsHeight = ImGui::GetTextLineHeight();
                static float signalHeight = 3*statsHeight;
                static float frequencyMarkerSize = 0.5f*ImGui::CalcTextSize("A").x;
                static float frequencySelected_hz = 550.0f;
                static float speedSelected_wpm = 25.0f;

                static ColorMap::Type colorMap = ColorMap::Type::Ggew;

                static bool showStats = true;
                static bool showSignal = true;
                static bool isHoldingDown = false;
                static bool isContextMenuOpen = false;
                static bool isFrequencyAuto = true;
                static bool isSpeedAuto = true;

                auto p0 = ImGui::GetCursorScreenPos();
                auto mainSize = ImGui::GetContentRegionAvail();
                mainSize.x += frequencyMarkerSize;
                mainSize.y -= rxDataHeight*ImGui::GetTextLineHeightWithSpacing() + 2.0f*style.ItemSpacing.y;

                auto itemSpacingSave = style.ItemSpacing;
                style.ItemSpacing.x = 0.0f;
                style.ItemSpacing.y = 0.0f;

                auto windowPaddingSave = style.WindowPadding;
                style.WindowPadding.x = 0.0f;
                style.WindowPadding.y = 0.0f;

                auto childBorderSizeSave = style.ChildBorderSize;
                style.ChildBorderSize = 0.0f;

                ImGui::BeginChild("Rx:main", mainSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                const int nx = spectrogramCurrent.size();
                const int ny = binMax - binMin;

                float sum = 0.0;
                for (int i = 0; i < nx; ++i) {
                    for (int j = 0; j < ny; ++j) {
                        sum += spectrogramCurrent[i][binMin + j];
                    }
                }

                sum /= (nx*ny);
                if (sum == 0.0) sum = 1.0;

                auto wSize = ImGui::GetContentRegionAvail();
                wSize.x -= frequencyMarkerSize;

                const float dx = wSize.x/nx;
                const float dy = wSize.y/ny;

                int nChildWindows = 0;
                int nFreqPerChild = 32;
                ImGui::PushID(nChildWindows++);
                ImGui::BeginChild("Spectrogram", { wSize.x + frequencyMarkerSize, (nFreqPerChild + 1)*dy }, true);
                auto drawList = ImGui::GetWindowDrawList();
                auto drawList0 = ImGui::GetWindowDrawList();

                for (int j = 0; j < ny; ++j) {
                    if (j > 0 && j % nFreqPerChild == 0) {
                        ImGui::EndChild();
                        ImGui::PopID();

                        ImGui::PushID(nChildWindows++);
                        ImGui::SetCursorScreenPos({ p0.x, p0.y + nFreqPerChild*int(j/nFreqPerChild)*dy });
                        ImGui::BeginChild("Spectrogram", { wSize.x + frequencyMarkerSize, (nFreqPerChild + 1)*dy }, true);
                        drawList = ImGui::GetWindowDrawList();
                    }

                    int k = binMin + j;
                    for (int i = 0; i < nx; ++i) {
                        auto c0 = ImGui::ColorConvertFloat4ToU32(getColor(colorMap, spectrogramCurrent[i][k]/(intensityScale*sum)));
                        drawList->AddRectFilled({ p0.x + i*dx, p0.y + j*dy }, { p0.x + i*dx + dx, p0.y + j*dy + dy }, c0);

                        //auto c1 =               i < nx - 1 ? ImGui::ColorConvertFloat4ToU32(getColor(ColorMap::Ggew, spectrogramCurrent[i + 1][k]/(intensityScale*sum))) : c0;
                        //auto c2 = i < nx - 1 && j < ny - 1 ? ImGui::ColorConvertFloat4ToU32(getColor(ColorMap::Ggew, spectrogramCurrent[i + 1][k + 1]/(intensityScale*sum))) : c0;
                        //auto c3 =               j < ny - 1 ? ImGui::ColorConvertFloat4ToU32(getColor(ColorMap::Ggew, spectrogramCurrent[i][k + 1]/(intensityScale*sum))) : c0;
                        //drawList->AddRectFilledMultiColor({ p0.x + i*dx, p0.y + j*dy }, { p0.x + i*dx + dx, p0.y + j*dy + dy }, c0, c1, c2, c3);
                    }

                    const auto & f = statsCurrent.statistics.estimatedPitch_Hz;
                    if (f >= k*df && f < (k + 1)*df) {
                        drawList0->AddTriangleFilled({ p0.x + wSize.x,                  p0.y + j*dy + 0.5f*dy },
                                                     { p0.x + wSize.x + frequencyMarkerSize, p0.y + j*dy + 0.5f*dy - 0.5f*frequencyMarkerSize },
                                                     { p0.x + wSize.x + frequencyMarkerSize, p0.y + j*dy + 0.5f*dy + 0.5f*frequencyMarkerSize },
                                                     ImGui::ColorConvertFloat4ToU32({ 1.0f, 1.0f, 0.0f, 1.0f }));
                    }
                }

                ImGui::EndChild();
                ImGui::PopID();

                ImGui::SetCursorScreenPos(p0);
                ImGui::BeginChild("Stats", { wSize.x + frequencyMarkerSize, mainSize.y }, true);
                if (showSignal) {
                    ImGui::SetCursorScreenPos({ p0.x, p0.y + wSize.y - signalHeight });
                    ImGui::PlotHistogram("##signal", signalFCurrent.data(), signalFCurrent.size(), 0, NULL, FLT_MAX, FLT_MAX, { wSize.x, signalHeight });
                    drawList->AddLine({ p0.x, p0.y + wSize.y - statsCurrent.statistics.signalThreshold*signalHeight },
                                       { p0.x + wSize.x, p0.y + wSize.y - statsCurrent.statistics.signalThreshold*signalHeight },
                                       ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.0f, 0.0f, 0.75f }));
                }

                if (showStats) {
                    ImGui::SetCursorScreenPos({ p0.x + 0.5f*itemSpacingSave.x, p0.y + wSize.y - statsHeight });
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.back());
                    ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "F: %6.1f Hz | S: %2.0f WPM | FPS: %4.1f | R: %4.1f ms",
                                       statsCurrent.statistics.estimatedPitch_Hz,
                                       statsCurrent.statistics.estimatedSpeed_wpm,
                                       ImGui::GetIO().Framerate,
                                       tLastFrame
                                      );
                    ImGui::PopFont();
                }
                ImGui::EndChild();

                ImGui::EndChild();

                style.ItemSpacing = itemSpacingSave;
                style.WindowPadding = windowPaddingSave;
                style.ChildBorderSize = childBorderSizeSave;

                if (!isContextMenuOpen) {
                    auto p1 = p0;
                    p1.x += mainSize.x;
                    p1.y += mainSize.y;

                    if (ImGui::IsMouseHoveringRect(p0, p1, true)) {
                        if (ImGui::GetIO().MouseDownDuration[0] > tHoldContextPopup) {
                            isHoldingDown = true;
                        }
                    }
                }

                if (ImGui::IsMouseReleased(0) && isHoldingDown) {
                    auto pos = ImGui::GetMousePos();
                    ImGui::SetNextWindowPos(pos);

                    ImGui::OpenPopup("Message options");
                    isHoldingDown = false;
                    isContextMenuOpen = true;
                }

                if (ImGui::BeginPopup("Message options")) {
                    ImGui::TextDisabled("Advanced settings");
                    ImGui::Separator();
                    ImGui::PushItemWidth(0.5*mainSize.x);
                    static char buf[64];
                    snprintf(buf, 64, "Bin: %3d, Freq: %5.1f Hz", binMin, 0.5*binMin*statsCurrent.sampleRateInp/nBins);
                    ImGui::DragInt("##binMin", &binMin, 1, 0, binMax - 2, buf);
                    snprintf(buf, 64, "Bin: %3d, Freq: %5.1f Hz", binMax, 0.5*binMax*statsCurrent.sampleRateInp/nBins);
                    ImGui::DragInt("##binMax", &binMax, 1, binMin + 1, nBins, buf);
                    ImGui::DragFloat("##intensityScale", &intensityScale, 1.0f, 1.0f, 1000.0f, "Intensity scale: %.1f", 1.2f);
                    if (ImGui::BeginCombo("##colormap", ColorMap::kTypeString.at(colorMap))) {
                        for (int i = 0; i < (int) ColorMap::kTypeString.size(); ++i) {
                            const bool isSelected = (colorMap == i);
                            if (ImGui::Selectable(ColorMap::kTypeString.at(ColorMap::Type(i)), isSelected)) {
                                colorMap = ColorMap::Type(i);
                            }

                            if (isSelected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::DragFloat("##height", &rxDataHeight, 0.1f, 1.0f, 10.0f, "Rx height: %.1f", 1.0f);

                    if (isFrequencyAuto) {
                        frequencySelected_hz = statsCurrent.statistics.estimatedPitch_Hz;
                    }
                    if (ImGui::DragFloat("##frequency", &frequencySelected_hz, 1.0f, 200.0f, 1200.0f, "Frequency: %.1f Hz", 1.0f)) {
                        isFrequencyAuto = false;
                        g_buffer.inputUI.flags.newParametersDecode = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("Auto##frequency", &isFrequencyAuto)) {
                        g_buffer.inputUI.flags.newParametersDecode = true;
                    }

                    if (isSpeedAuto) {
                        speedSelected_wpm = statsCurrent.statistics.estimatedSpeed_wpm;
                    }
                    if (ImGui::DragFloat("##speed", &speedSelected_wpm, 1.0f, 5.0f, 55.0f, "Speed: %.0f wpm", 1.0f)) {
                        isSpeedAuto = false;
                        g_buffer.inputUI.flags.newParametersDecode = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("Auto##speed", &isSpeedAuto)) {
                        g_buffer.inputUI.flags.newParametersDecode = true;
                    }

                    ImGui::Checkbox("Show signal", &showSignal);
                    ImGui::Checkbox("Show stats", &showStats);
                    ImGui::PopItemWidth();

                    ImGui::EndPopup();
                } else {
                    isContextMenuOpen = false;
                }

                if (g_buffer.inputUI.flags.newParametersDecode) {
                    g_buffer.inputUI.update = true;
                    g_buffer.inputUI.parametersDecode.frequency_hz = isFrequencyAuto ? -1.0f : frequencySelected_hz;
                    g_buffer.inputUI.parametersDecode.speed_wpm = isSpeedAuto ? -1.0f : speedSelected_wpm;
                }
            }

            {
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.back());
                ImGui::BeginChild("Rx:data", ImGui::GetContentRegionAvail(), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvailWidth());
                ImGui::Text("%s", rxData.c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetScrollY(ImGui::GetScrollMaxY() - style.ItemSpacing.y);
                ImGui::EndChild();
                ImGui::PopFont();
            }
        }
    }

    ImGui::End();

    ImGui::GetIO().KeysDown[ImGui::GetIO().KeyMap[ImGuiKey_Backspace]] = false;
    ImGui::GetIO().KeysDown[ImGui::GetIO().KeyMap[ImGuiKey_Enter]] = false;

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        g_buffer.inputUI.apply(g_buffer.inputCore);
    }

    auto tFrameEnd = std::chrono::high_resolution_clock::now();
    tLastFrame = getTime_ms(tFrameStart, tFrameEnd);
}

void deinitMain() {
    g_isRunning = false;
}

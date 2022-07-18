#include "common.h"
#include "interface.h"

#include "ggmorse-common.h"

#include "ggmorse/ggmorse.h"

#include "malloc-count/malloc_count.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#if defined(__APPLE__)
#define SDL_DISABLE_ARM_NEON_H 1
#endif
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

void ImGui_TextCentered(const char * text, bool disabled) {
    const auto w0 = ImGui::GetContentRegionAvailWidth();
    const auto w1 = ImGui::CalcTextSize(text).x;
    const auto p0 = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos({ p0.x + 0.5f*(w0 - w1), p0.y });
    if (disabled) {
        ImGui::TextDisabled("%s", text);
    } else {
        ImGui::Text("%s", text);
    }
}

std::mutex g_mutex;
[[maybe_unused]] char * toTimeString(const std::chrono::system_clock::time_point & tp) {
    std::lock_guard<std::mutex> lock(g_mutex);

    time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm * ptm = std::localtime(&t);
    static char buffer[32];
    std::strftime(buffer, 32, "%H:%M:%S", ptm);
    return buffer;
}

bool ScrollWhenDraggingOnVoid(const ImVec2 & delta, ImGuiMouseButton mouse_button) {
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
        auto dy = delta.y;
        // fix vertical scroll snapping:
        if (window->Scroll.y == 0.0f && delta.y > 0.0f && delta.y < 11.0f) dy = 11.0f;
        ImGui::SetScrollY(window, window->Scroll.y + dy);
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

struct Message {
    std::chrono::system_clock::time_point timestamp;
    std::string data;
    GGMorse::ParametersEncode parameters;
};

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
        bool newTxWaveform = false;
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

        if (this->flags.newTxWaveform) {
            dst.update = true;
            dst.flags.newTxWaveform = true;
            dst.txWaveform = std::move(this->txWaveform);
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
    GGMorse::WaveformI16 txWaveform;
    GGMorse::TxRx rxData;
    GGMorse::SignalF signalF;
};

struct Input {
    bool update = true;

    struct Flags {
        bool newParametersDecode = false;
        bool newMessage = false;

        void clear() { memset(this, 0, sizeof(Flags)); }
    } flags;

    void apply(Input & dst) {
        if (update == false) return;

        if (this->flags.newParametersDecode) {
            dst.update = true;
            dst.flags.newParametersDecode = true;
            dst.parametersDecode = std::move(this->parametersDecode);
        }

        if (this->flags.newMessage) {
            dst.update = true;
            dst.flags.newMessage = true;
            dst.message = std::move(this->message);
        }

        flags.clear();
        update = false;
    }

    // parametersDecode
    GGMorse::ParametersDecode parametersDecode = GGMorse::getDefaultParametersDecode();

    // message
    Message message;
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
    static int rxDataLengthLast = 0;
    static GGMorse::TxRx rxDataLast;
    static int signalFLengthLast = 0;
    static GGMorse::SignalF signalFLast;

    auto ggMorse = GGMorse_instance();
    if (ggMorse == nullptr) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        g_buffer.inputCore.apply(inputCurrent);
    }

    if (inputCurrent.update) {
        if (inputCurrent.flags.newParametersDecode) {
            ggMorse->setParametersDecode(inputCurrent.parametersDecode);
        }

        if (inputCurrent.flags.newMessage) {
            ggMorse->setParametersEncode(inputCurrent.message.parameters);
            ggMorse->init(
                    (int) inputCurrent.message.data.size(),
                    inputCurrent.message.data.data());
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

    if (ggMorse->takeTxWaveformI16(g_buffer.stateCore.txWaveform)) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newTxWaveform = true;
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
        int nBins = 1;
        int binMin = 0;
        int binMax = 0;
        float df = 1.0f;

        float volume = 0.40f;
        float intensityScale = 30.0f;
        float signalHeight = 2*ImGui::GetTextLineHeightWithSpacing();
        float frequencySelected_hz = 550.0f;
        float speedSelected_wpm = 25.0f;

        ColorMap::Type colorMap = ColorMap::Type::Ggew;

        bool showStats = true;
        bool showSignal = true;
        bool isFrequencyAuto = GGMorse::getDefaultParametersDecode().frequency_hz < 0.0;
        bool isSpeedAuto = GGMorse::getDefaultParametersDecode().speed_wpm < 0.0;
        bool applyFilterHighPass = GGMorse::getDefaultParametersDecode().applyFilterHighPass;
        bool applyFilterLowPass = GGMorse::getDefaultParametersDecode().applyFilterHighPass;

        bool txRepeat = false;
        float txFrequency_hz = 550.0f;
        int txSpeedCharacters_wpm = 25;
        int txSpeedFarnsworth_wpm = 25;
    };

    static WindowId windowId = WindowId::Rx;
    static WindowId windowIdLast = windowId;
    [[maybe_unused]] static SubWindowIdRx subWindowIdRx = SubWindowIdRx::Main;

    static Settings settings;

    const double tHoldContextPopup = 0.2;

    const int kMaxInputSize = 140;
    static char inputBuf[kMaxInputSize];

    static bool doInputFocus = false;
    static bool doSendMessage = false;
    static bool mouseButtonLeftLast = 0;
    static bool isTextInput = false;
    static bool hasAudioCaptureData = false;
    static bool hasNewMessages = false;

    [[maybe_unused]] static double tStartInput = 0.0;
    [[maybe_unused]] static double tEndInput = -100.0;
    static double tStartTx = 0.0;
    static double tLengthTx = 0.0;
    static double tLastFrame = 0.0;

    static std::string rxData = "";

    static GGMorseStats statsCurrent;
    static GGMorse::Spectrogram spectrogramCurrent;
    static GGMorse::WaveformI16 txWaveformCurrent;
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
        if (stateCurrent.flags.newTxWaveform) {
            txWaveformCurrent = std::move(stateCurrent.txWaveform);

            tStartTx = ImGui::GetTime() + (24.0f*1024.0f)/statsCurrent.sampleRateOut;
            tLengthTx = txWaveformCurrent.size()/statsCurrent.sampleRateOut;
            {
                auto & ampl = txWaveformCurrent;
                int nBins = 512;
                int nspb = (int) ampl.size()/nBins;
                for (int i = 0; i < nBins; ++i) {
                    double sum = 0.0;
                    for (int j = 0; j < nspb; ++j) {
                        sum += std::abs(ampl[i*nspb + j]);
                    }
                    ampl[i] = sum/nspb;
                }
                ampl.resize(nBins);
            }
        }
        if (stateCurrent.flags.newRxData) {
            for (const auto & ch : stateCurrent.rxData) {
                rxData += ch;
            }

            if (rxData.size() > 512) {
                rxData = rxData.substr(256);
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
    [[maybe_unused]] const double tShowKeyboard = 0.2f;
#if defined(IOS)
    static float statusBarHeightDevice = getStatusBarHeightDevice();
    const float statusBarHeight = displaySize.x < displaySize.y ? statusBarHeightDevice : 0.1f;
#else
    const float statusBarHeight = 0.1f;
#endif
    const float menuButtonHeight = 1.75f*ImGui::GetTextLineHeightWithSpacing();

    const auto & mouse_delta = ImGui::GetIO().MouseDelta;

    if (settings.isFrequencyAuto) {
        settings.frequencySelected_hz = statsCurrent.statistics.estimatedPitch_Hz;
    }
    if (settings.isSpeedAuto) {
        settings.speedSelected_wpm = statsCurrent.statistics.estimatedSpeed_wpm;
    }

    ImGui::SetNextWindowPos({ 0, 0, });
    ImGui::SetNextWindowSize(displaySize);
    ImGui::Begin("Main", nullptr,
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoSavedSettings);

    if (displaySize.x > displaySize.y) {
        windowId = WindowId::Rx;
    } else {
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
    }

    if ((windowIdLast != windowId) || (hasAudioCaptureData == false)) {
        windowIdLast = windowId;
    }

    if (windowId == WindowId::Settings) {
        ImGui::BeginChild("Settings:main", ImGui::GetContentRegionAvail(), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui_TextCentered("GGMorse v1.3.6", false);
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.back());
        ImGui::Text("%s", "");
        ImGui_TextCentered("created by", true);
        ImGui_TextCentered("  Georgi Gerganov, LZ2ZGJ", true);
        ImGui_TextCentered("  Vladimir Gerganov, LZ2ZG", true);
        ImGui::Text("%s", "");
        ImGui::PopFont();
        ImGui::Separator();

        static char buf[64];
        const float kLabelWidth = ImGui::CalcTextSize("Color Map:   ").x;

        // Rx settings
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("Rx settings");
            ImGui::PopTextWrapPos();
        }

        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Frequency: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            if (ImGui::DragFloat("##rxFrequency", &settings.frequencySelected_hz, 1.0f, 200.0f, 1200.0f, "%.1f Hz", 1.0f)) {
                settings.isFrequencyAuto = false;
                g_buffer.inputUI.flags.newParametersDecode = true;
            }
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            if (ImGui::Checkbox("Auto##rxFrequency", &settings.isFrequencyAuto)) {
                g_buffer.inputUI.flags.newParametersDecode = true;
            }
        }

        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Speed: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            if (ImGui::DragFloat("##speed", &settings.speedSelected_wpm, 1.0f, 5.0f, 55.0f, "%.0f WPM", 1.0f)) {
                settings.isSpeedAuto = false;
                g_buffer.inputUI.flags.newParametersDecode = true;
            }
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            if (ImGui::Checkbox("Auto##speed", &settings.isSpeedAuto)) {
                g_buffer.inputUI.flags.newParametersDecode = true;
            }
        }

        //ImGui::Text("%s", "");
        //{
        //    auto posSave = ImGui::GetCursorScreenPos();
        //    ImGui::Text("%s", "");
        //    ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        //    ImGui::PushTextWrapPos();
        //    ImGui::TextDisabled("High-pass filter: %.2f Hz", settings.binMin*settings.df);
        //    ImGui::PopTextWrapPos();
        //}
        //{
        //    auto posSave = ImGui::GetCursorScreenPos();
        //    ImGui::Text("High-pass");
        //    ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        //}
        //{
        //    if (ImGui::Checkbox("Apply##apply high-pass filter", &settings.applyFilterHighPass)) {
        //        g_buffer.inputUI.flags.newParametersDecode = true;
        //    }
        //}

        //ImGui::Text("%s", "");
        //{
        //    auto posSave = ImGui::GetCursorScreenPos();
        //    ImGui::Text("%s", "");
        //    ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        //    ImGui::PushTextWrapPos();
        //    ImGui::TextDisabled("Low-pass filter: %.2f Hz", settings.binMax*settings.df);
        //    ImGui::PopTextWrapPos();
        //}
        //{
        //    auto posSave = ImGui::GetCursorScreenPos();
        //    ImGui::Text("Low-pass");
        //    ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        //}
        //{
        //    if (ImGui::Checkbox("Apply##apply low-pass filter", &settings.applyFilterLowPass)) {
        //        g_buffer.inputUI.flags.newParametersDecode = true;
        //    }
        //}

        // Tx settings
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("Tx settings");
            ImGui::PopTextWrapPos();
        }

        //ImGui::Text("%s", "");
        //{
        //    auto posSave = ImGui::GetCursorScreenPos();
        //    ImGui::Text("%s", "");
        //    ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        //    if (settings.volume < 0.2f) {
        //        ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 0.5f }, "Normal volume");
        //    } else if (settings.volume < 0.5f) {
        //        ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 0.5f }, "Intermediate volume");
        //    } else {
        //        ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 0.5f }, "Warning: high volume!");
        //    }
        //}
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
                if (ImGui::SliderFloat("##volume", &settings.volume, 0.0f, 1.0f)) {
                    settings.volume = std::min(settings.volume, 1.0f);
                }
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
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Frequency: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            snprintf(buf, 64, "%5.1f Hz", settings.txFrequency_hz);
            ImGui::DragFloat("##txFrequency", &settings.txFrequency_hz, 1, 200, 1900, buf);
        }

        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Speed: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            snprintf(buf, 64, "Characters speed: %2d WPM", settings.txSpeedCharacters_wpm);
            if (ImGui::DragInt("##speedCharacters", &settings.txSpeedCharacters_wpm, 1, 5, 55, buf)) {
                settings.txSpeedFarnsworth_wpm = settings.txSpeedCharacters_wpm;
            }
        }

        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            snprintf(buf, 64, "Farnsworth speed: %2d WPM", settings.txSpeedFarnsworth_wpm);
            ImGui::DragInt("##speedFarnsworth", &settings.txSpeedFarnsworth_wpm, 1, 5, 55, buf);
        }

        // Spectrogam settings
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("Spectrogram settings");
            ImGui::PopTextWrapPos();
        }

        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Min: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            snprintf(buf, 64, "Bin: %3d, Freq: %5.1f Hz", settings.binMin, settings.binMin*settings.df);
            if (ImGui::DragInt("##binMin", &settings.binMin, 1, 0, settings.binMax - 2, buf)) {
                g_buffer.inputUI.flags.newParametersDecode = true;
            }
        }

        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Max: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            snprintf(buf, 64, "Bin: %3d, Freq: %5.1f Hz", settings.binMax, settings.binMax*settings.df);
            if (ImGui::DragInt("##binMax", &settings.binMax, 1, settings.binMin + 1, settings.nBins, buf)) {
                g_buffer.inputUI.flags.newParametersDecode = true;
            }
        }

        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Intensity: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            ImGui::DragFloat("##intensityScale", &settings.intensityScale, 1.0f, 1.0f, 1000.0f, "Scale: %.1f");
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Color Map: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            if (ImGui::BeginCombo("##colormap", ColorMap::kTypeString.at(settings.colorMap))) {
                for (int i = 0; i < (int) ColorMap::kTypeString.size(); ++i) {
                    const bool isSelected = (settings.colorMap == i);
                    if (ImGui::Selectable(ColorMap::kTypeString.at(ColorMap::Type(i)), isSelected)) {
                        settings.colorMap = ColorMap::Type(i);
                    }

                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("Show signal plot");
            ImGui::PopTextWrapPos();
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Signal: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            ImGui::Checkbox("##Show signal", &settings.showSignal);
        }

        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("Show realtime stats");
            ImGui::PopTextWrapPos();
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Stats: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            ImGui::Checkbox("##Show stats", &settings.showStats);
        }

        ImGui::Text("%s", "");
        ImGui::Separator();

        {
            ImGui::Text("%s", "");
            ImGui::TextDisabled("Debug information");
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.back());
            ImGui::Text("%s", "");
            ImGui::Text("Sample rate (capture):  %g, %d B/sample", statsCurrent.sampleRateInp, statsCurrent.sampleSizeBytesInp);
            ImGui::Text("Sample rate (playback): %g, %d B/sample", statsCurrent.sampleRateOut, statsCurrent.sampleSizeBytesOut);
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
            ImGui::Text("Status bar height:     %6.2f px", statusBarHeight);
            ImGui::PopFont();
        }

        ScrollWhenDraggingOnVoid(ImVec2(0.0f, -mouse_delta.y), ImGuiMouseButton_Left);

        ImGui::EndChild();
    }

    if (windowId != WindowId::Settings) {
        static float rxDataHeight = 10.5f;
        static float rxFontScale = 1.0f;

        const float rxDataHeightMax = std::round(ImGui::GetContentRegionAvail().y/ImGui::GetTextLineHeightWithSpacing()) - 5;
        if (rxDataHeightMax > 4) {
            rxDataHeight = std::min(rxDataHeight, rxDataHeightMax);
        }

        if (hasAudioCaptureData == false) {
            ImGui::Text("%s", "");
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "No capture data available!");
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Please make sure you have allowed microphone access for this app.");
        } else {
            {
                settings.nBins = (int) spectrogramCurrent[0].size()/2;
                settings.df = 0.5*statsCurrent.sampleRateBase/settings.nBins;

                if (settings.binMin == 0 && settings.binMax == 0 && settings.nBins > 1) {
                    settings.binMin = std::max(0.0f, g_buffer.inputUI.parametersDecode.frequencyRangeMin_hz - 20.0f)/settings.df;
                    settings.binMax = (g_buffer.inputUI.parametersDecode.frequencyRangeMax_hz + 20.0f)/settings.df;
                }

                static float frequencyMarkerSize = 0.5f*ImGui::CalcTextSize("A").x;
                //static bool isHoldingDown = false;
                //static bool isContextMenuOpen = false;

                const auto p0 = ImGui::GetCursorScreenPos();
                auto mainSize = ImGui::GetContentRegionAvail();
                mainSize.x += frequencyMarkerSize;
                mainSize.y -= rxDataHeight*ImGui::GetTextLineHeightWithSpacing() + 2.0f*style.ItemSpacing.y;

#if defined(IOS) || defined(ANDROID)
                if (displaySize.x < displaySize.y) {
                    if (isTextInput) {
                        mainSize.y -= 0.4f*displaySize.y*std::min(tShowKeyboard, ImGui::GetTime() - tStartInput) / tShowKeyboard;
                    } else {
                        mainSize.y -= 0.4f*displaySize.y - 0.4f*displaySize.y*std::min(tShowKeyboard, ImGui::GetTime() - tEndInput) / tShowKeyboard;
                    }
                }
#endif

                auto itemSpacingSave = style.ItemSpacing;
                style.ItemSpacing.x = 0.0f;
                style.ItemSpacing.y = 0.0f;

                auto windowPaddingSave = style.WindowPadding;
                style.WindowPadding.x = 0.0f;
                style.WindowPadding.y = 0.0f;

                auto childBorderSizeSave = style.ChildBorderSize;
                style.ChildBorderSize = 0.0f;

                ImGui::BeginChild("Rx:main", mainSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                const int nx = (int) spectrogramCurrent.size();
                const int ny = settings.binMax - settings.binMin;

                float sum = 0.0;
                for (int i = 0; i < nx; ++i) {
                    for (int j = 0; j < ny; ++j) {
                        sum += spectrogramCurrent[i][settings.binMin + j];
                    }
                }

                sum /= (nx*ny);
                if (sum == 0.0) sum = 1.0;
                const float iscale = 1.0f/(settings.intensityScale*sum);
                const auto c00 = ImGui::ColorConvertFloat4ToU32(getColor(settings.colorMap, 0.0f));

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

                    int k = settings.binMin + j;
                    for (int i = 0; i < nx; ++i) {
                        auto v = spectrogramCurrent[i][k]*iscale;
                        auto c0 = c00;
                        if (v > 0.05f) {
                            c0 = ImGui::ColorConvertFloat4ToU32(getColor(settings.colorMap, v));
                        }
                        //auto c0 = ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 0.0f, v});
                        drawList->AddRectFilled({ p0.x + i*dx, p0.y + j*dy }, { p0.x + i*dx + dx, p0.y + j*dy + dy }, c0);

                        //auto c1 =               i < nx - 1 ? ImGui::ColorConvertFloat4ToU32(getColor(ColorMap::Ggew, spectrogramCurrent[i + 1][k]/(intensityScale*sum))) : c0;
                        //auto c2 = i < nx - 1 && j < ny - 1 ? ImGui::ColorConvertFloat4ToU32(getColor(ColorMap::Ggew, spectrogramCurrent[i + 1][k + 1]/(intensityScale*sum))) : c0;
                        //auto c3 =               j < ny - 1 ? ImGui::ColorConvertFloat4ToU32(getColor(ColorMap::Ggew, spectrogramCurrent[i][k + 1]/(intensityScale*sum))) : c0;
                        //drawList->AddRectFilledMultiColor({ p0.x + i*dx, p0.y + j*dy }, { p0.x + i*dx + dx, p0.y + j*dy + dy }, c0, c1, c2, c3);
                    }

                    const auto & f = statsCurrent.statistics.estimatedPitch_Hz;
                    if (f >= k*settings.df && f < (k + 1)*settings.df) {
                        drawList0->AddTriangleFilled({ p0.x + wSize.x,                       p0.y + j*dy + 0.5f*dy },
                                                     { p0.x + wSize.x + frequencyMarkerSize, p0.y + j*dy + 0.5f*dy - 0.5f*frequencyMarkerSize },
                                                     { p0.x + wSize.x + frequencyMarkerSize, p0.y + j*dy + 0.5f*dy + 0.5f*frequencyMarkerSize },
                                                     ImGui::ColorConvertFloat4ToU32({ 1.0f, 1.0f, 0.0f, 1.0f }));
                    }
                }

                ImGui::EndChild();
                ImGui::PopID();

                ImGui::SetCursorScreenPos(p0);
                ImGui::BeginChild("Stats", { wSize.x + frequencyMarkerSize, mainSize.y }, true);
                if (settings.showSignal) {
                    ImGui::SetCursorScreenPos({ p0.x, p0.y + wSize.y - settings.signalHeight });
                    ImGui::PlotHistogram("##signal", signalFCurrent.data(), (int) signalFCurrent.size(), 0, NULL, FLT_MAX, FLT_MAX, { wSize.x, settings.signalHeight });
                    drawList->AddLine({ p0.x, p0.y + wSize.y - statsCurrent.statistics.signalThreshold*settings.signalHeight },
                                      { p0.x + wSize.x, p0.y + wSize.y - statsCurrent.statistics.signalThreshold*settings.signalHeight },
                                      ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.0f, 0.0f, 0.75f }));
                }

                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.back());

                const float offset = settings.showSignal ? settings.signalHeight : 0.0f;
                const float lockSize = 1.5f*ImGui::GetTextLineHeightWithSpacing();
                const float statsHeight = 1.0f*ImGui::GetTextLineHeightWithSpacing();
                const bool isLocked = !(settings.isFrequencyAuto && settings.isSpeedAuto);

                if (settings.showStats) {
                    ImGui::SetCursorScreenPos({ p0.x + 1.0f*itemSpacingSave.x + 2*lockSize, p0.y + wSize.y - offset - 2*statsHeight });
                    ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "FPS: %4.1f | R: %4.1f ms",
                                       ImGui::GetIO().Framerate,
                                       tLastFrame
                                      );
                    ImGui::SetCursorScreenPos({ p0.x + 1.0f*itemSpacingSave.x + 2*lockSize, p0.y + wSize.y - offset - 1*statsHeight });
                    ImGui::TextColored(isLocked ? ImVec4 { 0.0f, 1.0f, 0.0f, 1.0f } : ImVec4 { 1.0f, 1.0f, 0.0f, 1.0f }, "F: %6.1f Hz | S: %2.0f WPM",
                                       statsCurrent.statistics.estimatedPitch_Hz,
                                       statsCurrent.statistics.estimatedSpeed_wpm
                                      );
                    ImGui::SameLine();
                    ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, " | C: %5.3f",
                                       statsCurrent.statistics.costFunction
                                      );
                }
                ImGui::PopFont();

                ImGui::SetCursorScreenPos({ p0.x + 0.25f*itemSpacingSave.x, p0.y + wSize.y - offset - 2.0f*lockSize - 0.5f*itemSpacingSave.y});

                const auto iconLock = isLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK_ALT;
                if (ImGui::Button(iconLock, ImVec2 { 2.0f*lockSize, 2.0f*lockSize })) {
                    if (!isLocked) {
                        settings.isFrequencyAuto = false;
                        settings.isSpeedAuto = false;
                        g_buffer.inputUI.flags.newParametersDecode = true;
                    } else {
                        settings.isFrequencyAuto = true;
                        settings.isSpeedAuto = true;
                        g_buffer.inputUI.flags.newParametersDecode = true;
                    }
                }

                ImGui::EndChild();

                ImGui::EndChild();

                style.ItemSpacing = itemSpacingSave;
                style.WindowPadding = windowPaddingSave;
                style.ChildBorderSize = childBorderSizeSave;

                // Disable context menu on spectrogram
                //
                //if (!isContextMenuOpen) {
                //    auto p1 = p0;
                //    p1.x += mainSize.x;
                //    p1.y += mainSize.y;

                //    if (ImGui::IsMouseHoveringRect(p0, p1, true)) {
                //        if (ImGui::GetIO().MouseDownDuration[0] > tHoldContextPopup) {
                //            isHoldingDown = true;
                //        }
                //    }
                //}

                //if (ImGui::IsMouseReleased(0) && isHoldingDown) {
                //    auto pos = ImGui::GetMousePos();
                //    ImGui::SetNextWindowPos(pos);

                //    ImGui::OpenPopup("Main Options");
                //    isHoldingDown = false;
                //    isContextMenuOpen = true;
                //}

                //if (ImGui::BeginPopup("Main Options")) {
                //    ImGui::EndPopup();
                //} else {
                //    isContextMenuOpen = false;
                //}
            }

            {
                static bool isContextMenuOpen = false;
                static bool isHoldingDown = false;

                const auto p0 = ImGui::GetCursorScreenPos();
                auto mainSize = ImGui::GetContentRegionAvail();
                mainSize.y = rxDataHeight*ImGui::GetTextLineHeightWithSpacing();
                if (windowId == WindowId::Tx) {
                    mainSize.y -= 2.5*ImGui::GetTextLineHeightWithSpacing() + 2.0f*style.ItemSpacing.y;
                }

                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.back());
                ImGui::BeginChild("Rx:data", mainSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::SetWindowFontScale(rxFontScale);
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvailWidth());
                ImGui::Text("%s", rxData.c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetWindowFontScale(1.0f);
                ImGui::SetScrollY(ImGui::GetScrollMaxY() - style.ItemSpacing.y);

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
                    pos.x -= 1.0f*ImGui::CalcTextSize("Clear | Copy").x;
                    pos.y -= 1.0f*ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetNextWindowPos(pos);

                    ImGui::OpenPopup("Rx options");
                    isHoldingDown = false;
                    isContextMenuOpen = true;
                }

                if (ImGui::BeginPopup("Rx options")) {
                    ImGui::PushItemWidth(0.5*mainSize.x);

                    ImGui::DragFloat("##height", &rxDataHeight, 0.1f, 4.0f, rxDataHeightMax, "Rx height: %.1f", 1.0f);
                    ImGui::DragFloat("##fontScale", &rxFontScale, 0.01f, 0.8f, 2.0f, "Font scale: %.2f", 1.0f);

                    ImGui::PopItemWidth();

                    ImGui::Separator();

                    if (ImGui::ButtonDisablable("Clear", {}, false)) {
                        rxData.clear();
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();
                    if (ImGui::ButtonDisablable("Copy", {}, false)) {
                        SDL_SetClipboardText(rxData.c_str());
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                } else {
                    isContextMenuOpen = false;
                }

                ImGui::EndChild();
                ImGui::PopFont();
            }

            if (windowId == WindowId::Tx) {
                static bool isHoldingInput = false;
                static std::string inputLast = "";

                const auto p0 = ImGui::GetCursorScreenPos();
                ImGui::SetCursorScreenPos({ p0.x, p0.y + 0.5f*ImGui::GetTextLineHeight() });

                static float amax = 0.0f;
                static float frac = 0.0f;

                amax = 0.0f;
                frac = (ImGui::GetTime() - tStartTx)/tLengthTx;

                if (txWaveformCurrent.size() && frac <= 1.0f) {
                    struct Funcs {
                        static float Sample(void * data, int i) {
                            auto res = std::fabs(((int16_t *)(data))[i]) ;
                            if (res > amax) amax = res;
                            return res;
                        }

                        static float SampleFrac(void * data, int i) {
                            if (i > frac*txWaveformCurrent.size()) {
                                return 0.0f;
                            }
                            return std::fabs(((int16_t *)(data))[i]);
                        }
                    };

                    auto posSave = ImGui::GetCursorScreenPos();
                    auto wSize = ImGui::GetContentRegionAvail();
                    wSize.x = ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(sendButtonText).x - 2*style.ItemSpacing.x;
                    wSize.y = ImGui::GetTextLineHeight();

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.3f, 0.3f, 0.3f, 0.3f });
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::PlotHistogram("##plotSpectrumCurrent",
                                         Funcs::Sample,
                                         txWaveformCurrent.data(),
                                         (int) txWaveformCurrent.size(), 0,
                                         (std::string("")).c_str(),
                                         0.0f, FLT_MAX, wSize);
                    ImGui::PopStyleColor(2);

                    ImGui::SetCursorScreenPos(posSave);

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.0f, 0.0f, 0.0f, 0.0f });
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.0f, 1.0f, 0.0f, 1.0f });
                    ImGui::PlotHistogram("##plotSpectrumCurrent",
                                         Funcs::SampleFrac,
                                         txWaveformCurrent.data(),
                                         (int) txWaveformCurrent.size(), 0,
                                         (std::string("")).c_str(),
                                         0.0f, amax, wSize);
                    ImGui::PopStyleColor(2);
                } else {
                    ImGui::TextDisabled("F: %5.1f Hz | S: %d/%d WPM",
                                        settings.txFrequency_hz, settings.txSpeedCharacters_wpm, settings.txSpeedFarnsworth_wpm);
                }

                if (doInputFocus) {
                    ImGui::SetKeyboardFocusHere();
                    doInputFocus = false;
                }

                doSendMessage = false;
                {
                    auto pos0 = ImGui::GetCursorScreenPos();
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(sendButtonText).x - 2*style.ItemSpacing.x);
                    if (ImGui::InputText("##Messages:Input", inputBuf, kMaxInputSize, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        doSendMessage = true;
                    }
                    ImGui::PopItemWidth();

                    if (isTextInput == false && inputBuf[0] == 0) {
                        auto drawList = ImGui::GetWindowDrawList();
                        pos0.x += style.ItemInnerSpacing.x;
                        pos0.y += 0.5*style.ItemInnerSpacing.y;
                        static char tmp[128];
                        snprintf(tmp, 128, "Type some text");
                        drawList->AddText(pos0, ImGui::ColorConvertFloat4ToU32({0.0f, 0.6f, 0.4f, 1.0f}), tmp);
                    }
                }

                if (ImGui::IsItemActive() && isTextInput == false) {
                    SDL_StartTextInput();
                    isTextInput = true;
                    tStartInput = ImGui::GetTime();
                }
                bool requestStopTextInput = false;
                if (ImGui::IsItemDeactivated()) {
                    requestStopTextInput = true;
                }

                if (isTextInput) {
                    if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDownDuration[0] > tHoldContextPopup) {
                        isHoldingInput = true;
                    }
                }

                if (ImGui::IsMouseReleased(0) && isHoldingInput) {
                    auto pos = ImGui::GetMousePos();
                    pos.x -= 2.0f*ImGui::CalcTextSize("Paste").x;
                    pos.y -= 1.0f*ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetNextWindowPos(pos);

                    ImGui::OpenPopup("Input options");
                    isHoldingInput = false;
                }

                if (ImGui::BeginPopup("Input options")) {
                    if (ImGui::Button("Paste")) {
                        for (int i = 0; i < kMaxInputSize; ++i) inputBuf[i] = 0;
                        strncpy(inputBuf, SDL_GetClipboardText(), kMaxInputSize - 1);
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }

                ImGui::SameLine();
                {
                    auto posCur = ImGui::GetCursorScreenPos();
                    posCur.y -= ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetCursorScreenPos(posCur);
                }
                if ((ImGui::Button(sendButtonText, { 0, 2*ImGui::GetTextLineHeightWithSpacing() }) || doSendMessage)) {
                    if (inputBuf[0] == 0) {
                        strncpy(inputBuf, inputLast.data(), kMaxInputSize - 1);
                    }
                    if (inputBuf[0] != 0) {
                        inputLast = std::string(inputBuf);
                        g_buffer.inputUI.update = true;
                        g_buffer.inputUI.flags.newMessage = true;
                        g_buffer.inputUI.message = {
                            std::chrono::system_clock::now(),
                            std::string(inputBuf),
                            {
                                settings.volume, settings.txFrequency_hz,
                                (float) settings.txSpeedCharacters_wpm,
                                (float) settings.txSpeedFarnsworth_wpm
                            }
                        };

                        inputBuf[0] = 0;
                        doInputFocus = true;
                    }
                }
                if (!ImGui::IsItemHovered() && requestStopTextInput) {
                    SDL_StopTextInput();
                    isTextInput = false;
                    tEndInput = ImGui::GetTime();
                }
            }
        }
    }

    ImGui::End();

    ImGui::GetIO().KeysDown[ImGui::GetIO().KeyMap[ImGuiKey_Backspace]] = false;
    ImGui::GetIO().KeysDown[ImGui::GetIO().KeyMap[ImGuiKey_Enter]] = false;

    if (g_buffer.inputUI.flags.newParametersDecode) {
        g_buffer.inputUI.update = true;
        g_buffer.inputUI.parametersDecode.frequency_hz = settings.isFrequencyAuto ? -1.0f : settings.frequencySelected_hz;
        g_buffer.inputUI.parametersDecode.speed_wpm = settings.isSpeedAuto ? -1.0f : settings.speedSelected_wpm;
        g_buffer.inputUI.parametersDecode.frequencyRangeMin_hz = settings.binMin*settings.df;
        g_buffer.inputUI.parametersDecode.frequencyRangeMax_hz = settings.binMax*settings.df;
        g_buffer.inputUI.parametersDecode.applyFilterHighPass = settings.applyFilterHighPass;
        g_buffer.inputUI.parametersDecode.applyFilterLowPass = settings.applyFilterLowPass;
    }

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

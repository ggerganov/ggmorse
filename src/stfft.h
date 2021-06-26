#pragma once

#include "fft.h"

#include <vector>
#include <cmath>

struct STFFT {
    void init(
            int sampleRate,
            int fft_size,
            int fft_step,
            float history_s) {
        m_sampleRate = sampleRate;

        m_hamming.resize(fft_size);
        for (int i = 0; i < fft_size; i++) {
            m_hamming[i] = 0.54 - 0.46*std::cos((2.0*M_PI*i)/fft_size);
        }

        int history_samples = history_s*sampleRate;
        m_historyHead = 0;
        m_history.resize(history_samples, 0);

        int historySteps = 1 + (history_samples - fft_size)/fft_step;
        m_spectrogramHead = 0;
        m_spectrogram.resize(historySteps);
        for (auto & row : m_spectrogram) {
            row.resize(fft_size, 0);
        }
        m_spectrogramOrdered = m_spectrogram;

        m_needed_samples = fft_step;
        m_fft_step = fft_step;
        m_fft_buffer.resize(2*fft_size);

        m_processed_samples = 0;
    }

    void process(float * samples, int n) {
        int nw = (int) m_hamming.size();
        int nh = (int) m_history.size();
        int ns = (int) m_spectrogram.size();

        for (int i = 0; i < n; ++i) {
            m_history[m_historyHead] = samples[i];
            m_historyHead++;
            if (m_historyHead >= nh) {
                m_historyHead = 0;
            }

            m_processed_samples++;

            m_needed_samples--;
            if (m_needed_samples == 0) {
                filter(m_historyHead - nw);
                m_spectrogramHead++;
                if (m_spectrogramHead >= ns) {
                    m_spectrogramHead = 0;
                }
                m_needed_samples = m_fft_step;
            }
        }
    }

    float pitch(float fMin_hz, float fMax_hz) {
        int n = (int) m_hamming.size();
        int ns = (int) m_spectrogram.size();
        float maxSignal = 0.0f;
        float bestPitch = 0.0f;
        float df = float(m_sampleRate)/n;

        for (int j = 0; j < n/2; ++j) {
            float f = j*df;
            if (f < fMin_hz || f > fMax_hz) continue;

            float curSignal = 0.0;

            int ih = m_spectrogramHead + ns/2;
            if (ih >= ns) {
                ih = 0;
            }
            for (int i = 0; i < ns/2; ++i) {
                curSignal += m_spectrogram[ih][j];
                ++ih;
                if (ih >= ns) {
                    ih = 0;
                }
            }

            if (curSignal > maxSignal) {
                maxSignal = curSignal;
                bestPitch = f;
            }
        }

        return bestPitch;
    }

    const std::vector<std::vector<float>> & spectrogram() {
        int n = (int) m_hamming.size();
        int ns = (int) m_spectrogram.size();
        int ih = m_spectrogramHead;
        for (int i = 0; i < ns; ++i) {
            for (int j = 0; j < n; ++j) {
                m_spectrogramOrdered[i][j] = m_spectrogram[ih][j];
            }
            ++ih;
            if (ih >= ns) {
                ih = 0;
            }
        }

        return m_spectrogramOrdered;
    }

private:
    void filter(int idx) {
        if (idx < 0) idx += m_history.size();

        int n = (int) m_hamming.size();
        for (int i = 0; i < n; i++) {
            m_fft_buffer[2*i + 0] = m_hamming[i]*m_history[idx++];
            m_fft_buffer[2*i + 1] = 0.0f;
            if (idx >= (int) m_history.size()) idx = 0;
        }

        FFT(m_fft_buffer.data(), n, 1.0);

        auto & dst = m_spectrogram[m_spectrogramHead];
        for (int i = 0; i < n; i++) {
            dst[i] = (m_fft_buffer[2*i + 0]*m_fft_buffer[2*i + 0] + m_fft_buffer[2*i + 1]*m_fft_buffer[2*i + 1]);
        }
    }

    int m_sampleRate = 0;
    int m_processed_samples = 0;
    int m_fft_step = 0;

    std::vector<float> m_hamming;

    int m_historyHead = 0;
    std::vector<float> m_history;

    int m_needed_samples = 0;
    int m_spectrogramHead = 0;
    std::vector<std::vector<float>> m_spectrogram;
    std::vector<std::vector<float>> m_spectrogramOrdered;

    std::vector<float> m_fft_buffer;
};

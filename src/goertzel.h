#pragma once

#include <vector>
#include <cmath>

struct GoertzelRunningFIR {
    void init(
            float sampleRate,
            int window_samples,
            float history_s) {
        m_sampleRate = sampleRate;
        m_hamming.resize(window_samples);
        for (int i = 0; i < window_samples; i++) {
            m_hamming[i] = 0.54 - 0.46*std::cos((2.0*M_PI*i)/window_samples);
        }

        int history_samples = history_s*sampleRate;

        m_historyHead = 0;
        m_history.resize(history_samples, 0);

        m_filteredHead = 0;
        m_filtered.resize(history_samples - window_samples, 0);
        m_filteredOut.resize(history_samples - window_samples, 0);

        m_processed_samples = 0;
    }

    void process(float * samples, int n, float frequency_hz) {
        int nw = (int) m_hamming.size();
        int nh = (int) m_history.size();
        int nf = (int) m_filtered.size();

        float normalizedfreq = frequency_hz/m_sampleRate;

        float w = 2*M_PI*normalizedfreq;
        float wr = std::cos(w);
        float wi = std::sin(w);

        m_coeff = 2.0*wr;
        m_cos = wr;
        m_sin = wi;

        for (int i = 0; i < n; ++i) {
            m_history[m_historyHead] = samples[i];
            m_historyHead++;
            if (m_historyHead >= nh) {
                m_historyHead = 0;
            }

            m_processed_samples++;
            if (m_processed_samples >= nw) {
                m_filtered[m_filteredHead] = filter(m_historyHead - nw);
                m_filteredHead++;
                if (m_filteredHead >= nf) {
                    m_filteredHead = 0;
                }
            }
        }
    }

    const std::vector<float> & filtered() {
        int nf = (int) m_filtered.size();

        int j = m_filteredHead;
        for (int i = 0; i < nf; ++i) {
            m_filteredOut[i] = m_filtered[j];
            j++;
            if (j >= nf) {
                j = 0;
            }
        }

        return m_filteredOut;
    }

    const std::vector<float> & filtered_min(int w) {
        int nf = (int) m_filtered.size();

        int j = m_filteredHead;
        for (int i = 0; i < nf; ++i) {
            int j2 = j - std::min(i, w);                  // !!!! Need to double-check these computations
            int l = std::min(i, w) + std::min(nf - i, w); // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            if (j2 < 0) j2 += nf;
            float f = m_filtered[j2];
            for (int k = 0; k < l; ++k) {
                f = std::min(f, m_filtered[j2]);
                if (++j2 >= nf) j2 = 0;
            }
            m_filteredOut[i] = f;

            j++;
            if (j >= nf) {
                j = 0;
            }
        }

        return m_filteredOut;
    }

    void clear() {
        std::fill(m_history.begin(), m_history.end(), 0.0f);
        std::fill(m_filtered.begin(), m_filtered.end(), 0.0f);
    }

private:
    float filter(int idx) {
        if (idx < 0) idx += m_history.size();

        double sprev = 0.0;
        double sprev2 = 0.0;
        double s, imag, real;

        int n = (int) m_hamming.size();
        for (int i = 0; i < n; i++) {
            s = m_hamming[i]*m_history[idx++] + m_coeff*sprev - sprev2;
            if (idx >= (int) m_history.size()) idx = 0;
            sprev2 = sprev;
            sprev = s;
        }

        real = sprev*m_cos - sprev2;
        imag = -sprev*m_sin;

        return real*real + imag*imag;
    }

    int m_processed_samples = 0;

    float m_sampleRate = 0.0f;
    float m_coeff = 0.0f;
    float m_sin = 0.0f;
    float m_cos = 0.0f;

    std::vector<float> m_hamming;

    int m_historyHead = 0;
    std::vector<float> m_history;

    int m_filteredHead = 0;
    std::vector<float> m_filtered;
    std::vector<float> m_filteredOut;
};

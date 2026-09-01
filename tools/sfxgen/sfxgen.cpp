// sfxgen.cpp - 간단한 레트로/8-bit 스타일 SFX 생성기
// JKENGINE 프로젝트용 WAV 파일 생성 툴
// 빌드: g++ -std=c++17 -O2 sfxgen.cpp -o sfxgen

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;
#endif

constexpr int SAMPLE_RATE = 44100;
constexpr int BITS_PER_SAMPLE = 16;
constexpr int CHANNELS = 1; // 모노 SFX

struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize = 0;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1; // PCM
    uint16_t numChannels = CHANNELS;
    uint32_t sampleRate = SAMPLE_RATE;
    uint32_t byteRate = SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8);
    uint16_t blockAlign = CHANNELS * (BITS_PER_SAMPLE / 8);
    uint16_t bitsPerSample = BITS_PER_SAMPLE;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize = 0;
};

void WriteWav(const std::string& path, const std::vector<int16_t>& samples) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to write: " << path << "\n";
        return;
    }

    WavHeader header;
    header.dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    header.fileSize = 36 + header.dataSize;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(samples.data()), header.dataSize);
    std::cout << "Generated: " << path << " (" << samples.size() << " samples, "
              << static_cast<double>(samples.size()) / SAMPLE_RATE << "s)\n";
}

// 기본 파형 발생기
inline double Sine(double t, double freq) {
    return std::sin(2.0 * M_PI * freq * t);
}

inline double Square(double t, double freq) {
    return (std::fmod(t * freq, 1.0) < 0.5) ? 1.0 : -1.0;
}

inline double Sawtooth(double t, double freq) {
    return 2.0 * std::fmod(t * freq, 1.0) - 1.0;
}

inline double Triangle(double t, double freq) {
    double x = std::fmod(t * freq, 1.0);
    return (x < 0.5) ? (4.0 * x - 1.0) : (3.0 - 4.0 * x);
}

inline double WhiteNoise() {
    return (static_cast<double>(std::rand()) / RAND_MAX) * 2.0 - 1.0;
}

inline double EnvelopeAD(double t, double attack, double decay) {
    if (t < attack) return t / attack;
    double release = (t - attack) / decay;
    if (release >= 1.0) return 0.0;
    return 1.0 - release;
}

inline double EnvelopeExpDecay(double t, double decayTime) {
    return std::exp(-t / decayTime);
}

inline int16_t ToInt16(double sample) {
    double clamped = sample;
    if (clamped > 1.0) clamped = 1.0;
    if (clamped < -1.0) clamped = -1.0;
    return static_cast<int16_t>(clamped * 32767.0);
}

} // namespace

// ============ SFX 생성 함수들 ============

std::vector<int16_t> GenerateButtonClick(double duration = 0.05) {
    std::vector<int16_t> out;
    int samples = static_cast<int>(SAMPLE_RATE * duration);
    double freqStart = 1200.0;
    double freqEnd = 800.0;
    for (int i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(SAMPLE_RATE);
        double freq = freqStart + (freqEnd - freqStart) * (t / duration);
        double env = EnvelopeAD(t, 0.005, duration - 0.005);
        double s = Square(t, freq) * env * 0.5;
        out.push_back(ToInt16(s));
    }
    return out;
}

std::vector<int16_t> GenerateMineOpen(double duration = 0.08) {
    std::vector<int16_t> out;
    int samples = static_cast<int>(SAMPLE_RATE * duration);
    for (int i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(SAMPLE_RATE);
        double env = EnvelopeAD(t, 0.01, duration - 0.01);
        // 노이즈 + 저역 필터 효과를 위한 누적 평균
        static double last = 0.0;
        double n = WhiteNoise();
        last = last * 0.7 + n * 0.3;
        double s = last * env * 0.4;
        out.push_back(ToInt16(s));
    }
    return out;
}

std::vector<int16_t> GenerateMineExplosion(double duration = 0.5) {
    std::vector<int16_t> out;
    int samples = static_cast<int>(SAMPLE_RATE * duration);
    double last = 0.0;
    for (int i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(SAMPLE_RATE);
        double env = EnvelopeExpDecay(t, 0.15);
        double n = WhiteNoise();
        // 저역 필터 (폭발의 울림 느낌)
        last = last * 0.85 + n * 0.15;
        // 주파수가 낮아지는 핑크노이즈 느낌
        double pitchDrop = 1.0 - std::min(t / 0.3, 1.0) * 0.5;
        double s = last * env * pitchDrop * 0.8;
        out.push_back(ToInt16(s));
    }
    return out;
}

std::vector<int16_t> GenerateTetrisMove(double duration = 0.04) {
    std::vector<int16_t> out;
    int samples = static_cast<int>(SAMPLE_RATE * duration);
    for (int i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(SAMPLE_RATE);
        double env = EnvelopeAD(t, 0.005, duration - 0.005);
        double s = Triangle(t, 220.0) * env * 0.3;
        out.push_back(ToInt16(s));
    }
    return out;
}

std::vector<int16_t> GenerateTetrisRotate(double duration = 0.05) {
    std::vector<int16_t> out;
    int samples = static_cast<int>(SAMPLE_RATE * duration);
    for (int i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(SAMPLE_RATE);
        double freq = 300.0 + 200.0 * (t / duration);
        double env = EnvelopeAD(t, 0.005, duration - 0.005);
        double s = Sine(t, freq) * env * 0.3;
        out.push_back(ToInt16(s));
    }
    return out;
}

std::vector<int16_t> GenerateTetrisDrop(double duration = 0.06) {
    std::vector<int16_t> out;
    int samples = static_cast<int>(SAMPLE_RATE * duration);
    for (int i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(SAMPLE_RATE);
        double freq = 400.0 - 200.0 * (t / duration);
        double env = EnvelopeAD(t, 0.005, duration - 0.005);
        double s = Square(t, freq) * env * 0.4;
        out.push_back(ToInt16(s));
    }
    return out;
}

std::vector<int16_t> GenerateTetrisClear(double duration = 0.4) {
    std::vector<int16_t> out;
    int samples = static_cast<int>(SAMPLE_RATE * duration);
    // 상승하는 4음 아르페지오
    double notes[4] = {523.25, 659.25, 783.99, 1046.50}; // C6, E6, G6, C7
    double noteDuration = duration / 4.0;
    for (int i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(SAMPLE_RATE);
        int noteIndex = static_cast<int>(t / noteDuration);
        if (noteIndex > 3) noteIndex = 3;
        double localT = t - noteIndex * noteDuration;
        double env = EnvelopeAD(localT, 0.01, noteDuration - 0.01);
        double s = Square(localT, notes[noteIndex]) * env * 0.3;
        out.push_back(ToInt16(s));
    }
    return out;
}

std::vector<int16_t> GenerateTetrisGameOver(double duration = 0.8) {
    std::vector<int16_t> out;
    int samples = static_cast<int>(SAMPLE_RATE * duration);
    for (int i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(SAMPLE_RATE);
        double freq = 440.0 * std::pow(0.5, t / duration);
        double env = EnvelopeExpDecay(t, 0.4);
        double s = Sawtooth(t, freq) * env * 0.4;
        out.push_back(ToInt16(s));
    }
    return out;
}

// ============ BGM 생성 함수들 ============

// 간단한 아르페지오 시퀀서: 패턴을 샘플 수준으로 렌더링
using NoteFn = double(*)(double t, double freq);

struct BgmNote {
    double startBeat;
    double durationBeats;
    double freq;
    NoteFn waveform;
    double volume;
};

std::vector<int16_t> RenderPattern(const std::vector<BgmNote>& notes,
                                      double bpm,
                                      int loops,
                                      int stereoChannels) {
    double beatDuration = 60.0 / bpm;
    double loopBeats = 0.0;
    for (const auto& n : notes) {
        double end = n.startBeat + n.durationBeats;
        if (end > loopBeats) loopBeats = end;
    }
    double loopDuration = loopBeats * beatDuration;
    int totalSamples = static_cast<int>(SAMPLE_RATE * loopDuration * loops);

    std::vector<double> mix(totalSamples * stereoChannels, 0.0);

    for (int loop = 0; loop < loops; ++loop) {
        double loopOffset = loop * loopDuration;
        for (const auto& n : notes) {
            double startTime = loopOffset + n.startBeat * beatDuration;
            double endTime = startTime + n.durationBeats * beatDuration;
            int startSample = static_cast<int>(startTime * SAMPLE_RATE);
            int endSample = static_cast<int>(endTime * SAMPLE_RATE);
            if (startSample < 0) startSample = 0;
            if (endSample > totalSamples) endSample = totalSamples;

            for (int i = startSample; i < endSample; ++i) {
                double t = (i - startSample) / static_cast<double>(SAMPLE_RATE);
                double localDuration = n.durationBeats * beatDuration;
                double env = EnvelopeAD(t, 0.02, localDuration - 0.02);
                if (env < 0.0) env = 0.0;
                double sample = n.waveform(t, n.freq) * env * n.volume;

                // 스테레오: 약간의 위치 변화를 위해 양쪽에 동일하게 믹싱
                mix[i * stereoChannels + 0] += sample;
                if (stereoChannels > 1) {
                    mix[i * stereoChannels + 1] += sample;
                }
            }
        }
    }

    std::vector<int16_t> out;
    out.reserve(mix.size());
    for (double s : mix) {
        out.push_back(ToInt16(s));
    }
    return out;
}

void WriteWavStereo(const std::string& path, const std::vector<int16_t>& samples) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to write: " << path << "\n";
        return;
    }

    WavHeader header;
    header.numChannels = 2;
    header.byteRate = SAMPLE_RATE * 2 * (BITS_PER_SAMPLE / 8);
    header.blockAlign = 2 * (BITS_PER_SAMPLE / 8);
    header.dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    header.fileSize = 36 + header.dataSize;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(samples.data()), header.dataSize);
    std::cout << "Generated: " << path << " (" << samples.size() / 2 << " stereo samples, "
              << static_cast<double>(samples.size() / 2) / SAMPLE_RATE << "s)\n";
}

std::vector<int16_t> GenerateTetrisBGM() {
    // C5, E5, G5, A5, G5, E5, C5, G4 패턴
    std::vector<BgmNote> notes = {
        {0.0, 0.45, 523.25, Square, 0.20},
        {0.5, 0.45, 659.25, Square, 0.20},
        {1.0, 0.45, 783.99, Square, 0.20},
        {1.5, 0.45, 880.00, Square, 0.20},
        {2.0, 0.45, 783.99, Square, 0.20},
        {2.5, 0.45, 659.25, Square, 0.20},
        {3.0, 0.45, 523.25, Square, 0.20},
        {3.5, 0.45, 392.00, Square, 0.20},
    };
    // 8바 반복 = 32비트, BPM 140, 약 18.3초
    return RenderPattern(notes, 140.0, 8, 2);
}

std::vector<int16_t> GenerateMineBGM() {
    // 긴장감 있는 낮은 드론 + 느린 펄스
    std::vector<BgmNote> notes = {
        {0.0, 7.5, 55.0, Sawtooth, 0.15},
        {0.0, 0.8, 110.0, Square, 0.08},
        {2.0, 0.8, 110.0, Square, 0.08},
        {4.0, 0.8, 110.0, Square, 0.08},
        {6.0, 0.8, 110.0, Square, 0.08},
    };
    // BPM 60, 8마디 = 32비트, 32초
    return RenderPattern(notes, 60.0, 1, 2);
}

// ============ 메인 ============

int main(int argc, char* argv[]) {
    std::string outDir = ".";
    if (argc > 1) {
        outDir = argv[1];
    }

    std::cout << "JKENGINE SFX/BGM Generator\n";
    std::cout << "Output directory: " << outDir << "\n\n";

    WriteWav(outDir + "/button_click.wav", GenerateButtonClick());
    WriteWav(outDir + "/mine_open.wav", GenerateMineOpen());
    WriteWav(outDir + "/mine_explosion.wav", GenerateMineExplosion());
    WriteWav(outDir + "/tetris_move.wav", GenerateTetrisMove());
    WriteWav(outDir + "/tetris_rotate.wav", GenerateTetrisRotate());
    WriteWav(outDir + "/tetris_drop.wav", GenerateTetrisDrop());
    WriteWav(outDir + "/tetris_clear.wav", GenerateTetrisClear());
    WriteWav(outDir + "/tetris_gameover.wav", GenerateTetrisGameOver());

    WriteWavStereo(outDir + "/tetris_theme.wav", GenerateTetrisBGM());
    WriteWavStereo(outDir + "/mine_ambient.wav", GenerateMineBGM());

    std::cout << "\nAll SFX and BGM generated successfully.\n";
    return 0;
}

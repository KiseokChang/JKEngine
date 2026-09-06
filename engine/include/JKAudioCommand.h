#ifndef JKAUDIOCOMMAND_H
#define JKAUDIOCOMMAND_H

#include <string>
#include <cstdint>

namespace jk {

// Serialized audio commands sent from the application thread to JKAudioThread.
// Keep this layout simple so it can be passed through a binary message bus
// payload and later over IPC.
struct AudioCommand {
    enum class Type : uint32_t {
        None,
        Init,
        Quit,
        LoadSFX,
        PlaySFX,
        LoadBGM,
        PlayBGM,
        StopBGM,
        PauseBGM,
        ResumeBGM,
        SetMasterVolume,
        SetBusVolume,
        SetBGMVolume,
        HaltAllSFX
    };

    Type type = Type::None;
    char id[64] = {};
    char path[256] = {};
    char busId[64] = {};
    int loops = 0;
    float volume = 0.0f;
    float busVolume = 0.0f;
};

} // namespace jk

#endif // JKAUDIOCOMMAND_H

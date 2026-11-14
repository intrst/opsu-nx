// The registry (setAudioBackend / audioBackend) lives in SilentAudioBackend.cpp, because the fallback is a SilentAudioBackend.
// This translation unit only keeps the "one .cpp per header" layout of the other ports and checks that the header stands alone.
#include "opsu/audio/IAudioBackend.h"

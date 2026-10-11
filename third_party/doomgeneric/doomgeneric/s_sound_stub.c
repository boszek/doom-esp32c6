// Zaslepka systemu dzwieku (DG_MONO): dema bez dzwieku i muzyki.
// Zastepuje s_sound.c; dzieki temu tablice sounds.c nie sa linkowane (oszczednosc RAM/flasha).
#include "doomtype.h"
#include "p_mobj.h"
#include "s_sound.h"

void S_Init(int sfxVolume, int musicVolume) {}
void S_Shutdown(void) {}
void S_Start(void) {}
void S_StartSound(void *origin, int sound_id) {}
void S_StopSound(mobj_t *origin) {}
void S_StartMusic(int music_id) {}
void S_ChangeMusic(int music_id, int looping) {}
boolean S_MusicPlaying(void) { return false; }
void S_StopMusic(void) {}
void S_PauseSound(void) {}
void S_ResumeSound(void) {}
void S_UpdateSounds(mobj_t *listener) {}
void S_SetMusicVolume(int volume) {}
void S_SetSfxVolume(int volume) {}

// zmienne konfiguracyjne (normalnie w s_sound.c)
int sfxVolume = 8;
int musicVolume = 8;
int snd_channels = 8;

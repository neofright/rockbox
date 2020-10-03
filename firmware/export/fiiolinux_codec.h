#ifndef __FIIOLINUX_CODEC__
#define __FIIOLINUX_CODEC__

#define AUDIOHW_CAPS (FILTER_ROLL_OFF_CAP)
AUDIOHW_SETTING(VOLUME, "dB", 0, 1, -100,  0, -30)
AUDIOHW_SETTING(FILTER_ROLL_OFF, "", 0, 1, 0, 4, 0)
#endif

void audiohw_mute(int mute);

/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * electric_volume.h
 *
 * Created: 2015/12/13 21:11:16
 *  Author: akihiro
 */

#include <stdint-gcc.h>

#ifndef ELECTRIC_VOLUME_H_
#define ELECTRIC_VOLUME_H_

typedef int8_t elevol_t;
#define ELEVOL_MIN  0
#define ELEVOL_MAX  31

extern void elevol_prepare();
extern void elevol_power_save();

extern elevol_t elevol_validate_volume( elevol_t volume );
extern elevol_t elevol_setvolume(elevol_t setvol);

extern void elevol_set_mute();
extern elevol_t elevol_clear_mute( elevol_t vol );

#endif /* ELECTRIC_VOLUME_H_ */

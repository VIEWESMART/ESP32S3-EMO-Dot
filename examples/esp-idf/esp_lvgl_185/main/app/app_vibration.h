#pragma once
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif


#define Z_axis              (0)

typedef enum {
    TOUCH = 0,
    PRESS = 1,
    LONG_PRESS = 2,
    SCROLL = 3,
    LONG_VIBRATION = 4
} vibration_type_t;

#if Z_axis
    typedef enum {
        STRONG = 512,
        MEDIUM = 300,
        WEAK = 100,
        OFF =0
    } vibration_intensity_t;
#else
    typedef enum {
        OFF =0,
        WEAK = 500,
        MEDIUM = 800,
        STRONG = 1023
    } vibration_intensity_t;
#endif

typedef struct {
    vibration_type_t type;
    vibration_intensity_t intensity;
} vibration_message_t;


void vibration_init(void);
void vibration_deinit(void);
void vibration_send(vibration_type_t type, vibration_intensity_t intensity);
#ifdef __cplusplus
} /*extern "C"*/
#endif


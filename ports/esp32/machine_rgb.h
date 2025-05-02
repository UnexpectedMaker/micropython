#ifndef MICROPY_INCLUDED_PORTS_ESP32_MACHINE_RGB_H
#define MICROPY_INCLUDED_PORTS_ESP32_MACHINE_RGB_H

// #include "py/obj.h"

#include "modmachine.h"

// Enable RGB peripheral type only on ESP32-S3
#if MICROPY_PY_MACHINE_RGB

// Extern declaration of the machine.RGB type
extern const mp_obj_type_t machine_rgb_type;

#endif // MICROPY_PY_MACHINE_RGB

#endif // MICROPY_INCLUDED_PORTS_ESP32_MACHINE_RGB_H

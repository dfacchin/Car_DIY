#include "pid_store.h"
#include "config.h"
#include <EEPROM.h>

#define EEPROM_MAGIC        0xCA  // Marker to detect valid data
#define EEPROM_ADDR_MAGIC   0
#define EEPROM_ADDR_DATA    1

bool pid_store_load(pid_params_t *params) {
    EEPROM.begin();

    if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
        // No valid data, fill defaults
        params->kp = PID_KP;
        params->ki = PID_KI;
        params->kd = PID_KD;
        params->integral_max = PID_INTEGRAL_MAX;
        params->ramp_limit = SAFETY_RAMP_MAX_RPM_PER_CYCLE;
        return false;
    }

    EEPROM.get(EEPROM_ADDR_DATA, *params);
    return true;
}

void pid_store_save(const pid_params_t *params) {
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.put(EEPROM_ADDR_DATA, *params);
}

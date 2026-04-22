/* Self header */
#include "tmc5160.h"

/**
 * Clamp helper that works regardless of Arduino min/max macro definitions.
 * This avoids the classic min/max type mismatch issues on Arduino ESP32.
 */
static inline uint32_t tmc5160_clamp_u32(const uint32_t value, const uint32_t max_value) {
    return (value > max_value) ? max_value : value;
}

/**
 *
 * @param[in] config
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 *  -ENODEV If the device was not detected
 */
int tmc5160::setup(struct config &config) {
    int res;

    /* Ensure driver is detected and has the expected version */
    union reg_io_input_output reg_io_input_output = {0};
    res = register_read(reg::IO_INPUT_OUTPUT, reg_io_input_output.raw);
    if (res < 0) {
        return -EIO;
    }
    if (reg_io_input_output.fields.version != 0x30) {
        return -ENODEV;
    }

    /* Clear the reset, driver error and charge pump undervoltage flags */
    union reg_gstat reg_gstat = {0};
    reg_gstat.fields.reset = 1;
    reg_gstat.fields.drv_err = 1;
    reg_gstat.fields.uv_cp = 1;
    res = register_write(reg::GSTAT, reg_gstat.raw);
    if (res < 0) {
        return -EIO;
    }

    /* Write configuration registers */
    res = 0;
    res |= register_write(reg::DRV_CONF, config.reg_drv_conf.raw);
    res |= register_write(reg::GLOBAL_SCALER, tmc5160_clamp_u32(config.reg_global_scaler, 256));
    res |= register_write(reg::SHORT_CONF, config.reg_short_conf.raw);
    res |= register_write(reg::CHOPCONF, config.reg_chopconf.raw);
    res |= register_write(reg::IHOLD_IRUN, config.reg_ihold_irun.raw);
    res |= register_write(reg::TPOWERDOWN, config.reg_tpowerdown.raw);
    res |= register_write(reg::GCONF, config.reg_gconf.raw);
    res |= register_write(reg::TPWMTHRS, config.reg_tpwmthrs.raw);
    res |= register_write(reg::PWMCONF, config.reg_pwmconf.raw);
    if (res < 0) {
        return -EIO;
    }

    /* Cache CHOPCONF so we can restore TOFF when re-enabling the driver */
    m_reg_chopconf_cache.raw = config.reg_chopconf.raw;

    /* Remember the sense resistor value so current_set / current_get can do the math */
    m_rsense = config.rsense;

    /* Set default speeds.
     * This is done at least here because the datasheet explicitly states that D1 and VSTOP
     * should not be set to 0 in positioning mode. */
    res = 0;
    res |= register_write(reg::RAMPMODE, 0);
    res |= register_write(reg::VSTART, 0);
    res |= register_write(reg::V_1, 0);
    res |= register_write(reg::VSTOP, 10);
    res |= register_write(reg::VMAX, 100);
    res |= register_write(reg::AMAX, 10000);
    res |= register_write(reg::DMAX, 10000);
    res |= register_write(reg::A_1, 10000);
    res |= register_write(reg::D_1, 10000);
    if (res < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @param[in] vstart
 * @param[in] vstop
 * @param[in] vtrans
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::speed_ramp_set(const float vstart, const float vstop, const float vtrans) {
    int res;

    /* VSTART and VSTOP are 18-bit fields, V1 is 20-bit */
    res = 0;
    res |= register_write(reg::VSTART, tmc5160_clamp_u32(convert_velocity_to_tmc(fabs(vstart)), 0x3FFFF));
    res |= register_write(reg::VSTOP, tmc5160_clamp_u32(convert_velocity_to_tmc(fabs(vstop)), 0x3FFFF));
    res |= register_write(reg::V_1, tmc5160_clamp_u32(convert_velocity_to_tmc(fabs(vtrans)), 0xFFFFF));
    if (res < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @param[in] speed
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EINVAL If the argument is invalid
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::speed_limit_set(const float speed) {
    int res;

    /* Ensure speed is positive */
    if (speed < 0) {
        return -EINVAL;
    }

    /* VMAX is a 23-bit field */
    res = 0;
    res |= register_write(reg::VMAX, tmc5160_clamp_u32(convert_velocity_to_tmc(speed), 0x7FFFFF));
    if (res < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @param[in] acceleration
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EINVAL If the argument is invalid
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::acceleration_limit_set(const float acceleration) {
    int res;

    /* Ensure acceleration is positive */
    if (acceleration < 0) {
        return -EINVAL;
    }

    /* AMAX, DMAX, A1, D1 are all 16-bit fields */
    const uint32_t tmc_accel = tmc5160_clamp_u32(convert_acceleration_to_tmc(acceleration), 0xFFFF);
    res = 0;
    res |= register_write(reg::AMAX, tmc_accel);
    res |= register_write(reg::DMAX, tmc_accel);
    res |= register_write(reg::A_1, tmc_accel);
    res |= register_write(reg::D_1, tmc_accel);
    if (res < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @param[in] position
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::move_to_position(const float position) {
    int res;

    /* Set RAMPMODE to Positioning mode */
    res = register_write(reg::RAMPMODE, 0x00);
    if (res < 0) {
        return -EIO;
    }

    /* Set XTARGET */
    int32_t reg_xtarget = roundf(position * m_ustep_per_step);
    res = register_write(reg::XTARGET, (uint32_t)reg_xtarget);
    if (res < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @param[in] velocity
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::move_at_velocity(const float velocity) {
    int res;

    /* Set VMAX then select velocity mode (positive or negative) */
    res = 0;
    res |= register_write(reg::VMAX, tmc5160_clamp_u32(convert_velocity_to_tmc(fabs(velocity)), 0x7FFFFF));
    res |= register_write(reg::RAMPMODE, velocity < 0.0f ? 2 : 1);
    if (res < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::move_stop(void) {

    /* For a stop in positioning mode, set VSTART=0 and VMAX=0
     * See datasheet §14.2.4 "Early Ramp Termination" option b) */
    int res = 0;
    res |= register_write(reg::VSTART, 0);
    res |= register_write(reg::VMAX, 0);
    if (res != 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @param[out] position
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::position_current_get(float &position) {
    uint32_t reg_xactual;
    if (register_read(reg::XACTUAL, reg_xactual) < 0) {
        return -EIO;
    }
    position = (int32_t)reg_xactual;
    position /= m_ustep_per_step;
    return 0;
}

/**
 *
 * @param[in] position
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::position_current_set(const float position) {
    int32_t reg_xactual = roundf(position * m_ustep_per_step);
    if (register_write(reg::XACTUAL, (uint32_t)reg_xactual) < 0) {
        return -EIO;
    }
    return 0;
}

/**
 *
 * @param[out] position
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::position_latched_get(float &position) {
    uint32_t reg_xlatch;
    if (register_read(reg::XLATCH, reg_xlatch) < 0) {
        return -EIO;
    }
    position = (int32_t)reg_xlatch;
    position /= m_ustep_per_step;
    return 0;
}

/**
 *
 * @param[out] position
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::position_target_get(float &position) {
    uint32_t reg_xtarget;
    if (register_read(reg::XTARGET, reg_xtarget) < 0) {
        return -EIO;
    }
    position = (int32_t)reg_xtarget;
    position /= m_ustep_per_step;
    return 0;
}

/**
 *
 * @param[out] velocity
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::velocity_current_get(float &velocity) {
    uint32_t reg_vactual;
    if (register_read(reg::VACTUAL, reg_vactual) < 0) {
        return -EIO;
    }

    /* VACTUAL is a 24-bit signed value, sign extend to 32 bits */
    int32_t signed_vactual = reg_vactual & 0x00FFFFFF;
    if (signed_vactual & (1L << 23)) {
        signed_vactual |= 0xFF000000;
    }

    /* Convert back to steps per second */
    velocity = (float)signed_vactual * ((float)m_fclk / (float)(1ul << 24)) / (float)m_ustep_per_step;
    return 0;
}

/**
 * Enable the driver by restoring the cached CHOPCONF register (which contains a non-zero TOFF).
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::driver_enable(void) {
    if (register_write(reg::CHOPCONF, m_reg_chopconf_cache.raw) < 0) {
        return -EIO;
    }
    return 0;
}

/**
 * Disable the driver by setting TOFF=0 in CHOPCONF.
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::driver_disable(void) {
    union reg_chopconf reg_chopconf_disabled;
    reg_chopconf_disabled.raw = m_reg_chopconf_cache.raw;
    reg_chopconf_disabled.fields.toff = 0;
    if (register_write(reg::CHOPCONF, reg_chopconf_disabled.raw) < 0) {
        return -EIO;
    }
    return 0;
}

/**
 *
 * @return 1 if the target position has been reached, 0 if it has not, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::target_position_reached_is(void) {

    /* Read bit 9 position_reached of RAMP_STAT
     * 1: Signals that the target position is reached */
    uint32_t reg_ramp_status;
    if (register_read(reg::RAMP_STAT, reg_ramp_status) < 0) {
        return -EIO;
    }

    /* Remember flags that are cleared upon reading */
    m_reference_l_latched = (reg_ramp_status & (1 << 2)) ? true : m_reference_l_latched;
    m_reference_r_latched = (reg_ramp_status & (1 << 3)) ? true : m_reference_r_latched;

    /* Return whether the target is reached */
    if (reg_ramp_status & (1 << 9)) {
        return 1;
    } else {
        return 0;
    }
}

/**
 *
 * @return 1 if the target velocity has been reached, 0 if it has not, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::target_velocity_reached_is(void) {

    /* Read bit 8 velocity_reached of RAMP_STAT
     * 1: Signals that the target velocity is reached */
    uint32_t reg_ramp_status;
    if (register_read(reg::RAMP_STAT, reg_ramp_status) < 0) {
        return -EIO;
    }

    /* Remember flags that are cleared upon reading */
    m_reference_l_latched = (reg_ramp_status & (1 << 2)) ? true : m_reference_l_latched;
    m_reference_r_latched = (reg_ramp_status & (1 << 3)) ? true : m_reference_r_latched;

    /* Return whether the target is reached */
    if (reg_ramp_status & (1 << 8)) {
        return 1;
    } else {
        return 0;
    }
}

/**
 *
 * @param[in] swap
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::reference_swap(bool swap) {

    /* Set bit 4 of SW_MODE
     * 1: Swap the left and the right reference switch input REFL and REFR */
    uint32_t reg_sw_mode;
    if (register_read(reg::SW_MODE, reg_sw_mode) < 0) {
        return -EIO;
    }
    if (swap) {
        reg_sw_mode |= (1 << 4);
    } else {
        reg_sw_mode &= ~(1 << 4);
    }
    if (register_write(reg::SW_MODE, reg_sw_mode) < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @param[in] active_high
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::reference_l_polarity_set(bool active_high) {

    /* Set bit 2 of SW_MODE
     * Sets the active polarity of the left reference switch input
     * 0=non-inverted, high active: a high level on REFL stops the motor
     * 1=inverted, low active: a low level on REFL stops the motor */
    uint32_t reg_sw_mode;
    if (register_read(reg::SW_MODE, reg_sw_mode) < 0) {
        return -EIO;
    }
    if (active_high) {
        reg_sw_mode &= ~(1 << 2);
    } else {
        reg_sw_mode |= (1 << 2);
    }
    if (register_write(reg::SW_MODE, reg_sw_mode) < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @param[in] active_high
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::reference_r_polarity_set(bool active_high) {

    /* Set bit 3 of SW_MODE
     * Sets the active polarity of the right reference switch input
     * 0=non-inverted, high active: a high level on REFR stops the motor
     * 1=inverted, low active: a low level on REFR stops the motor */
    uint32_t reg_sw_mode;
    if (register_read(reg::SW_MODE, reg_sw_mode) < 0) {
        return -EIO;
    }
    if (active_high) {
        reg_sw_mode &= ~(1 << 3);
    } else {
        reg_sw_mode |= (1 << 3);
    }
    if (register_write(reg::SW_MODE, reg_sw_mode) < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @return 1 if the left reference switch is active, 0 if it is not, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::reference_l_active_get(void) {

    /* Read bit 0 status_stop_l of RAMP_STAT
     * Reference switch left status (1=active) */
    uint32_t reg_ramp_status;
    if (register_read(reg::RAMP_STAT, reg_ramp_status) < 0) {
        return -EIO;
    }

    /* Remember flags that are cleared upon reading */
    m_reference_l_latched = (reg_ramp_status & (1 << 2)) ? true : m_reference_l_latched;
    m_reference_r_latched = (reg_ramp_status & (1 << 3)) ? true : m_reference_r_latched;

    /* Return whether the switch is active */
    return (reg_ramp_status & (1 << 0)) ? 1 : 0;
}

/**
 *
 * @return 1 if the right reference switch is active, 0 if it is not, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::reference_r_active_get(void) {

    /* Read bit 1 status_stop_r of RAMP_STAT
     * Reference switch right status (1=active) */
    uint32_t reg_ramp_status;
    if (register_read(reg::RAMP_STAT, reg_ramp_status) < 0) {
        return -EIO;
    }

    /* Remember flags that are cleared upon reading */
    m_reference_l_latched = (reg_ramp_status & (1 << 2)) ? true : m_reference_l_latched;
    m_reference_r_latched = (reg_ramp_status & (1 << 3)) ? true : m_reference_r_latched;

    /* Return whether the switch is active */
    return (reg_ramp_status & (1 << 1)) ? 1 : 0;
}

/**
 *
 * @param[in] polarity If true the position will be latched when the reference switch goes active, and conversely.
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::reference_l_latch_enable(bool polarity) {

    /* Reset flag */
    m_reference_l_latched = false;

    /* Set bits 5 and 6 of SW_MODE */
    uint32_t reg_sw_mode;
    if (register_read(reg::SW_MODE, reg_sw_mode) < 0) {
        return -EIO;
    }
    if (polarity) {
        reg_sw_mode &= ~(1 << 6);  // latch_l_inactive = 0
        reg_sw_mode |= (1 << 5);   // latch_l_active = 1
    } else {
        reg_sw_mode |= (1 << 6);   // latch_l_inactive = 1
        reg_sw_mode &= ~(1 << 5);  // latch_l_active = 0
    }
    if (register_write(reg::SW_MODE, reg_sw_mode) < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @param[in] polarity If true the position will be latched when the reference switch goes active, and conversely.
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::reference_r_latch_enable(bool polarity) {

    /* Reset flag */
    m_reference_r_latched = false;

    /* Set bits 7 and 8 of SW_MODE */
    uint32_t reg_sw_mode;
    if (register_read(reg::SW_MODE, reg_sw_mode) < 0) {
        return -EIO;
    }
    if (polarity) {
        reg_sw_mode &= ~(1 << 8);  // latch_r_inactive = 0
        reg_sw_mode |= (1 << 7);   // latch_r_active = 1
    } else {
        reg_sw_mode |= (1 << 8);   // latch_r_inactive = 1
        reg_sw_mode &= ~(1 << 7);  // latch_r_active = 0
    }
    if (register_write(reg::SW_MODE, reg_sw_mode) < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @param[out] position
 * @return 1 if the latched position is available, 0 if it is not, or a negative error code otherwise.
 */
int tmc5160::reference_l_latch_get(float &position) {

    /* Read bit 2 status_latch_l of RAMP_STAT
     * 1: Latch left ready */
    uint32_t reg_ramp_status;
    if (register_read(reg::RAMP_STAT, reg_ramp_status) < 0) {
        return -EIO;
    }

    /* Remember flags that are cleared upon reading */
    m_reference_l_latched = (reg_ramp_status & (1 << 2)) ? true : m_reference_l_latched;
    m_reference_r_latched = (reg_ramp_status & (1 << 3)) ? true : m_reference_r_latched;

    /* If latched position is available */
    if (m_reference_l_latched) {
        uint32_t reg_xlatch;
        if (register_read(reg::XLATCH, reg_xlatch) < 0) {
            return -EIO;
        }
        position = (int32_t)reg_xlatch;
        position /= m_ustep_per_step;
        m_reference_l_latched = false;
        return 1;
    } else {
        return 0;
    }
}

/**
 *
 * @param[out] position
 * @return 1 if the latched position is available, 0 if it is not, or a negative error code otherwise.
 */
int tmc5160::reference_r_latch_get(float &position) {

    /* Read bit 3 status_latch_r of RAMP_STAT
     * 1: Latch right ready */
    uint32_t reg_ramp_status;
    if (register_read(reg::RAMP_STAT, reg_ramp_status) < 0) {
        return -EIO;
    }

    /* Remember flags that are cleared upon reading */
    m_reference_l_latched = (reg_ramp_status & (1 << 2)) ? true : m_reference_l_latched;
    m_reference_r_latched = (reg_ramp_status & (1 << 3)) ? true : m_reference_r_latched;

    /* If latched position is available */
    if (m_reference_r_latched) {
        uint32_t reg_xlatch;
        if (register_read(reg::XLATCH, reg_xlatch) < 0) {
            return -EIO;
        }
        position = (int32_t)reg_xlatch;
        position /= m_ustep_per_step;
        m_reference_r_latched = false;
        return 1;
    } else {
        return 0;
    }
}

/**
 *
 * @param[out] position
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::encoder_position_get(float &position) {
    uint32_t reg_x_enc;
    if (register_read(reg::X_ENC, reg_x_enc) < 0) {
        return -EIO;
    }
    position = (int32_t)reg_x_enc;
    position /= m_ustep_per_step;
    return 0;
}

/**
 *
 * @param[in] position
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::encoder_position_set(const float position) {
    int32_t reg_x_enc = roundf(position * m_ustep_per_step);
    if (register_write(reg::X_ENC, (uint32_t)reg_x_enc) < 0) {
        return -EIO;
    }
    return 0;
}

/**
 * Configure the encoder prescaler so the encoder counter X_ENC tracks the motor position.
 * @param[in] motor_steps Motor full steps per turn (typically 200).
 * @param[in] encoder_resolution Encoder pulses per turn.
 * @param[in] inverted Whether the encoder rotation is inverted compared to the motor rotation.
 * @return 1 if an exact match was found using binary mode, 0 if the decimal mode approximation was used, or a negative error code otherwise.
 * @see Datasheet §22.2 "Encoder Prescaler"
 */
int tmc5160::encoder_resolution_set(const int32_t motor_steps, const int32_t encoder_resolution, const bool inverted) {
    int res;

    /* Ensure arguments are valid */
    if (motor_steps <= 0 || encoder_resolution <= 0) {
        return -EINVAL;
    }

    /* Compute the ratio */
    float factor = (float)motor_steps * (float)m_ustep_per_step / (float)encoder_resolution;

    /* Check if the binary prescaler gives an exact match */
    union reg_encmode reg_encmode = {0};
    if (register_read(reg::ENCMODE, reg_encmode.raw) < 0) {
        return -EIO;
    }
    if ((int64_t)(factor * 65536.0f) * encoder_resolution == (int64_t)motor_steps * m_ustep_per_step * 65536LL) {

        /* Use binary mode */
        reg_encmode.fields.enc_sel_decimal = 0;
        int32_t enc_const = (int32_t)(factor * 65536.0f);
        if (inverted) {
            enc_const = -enc_const;
        }
        res = 0;
        res |= register_write(reg::ENCMODE, reg_encmode.raw);
        res |= register_write(reg::ENC_CONST, (uint32_t)enc_const);
        if (res < 0) {
            return -EIO;
        }
        return 1;
    } else {

        /* Use decimal mode */
        reg_encmode.fields.enc_sel_decimal = 1;
        int integer_part = (int)floor(factor);
        int decimal_part = (int)((factor - (float)integer_part) * 10000.0f);
        if (inverted) {
            integer_part = 65535 - integer_part;
            decimal_part = 10000 - decimal_part;
        }
        int32_t enc_const = integer_part * 65536 + decimal_part;
        res = 0;
        res |= register_write(reg::ENCMODE, reg_encmode.raw);
        res |= register_write(reg::ENC_CONST, (uint32_t)enc_const);
        if (res < 0) {
            return -EIO;
        }
        return 0;
    }
}

/**
 *
 * @param[in] steps Maximum number of steps deviation allowed. Set to 0 to disable the deviation warning.
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EINVAL If the argument is invalid
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::encoder_deviation_set(const int32_t steps) {

    /* Ensure argument is valid */
    if (steps < 0) {
        return -EINVAL;
    }

    /* ENC_DEVIATION is a 20-bit field */
    uint32_t reg_enc_deviation = tmc5160_clamp_u32((uint32_t)steps * m_ustep_per_step, 0xFFFFF);
    if (register_write(reg::ENC_DEVIATION, reg_enc_deviation) < 0) {
        return -EIO;
    }
    return 0;
}

/**
 *
 * @return 1 if a deviation has been detected, 0 if not, or a negative error code otherwise.
 */
int tmc5160::encoder_deviation_detected_is(void) {
    union reg_enc_status reg_enc_status = {0};
    if (register_read(reg::ENC_STATUS, reg_enc_status.raw) < 0) {
        return -EIO;
    }
    return reg_enc_status.fields.deviation_warn ? 1 : 0;
}

/**
 *
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::encoder_deviation_clear(void) {
    union reg_enc_status reg_enc_status = {0};
    reg_enc_status.fields.deviation_warn = 1;
    if (register_write(reg::ENC_STATUS, reg_enc_status.raw) < 0) {
        return -EIO;
    }
    return 0;
}

/**
 *
 * @param[out] status
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device
 */
int tmc5160::driver_status_get(enum driver_status &status) {
    union reg_gstat reg_gstat = {0};
    union reg_drv_status reg_drv_status = {0};
    if (register_read(reg::GSTAT, reg_gstat.raw) < 0) {
        return -EIO;
    }
    if (register_read(reg::DRV_STATUS, reg_drv_status.raw) < 0) {
        return -EIO;
    }

    if (reg_gstat.fields.uv_cp) {
        status = DRIVER_STATUS_CP_UV;
    } else if (reg_drv_status.fields.s2vsa) {
        status = DRIVER_STATUS_S2VSA;
    } else if (reg_drv_status.fields.s2vsb) {
        status = DRIVER_STATUS_S2VSB;
    } else if (reg_drv_status.fields.s2ga) {
        status = DRIVER_STATUS_S2GA;
    } else if (reg_drv_status.fields.s2gb) {
        status = DRIVER_STATUS_S2GB;
    } else if (reg_drv_status.fields.ot) {
        status = DRIVER_STATUS_OT;
    } else if (reg_gstat.fields.drv_err) {
        status = DRIVER_STATUS_OTHER_ERR;
    } else if (reg_drv_status.fields.otpw) {
        status = DRIVER_STATUS_OTPW;
    } else {
        status = DRIVER_STATUS_OK;
    }
    return 0;
}

/**
 * Set the run and hold motor currents in amps RMS.
 *
 * The function picks the best combination of VSENSE (in CHOPCONF), GLOBAL_SCALER, and the
 * IRUN / IHOLD fields to hit the requested currents as closely as possible, based on the
 * sense resistor value provided in the config at setup() time.
 *
 * Tips:
 *  - IRUN and IHOLD share the same GLOBAL_SCALER and VSENSE, so the run current ultimately
 *    drives the available resolution.
 *  - For the lowest noise and best microstep accuracy, aim for IRUN that ends up close to
 *    its maximum (CS=31). This function does that automatically.
 *  - Setting ihold to a value greater than irun is allowed (the TMC5160 does not care).
 *
 * @param[in] irun_amps_rms Target run current in amps RMS. Must be strictly positive.
 * @param[in] ihold_amps_rms Target hold current in amps RMS. 0 is allowed and disables the motor current at standstill (see PWMCONF.freewheel for behavior).
 * @param[in] iholddelay Delay before power down, in multiples of 2^18 clocks (0..15). 0 means instant, higher means smoother transition.
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EINVAL If the requested current cannot be achieved even at full scale (target too high) or if iholddelay is out of range.
 *  -EIO If there was an error communicating with the device.
 * @see Datasheet §9 "Motor Current Control"
 */
int tmc5160::current_set(const float irun_amps_rms, const float ihold_amps_rms, const uint8_t iholddelay) {
    int res;

    /* Validate arguments */
    if (irun_amps_rms < 0.0f || ihold_amps_rms < 0.0f) {
        return -EINVAL;
    }
    if (iholddelay > 15) {
        return -EINVAL;
    }

    /* Compute the full-scale RMS current for each VSENSE setting
     * Formula: I_rms = (V_fs / (R_sense + 0.02)) / sqrt(2) at GLOBAL_SCALER=256 and CS=31/31.
     * V_fs = 0.325 V for VSENSE=0, 0.180 V for VSENSE=1. */
    const float rplus = m_rsense + 0.02f;
    const float i_fs_vsense0 = 0.325f / 1.41421356f / rplus;
    const float i_fs_vsense1 = 0.180f / 1.41421356f / rplus;

    /* Pick VSENSE
     * Prefer VSENSE=1 (higher resolution for low currents) when the run current fits in its range,
     * otherwise fall back to VSENSE=0. */
    uint8_t vsense;
    float i_fs;
    if (irun_amps_rms <= i_fs_vsense1) {
        vsense = 1;
        i_fs = i_fs_vsense1;
    } else if (irun_amps_rms <= i_fs_vsense0) {
        vsense = 0;
        i_fs = i_fs_vsense0;
    } else {
        return -EINVAL;
    }

    /* Pick GLOBAL_SCALER to place IRUN near CS=31 for best resolution.
     * At CS=31 (factor 32/32), I_rms = (GS / 256) * i_fs
     * so ideal GS = 256 * irun / i_fs. Clamp to [32, 256]. */
    int32_t gs = (int32_t)roundf(256.0f * irun_amps_rms / i_fs);
    if (gs < 32) {
        gs = 32;
    } else if (gs > 256) {
        gs = 256;
    }

    /* Compute CS for IRUN and IHOLD given the chosen GS and VSENSE.
     * I_rms = (GS / 256) * ((CS + 1) / 32) * i_fs
     * so CS = round(32 * I_rms * 256 / (GS * i_fs)) - 1, clamped to [0, 31]. */
    const float cs_scale = 32.0f * 256.0f / ((float)gs * i_fs);
    int32_t cs_irun = (int32_t)roundf(irun_amps_rms * cs_scale) - 1;
    int32_t cs_ihold = (int32_t)roundf(ihold_amps_rms * cs_scale) - 1;
    if (cs_irun < 0) {
        cs_irun = 0;
    } else if (cs_irun > 31) {
        cs_irun = 31;
    }
    if (cs_ihold < 0) {
        cs_ihold = 0;
    } else if (cs_ihold > 31) {
        cs_ihold = 31;
    }

    /* Write VSENSE into CHOPCONF (read-modify-write) */
    union reg_chopconf reg_chopconf_new;
    reg_chopconf_new.raw = m_reg_chopconf_cache.raw;
    reg_chopconf_new.fields.vsense = vsense;

    /* Build IHOLD_IRUN */
    union reg_ihold_irun reg_ihold_irun_new = {0};
    reg_ihold_irun_new.fields.ihold = (uint8_t)cs_ihold;
    reg_ihold_irun_new.fields.irun = (uint8_t)cs_irun;
    reg_ihold_irun_new.fields.iholddelay = iholddelay;

    /* Write registers
     * GLOBAL_SCALER uses 0 to mean "256" (full scale). Write 0 if we picked 256 for cleanliness. */
    const uint32_t gs_reg = (gs == 256) ? 0 : (uint32_t)gs;
    res = 0;
    res |= register_write(reg::CHOPCONF, reg_chopconf_new.raw);
    res |= register_write(reg::GLOBAL_SCALER, gs_reg);
    res |= register_write(reg::IHOLD_IRUN, reg_ihold_irun_new.raw);
    if (res < 0) {
        return -EIO;
    }

    /* Refresh the cached CHOPCONF so driver_enable/disable keeps the new VSENSE */
    m_reg_chopconf_cache.raw = reg_chopconf_new.raw;

    /* Return success */
    return 0;
}

/**
 * Read back the run and hold currents currently configured on the chip, in amps RMS.
 *
 * The values are computed from the chip's GLOBAL_SCALER, IHOLD_IRUN and CHOPCONF.VSENSE
 * registers using the sense resistor value provided in the config at setup() time.
 *
 * @param[out] irun_amps_rms Current run current in amps RMS.
 * @param[out] ihold_amps_rms Current hold current in amps RMS.
 * @param[out] iholddelay Current iholddelay field (0..15).
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device.
 * @see Datasheet §9 "Motor Current Control"
 */
int tmc5160::current_get(float &irun_amps_rms, float &ihold_amps_rms, uint8_t &iholddelay) {

    /* Read the relevant registers */
    uint32_t reg_global_scaler_raw;
    union reg_ihold_irun reg_ihold_irun_cur = {0};
    union reg_chopconf reg_chopconf_cur = {0};
    if (register_read(reg::GLOBAL_SCALER, reg_global_scaler_raw) < 0) {
        return -EIO;
    }
    if (register_read(reg::IHOLD_IRUN, reg_ihold_irun_cur.raw) < 0) {
        return -EIO;
    }
    if (register_read(reg::CHOPCONF, reg_chopconf_cur.raw) < 0) {
        return -EIO;
    }

    /* GLOBAL_SCALER is an 8-bit field, and the value 0 means 256 (full scale) */
    uint32_t gs = reg_global_scaler_raw & 0xFF;
    if (gs == 0) {
        gs = 256;
    }

    /* Compute the full-scale RMS current for the current VSENSE setting */
    const float rplus = m_rsense + 0.02f;
    const float v_fs = reg_chopconf_cur.fields.vsense ? 0.180f : 0.325f;
    const float i_fs = v_fs / 1.41421356f / rplus;

    /* Reverse the formula I_rms = (GS / 256) * ((CS + 1) / 32) * i_fs */
    const float factor = ((float)gs / 256.0f) * i_fs / 32.0f;
    irun_amps_rms = factor * (float)(reg_ihold_irun_cur.fields.irun + 1);
    ihold_amps_rms = factor * (float)(reg_ihold_irun_cur.fields.ihold + 1);
    iholddelay = reg_ihold_irun_cur.fields.iholddelay;

    /* Return success */
    return 0;
}

/**
 * Configure the SpreadCycle chopper parameters.
 *
 * SpreadCycle is the classic high-performance chopper mode. The default values in the
 * @ref spreadcycle_params struct are a safe starting point that works for most motors.
 * For best low-noise and low-vibration results, the TMC5160-Calculator tool from Trinamic
 * can suggest motor-specific values.
 *
 * Also switches the chopper to SpreadCycle (CHM=0) and clears the SpreadCycle-incompatible bits.
 *
 * @param[in] params The SpreadCycle parameters to apply.
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EINVAL If any parameter is out of range, or if HSTRT + HEND > 16 (hardware constraint).
 *  -EIO If there was an error communicating with the device.
 * @see Datasheet §7 "SpreadCycle and Classic Chopper"
 */
int tmc5160::spreadcycle_set(const struct spreadcycle_params &params) {

    /* Validate ranges */
    if (params.toff < 1 || params.toff > 15) {
        return -EINVAL;
    }
    if (params.tbl > 3) {
        return -EINVAL;
    }
    if (params.hstrt > 7) {
        return -EINVAL;
    }
    if (params.hend > 15) {
        return -EINVAL;
    }
    if (params.tpfd > 15) {
        return -EINVAL;
    }

    /* HSTRT raw is 0..7 meaning 1..8, HEND raw is 0..15 meaning -3..+12.
     * The datasheet §7.1 requires (decoded HSTRT) + (decoded HEND) <= 16,
     * which in raw field terms means hstrt + hend <= 18. */
    if ((uint16_t)params.hstrt + (uint16_t)params.hend > 18) {
        return -EINVAL;
    }

    /* Read-modify-write CHOPCONF to preserve the other fields (vsense, mres, intpol, ...) */
    union reg_chopconf reg_chopconf_new;
    reg_chopconf_new.raw = m_reg_chopconf_cache.raw;
    reg_chopconf_new.fields.toff = params.toff;
    reg_chopconf_new.fields.tbl = params.tbl;
    reg_chopconf_new.fields.hstrt_tfd = params.hstrt;
    reg_chopconf_new.fields.hend_offset = params.hend;
    reg_chopconf_new.fields.tpfd = params.tpfd;
    reg_chopconf_new.fields.chm = 0;       // Force SpreadCycle mode
    reg_chopconf_new.fields.tfd_3 = 0;     // Only used in chm=1
    reg_chopconf_new.fields.disfdcc = 0;   // Only used in chm=1
    reg_chopconf_new.fields.vhighchm = 0;  // Disable auto-switch to constant off-time chopper
    if (register_write(reg::CHOPCONF, reg_chopconf_new.raw) < 0) {
        return -EIO;
    }

    /* Update the cached CHOPCONF */
    m_reg_chopconf_cache.raw = reg_chopconf_new.raw;

    /* Return success */
    return 0;
}

/**
 * Enable StealthChop at all speeds.
 *
 * Sets GCONF.en_pwm_mode = 1 and TPWMTHRS = 0 so the driver uses StealthChop regardless of
 * motor velocity. StealthChop is the silent voltage-PWM chopper mode, best for quiet operation
 * but with reduced torque at higher speeds.
 *
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device.
 * @see Datasheet §8 "StealthChop"
 */
int tmc5160::stealthchop_enable_always(void) {
    int res;

    /* Read-modify-write GCONF to set en_pwm_mode without touching the other flags */
    union reg_gconf reg_gconf_cur = {0};
    if (register_read(reg::GCONF, reg_gconf_cur.raw) < 0) {
        return -EIO;
    }
    reg_gconf_cur.fields.en_pwm_mode = 1;

    /* Write TPWMTHRS=0 first, then enable en_pwm_mode
     * TPWMTHRS=0 means StealthChop stays active at any velocity (no switch to SpreadCycle). */
    res = 0;
    res |= register_write(reg::TPWMTHRS, 0);
    res |= register_write(reg::GCONF, reg_gconf_cur.raw);
    if (res < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 * Enable StealthChop below a given speed, SpreadCycle above.
 *
 * Sets GCONF.en_pwm_mode = 1 and configures TPWMTHRS so the driver runs StealthChop while
 * the motor speed is below @p speed, and switches to SpreadCycle when the speed is higher.
 * This is the recommended hybrid mode: silent at low speeds, full torque at high speeds.
 *
 * @param[in] speed Threshold speed in motor steps per second (not microsteps). Must be positive.
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EINVAL If the speed is zero or negative.
 *  -EIO If there was an error communicating with the device.
 * @see Datasheet §12 "Velocity Based Mode Control"
 */
int tmc5160::stealthchop_enable_under(const float speed) {
    int res;

    /* Validate argument */
    if (speed <= 0.0f) {
        return -EINVAL;
    }

    /* Convert speed to a TSTEP threshold value
     * The driver compares TSTEP to TPWMTHRS: TSTEP < TPWMTHRS means the motor is running
     * faster than the threshold (smaller TSTEP = higher speed). */
    const uint32_t tpwmthrs_value = tmc5160_clamp_u32(convert_speed_to_tstep(speed), 0xFFFFF);

    /* Read-modify-write GCONF to set en_pwm_mode without touching the other flags */
    union reg_gconf reg_gconf_cur = {0};
    if (register_read(reg::GCONF, reg_gconf_cur.raw) < 0) {
        return -EIO;
    }
    reg_gconf_cur.fields.en_pwm_mode = 1;

    /* Program TPWMTHRS first, then enable en_pwm_mode */
    res = 0;
    res |= register_write(reg::TPWMTHRS, tpwmthrs_value);
    res |= register_write(reg::GCONF, reg_gconf_cur.raw);
    if (res < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 * Disable StealthChop and use SpreadCycle at all speeds.
 *
 * Clears GCONF.en_pwm_mode. TPWMTHRS becomes irrelevant when StealthChop is disabled.
 *
 * @return 0 in case of success, or a negative error code otherwise, in particular:
 *  -EIO If there was an error communicating with the device.
 */
int tmc5160::stealthchop_disable(void) {

    /* Read-modify-write GCONF to clear en_pwm_mode without touching the other flags */
    union reg_gconf reg_gconf_cur = {0};
    if (register_read(reg::GCONF, reg_gconf_cur.raw) < 0) {
        return -EIO;
    }
    reg_gconf_cur.fields.en_pwm_mode = 0;
    if (register_write(reg::GCONF, reg_gconf_cur.raw) < 0) {
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 *
 * @see Datasheet, section 14.1 Real World Unit Conversion
 */
uint32_t tmc5160::convert_velocity_to_tmc(const float velocity) {
    return (int32_t)(velocity / ((float)m_fclk / (float)(1ul << 24)) * (float)m_ustep_per_step);
}

/**
 *
 * @see Datasheet, section 14.1 Real World Unit Conversion
 */
uint32_t tmc5160::convert_acceleration_to_tmc(const float acceleration) {
    return (int32_t)(acceleration / ((float)m_fclk * (float)m_fclk / (512.0f * 256.0f) / (float)(1ul << 24)) * (float)m_ustep_per_step);
}

/**
 * Convert a threshold speed (steps/second) into a TSTEP value for registers
 * TPWMTHRS, TCOOLTHRS, THIGH.
 * @see Datasheet, section 12 Velocity Based Mode Control
 */
uint32_t tmc5160::convert_speed_to_tstep(const float speed) {
    if (speed <= 0.0f) {
        return 0;
    }
    float tstep = (float)m_fclk / (speed * 256.0f);
    if (tstep < 0.0f) {
        return 0;
    } else if (tstep > 1048575.0f) {
        return 1048575;
    } else {
        return (uint32_t)tstep;
    }
}

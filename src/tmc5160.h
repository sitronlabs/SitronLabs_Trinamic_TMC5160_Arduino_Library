#ifndef TMC5160_H
#define TMC5160_H

/* Arduino libraries */
#include <Arduino.h>
#include <SPI.h>

/* C/C++ libraries */
#include <errno.h>

/**
 *
 * @note Use -Wno-packed-bitfield-compat
 */
class tmc5160 {

   public:
    /* Register addresses */
    enum reg {

        /* General configuration registers */
        GCONF = 0x00,            // Global configuration flags
        GSTAT = 0x01,            // Global status flags
        IFCNT = 0x02,            // UART transmission counter
        SLAVECONF = 0x03,        // UART slave configuration
        IO_INPUT_OUTPUT = 0x04,  // Read input / write output pins
        X_COMPARE = 0x05,        // Position comparison register
        OTP_PROG = 0x06,         // OTP programming register
        OTP_READ = 0x07,         // OTP read register
        FACTORY_CONF = 0x08,     // Factory configuration (clock trim)
        SHORT_CONF = 0x09,       // Short detector configuration
        DRV_CONF = 0x0A,         // Driver configuration
        GLOBAL_SCALER = 0x0B,    // Global scaling of motor current
        OFFSET_READ = 0x0C,      // Offset calibration results

        /* Velocity dependent driver feature control registers */
        IHOLD_IRUN = 0x10,  // Driver current control
        TPOWERDOWN = 0x11,  // Delay before power down
        TSTEP = 0x12,       // Actual time between microsteps
        TPWMTHRS = 0x13,    // Upper velocity for stealthChop voltage PWM mode
        TCOOLTHRS = 0x14,   // Lower threshold velocity for switching on smart energy coolStep and stallGuard feature
        THIGH = 0x15,       // Velocity threshold for switching into a different chopper mode and fullstepping

        /* Ramp generator motion control registers */
        RAMPMODE = 0x20,   // Driving mode (Velocity, Positioning, Hold)
        XACTUAL = 0x21,    // Actual motor position
        VACTUAL = 0x22,    // Actual motor velocity from ramp generator
        VSTART = 0x23,     // Motor start velocity (unsigned).
        A_1 = 0x24,        // First acceleration between VSTART and V1
        V_1 = 0x25,        // First acceleration/deceleration phase target velocity
        AMAX = 0x26,       // Second acceleration between V1 and VMAX
        VMAX = 0x27,       // Target velocity in velocity mode. It can be changed any time during a motion.
        DMAX = 0x28,       // Deceleration between VMAX and V1
        D_1 = 0x2A,        // Deceleration between V1 and VSTOP. Attention: Do not set 0 in positioning mode, even if V1=0!
        VSTOP = 0x2B,      // Motor stop velocity (unsigned). Attention: Set VSTOP > VSTART! Attention: Do not set 0 in positioning mode, minimum 10 recommend!
        TZEROWAIT = 0x2C,  // Waiting time after ramping down to zero velocity before next movement or direction inversion can start.
        XTARGET = 0x2D,    // Target position for ramp mode

        /* Ramp generator driver feature control registers */
        VDCMIN = 0x33,     // Velocity threshold for enabling automatic commutation dcStep
        SW_MODE = 0x34,    // Switch mode configuration
        RAMP_STAT = 0x35,  // Ramp status and switch event status
        XLATCH = 0x36,     // Ramp generator latch position upon programmable switch event

        /* Encoder registers */
        ENCMODE = 0x38,        // Encoder configuration and use of N channel
        X_ENC = 0x39,          // Actual encoder position
        ENC_CONST = 0x3A,      // Accumulation constant
        ENC_STATUS = 0x3B,     // Encoder status information
        ENC_LATCH = 0x3C,      // Encoder position latched on N event
        ENC_DEVIATION = 0x3D,  // Maximum number of steps deviation between encoder counter and XACTUAL for deviation warning

        /* Motor driver registers */
        MSLUT_0_7 = 0x60,   // Microstep table entries. Add 0...7 for the next registers
        MSLUTSEL = 0x68,    // Look up table segmentation definition
        MSLUTSTART = 0x69,  // Absolute current at microstep table entries 0 and 256
        MSCNT = 0x6A,       // Actual position in the microstep table
        MSCURACT = 0x6B,    // Actual microstep current
        CHOPCONF = 0x6C,    // Chopper and driver configuration
        COOLCONF = 0x6D,    // coolStep smart current control register and stallGuard2 configuration
        DCCTRL = 0x6E,      // dcStep automatic commutation configuration register
        DRV_STATUS = 0x6F,  // stallGuard2 value and driver error flags
        PWMCONF = 0x70,     // stealthChop voltage PWM mode chopper configuration
        PWM_SCALE = 0x71,   // Results of stealthChop amplitude regulator.
        PWM_AUTO = 0x72,    // Automatically determined PWM config values
        LOST_STEPS = 0x73,  // Number of input steps skipped due to dcStep. only with SD_MODE = 1
    };

    /* Register description */
    union reg_gconf {
        uint32_t raw;
        struct {
            uint8_t recalibrate : 1;             // Zero crossing recalibration during driver disable
            uint8_t faststandstill : 1;          // Timeout for step execution until standstill detection
            uint8_t en_pwm_mode : 1;             // Enable stealthChop voltage PWM mode
            uint8_t multistep_filt : 1;          // Enable step input filtering for stealthChop optimization with external step source
            uint8_t shaft : 1;                   // Normal / inverse motor direction
            uint8_t diag0_error : 1;             // Enable DIAG0 active on driver errors
            uint8_t diag0_otpw : 1;              // Enable DIAG0 active on driver over temperature prewarning
            uint8_t diag0_stall_step : 1;        // SD_MODE=1: active on motor stall. SD_MODE=0: STEP output
            uint8_t diag1_stall_dir : 1;         // SD_MODE=1: active on motor stall. SD_MODE=0: DIR output
            uint8_t diag1_index : 1;             // Enable DIAG1 active on index position
            uint8_t diag1_onstate : 1;           // Enable DIAG1 active when chopper is on
            uint8_t diag1_steps_skipped : 1;     // Enable output toggle when steps are skipped in dcStep mode
            uint8_t diag0_int_pushpull : 1;      // Enable SWN_DIAG0 push pull output
            uint8_t diag1_poscomp_pushpull : 1;  // Enable SWP_DIAG1 push pull output
            uint8_t small_hysteresis : 1;        // Set small hysteresis for step frequency comparison
            uint8_t stop_enable : 1;             // Enable emergency stop
            uint8_t direct_mode : 1;             // Enable direct motor coil current and polarity control
            uint8_t test_mode : 1;               // Not for normal use
            uint8_t : 6;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_gstat {
        uint32_t raw;
        struct {
            uint8_t reset : 1;    // Indicates that the IC has been reset since the last read access to GSTAT
            uint8_t drv_err : 1;  // Indicates that the driver has been shut down due to overtemperature or short circuit detection since the last read access
            uint8_t uv_cp : 1;    // Indicates an undervoltage on the charge pump. The driver is disabled in this case.
            uint8_t : 5;
            uint8_t : 8;
            uint8_t : 8;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_io_input_output {
        uint32_t raw;
        struct {
            uint8_t refl_step : 1;
            uint8_t refr_dir : 1;
            uint8_t encb_dcen_cfg4 : 1;
            uint8_t enca_dcin_cfg5 : 1;
            uint8_t drv_enn : 1;
            uint8_t enc_n_dco_cfg6 : 1;
            uint8_t sd_mode : 1;  // 1=External step and dir source
            uint8_t swcomp_in : 1;
            uint16_t : 16;
            uint8_t version : 8;  // Expected to be 0x30 for TMC5160
        } __attribute__((packed)) fields;
    };
    union reg_short_conf {
        uint32_t raw;
        struct {
            uint8_t s2vs_level : 4;  // Short to VS detector for low side FETs sensitivity (4=highest sensitivity, 15=lowest)
            uint8_t : 4;
            uint8_t s2g_level : 4;  // Short to GND detector for high side FETs sensitivity
            uint8_t : 4;
            uint8_t shortfilter : 2;  // Spike filtering bandwidth for short detection
            uint8_t shortdelay : 1;   // Short detection delay
            uint8_t : 5;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_drv_conf {
        uint32_t raw;
        struct {
            uint8_t bbmtime : 5;  // Break before make delay (0 to 24)
            uint8_t : 3;
            uint8_t bbmclks : 4;  // Digital BBM Time in clock cycles
            uint8_t : 4;
            uint8_t otselect : 2;     // Over temperature level for bridge disable
            uint8_t drvstrength : 2;  // Gate drivers current
            uint8_t filt_isense : 2;  // Filter time constant of sense amplifier
            uint8_t : 2;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_ihold_irun {
        uint32_t raw;
        struct {
            uint8_t ihold : 5;  // Standstill current (0=1/32...31=32/32)
            uint8_t : 3;
            uint8_t irun : 5;  // Motor run current (0=1/32...31=32/32)
            uint8_t : 3;
            uint8_t iholddelay : 4;  // Delay between stand still and motor current power down
            uint8_t : 4;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_tpowerdown {
        uint32_t raw;
        struct {
            uint8_t tpowerdown : 8;
            uint8_t : 8;
            uint8_t : 8;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_tpwmthrs {
        uint32_t raw;
        struct {
            uint32_t tpwmthrs : 20;
            uint8_t : 4;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_sw_mode {
        uint32_t raw;
        struct {
            uint8_t stop_l_enable : 1;     // Enable automatic motor stop during active left reference switch input
            uint8_t stop_r_enable : 1;     // Enable automatic motor stop during active right reference switch input
            uint8_t pol_stop_l : 1;        // Sets the active polarity of the left reference switch input (1=inverted, low active)
            uint8_t pol_stop_r : 1;        // Sets the active polarity of the right reference switch input (1=inverted, low active)
            uint8_t swap_lr : 1;           // Swap the left and the right reference switch inputs REFL and REFR
            uint8_t latch_l_active : 1;    // Latch position to XLATCH upon active going edge on REFL
            uint8_t latch_l_inactive : 1;  // Latch position to XLATCH upon inactive going edge on REFL
            uint8_t latch_r_active : 1;    // Latch position to XLATCH upon active going edge on REFR
            uint8_t latch_r_inactive : 1;  // Latch position to XLATCH upon inactive going edge on REFR
            uint8_t en_latch_encoder : 1;  // Latch encoder position to ENC_LATCH upon reference switch event
            uint8_t sg_stop : 1;           // Enable stop by stallGuard2
            uint8_t en_softstop : 1;       // Enable soft stop upon a stop event (uses the deceleration ramp settings)
            uint8_t : 4;
            uint8_t : 8;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_ramp_stat {
        uint32_t raw;
        struct {
            uint8_t status_stop_l : 1;      // Reference switch left status (1=active)
            uint8_t status_stop_r : 1;      // Reference switch right status (1=active)
            uint8_t status_latch_l : 1;     // Latch left ready
            uint8_t status_latch_r : 1;     // Latch right ready
            uint8_t event_stop_l : 1;       // Signals an active stop left condition due to stop switch
            uint8_t event_stop_r : 1;       // Signals an active stop right condition due to stop switch
            uint8_t event_stop_sg : 1;      // Signals an active StallGuard2 stop event
            uint8_t event_pos_reached : 1;  // Signals that the target position has been reached
            uint8_t velocity_reached : 1;   // Signals that the target velocity is reached
            uint8_t position_reached : 1;   // Signals that the target position is reached
            uint8_t vzero : 1;              // Signals that the actual velocity is 0
            uint8_t t_zerowait_active : 1;  // Signals that TZEROWAIT is active after a motor stop
            uint8_t second_move : 1;        // Signals that the automatic ramp required moving back in the opposite direction
            uint8_t status_sg : 1;          // Signals an active stallGuard2 input
            uint8_t : 2;
            uint8_t : 8;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_encmode {
        uint32_t raw;
        struct {
            uint8_t pol_a : 1;            // Required A polarity for an N channel event
            uint8_t pol_b : 1;            // Required B polarity for an N channel event
            uint8_t pol_n : 1;            // Defines active polarity of N (0=low active, 1=high active)
            uint8_t ignore_ab : 1;        // Ignore A and B polarity for N channel event
            uint8_t clr_cont : 1;         // Always latch or latch and clear X_ENC upon an N event
            uint8_t clr_once : 1;         // Latch or latch and clear X_ENC on the next N event
            uint8_t sensitivity : 2;      // N channel event sensitivity
            uint8_t clr_enc_x : 1;        // Clear encoder counter X_ENC upon N-event
            uint8_t latch_x_act : 1;      // Also latch XACTUAL position together with X_ENC
            uint8_t enc_sel_decimal : 1;  // 0=binary mode, 1=decimal mode
            uint8_t : 5;
            uint8_t : 8;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_enc_status {
        uint32_t raw;
        struct {
            uint8_t n_event : 1;         // N event detected
            uint8_t deviation_warn : 1;  // Deviation between XACTUAL and X_ENC detected
            uint8_t : 6;
            uint8_t : 8;
            uint8_t : 8;
            uint8_t : 8;
        } __attribute__((packed)) fields;
    };
    union reg_chopconf {
        uint32_t raw;
        struct {
            uint8_t toff : 4;         // Off time setting controls duration of slow decay phase. 0 = driver disabled
            uint8_t hstrt_tfd : 3;    // chm=0: hysteresis start value HSTRT, chm=1: fast decay time setting bits 0..2
            uint8_t hend_offset : 4;  // chm=0: hysteresis low value HEND, chm=1: sine wave offset
            uint8_t tfd_3 : 1;        // chm=1: fast decay time setting bit 3
            uint8_t disfdcc : 1;      // chm=1: disable current comparator usage for termination of the fast decay cycle
            uint8_t rndtf : 1;        // Enable random modulation of chopper TOFF time
            uint8_t chm : 1;          // Chopper mode (0=spreadCycle, 1=constant off time with fast decay)
            uint8_t tbl : 2;          // Comparator blank time select
            uint8_t vsense : 1;       // Sense resistor voltage sensitivity (0=low, 1=high)
            uint8_t vhighfs : 1;      // Enable switching to fullstep when VHIGH is exceeded
            uint8_t vhighchm : 1;     // Enable switching to chm=1 and fd=0 when VHIGH is exceeded
            uint8_t tpfd : 4;         // Passive fast decay time
            uint8_t mres : 4;         // Microstep resolution
            uint8_t intpol : 1;       // Enable interpolation to 256 microsteps
            uint8_t dedge : 1;        // Enable double edge step pulses
            uint8_t diss2g : 1;       // Disable short to GND protection
            uint8_t diss2vs : 1;      // Disable short to supply protection
        } __attribute__((packed)) fields;
    };
    union reg_coolconf {
        uint32_t raw;
        struct {
            uint8_t semin : 4;  // Minimum stallGuard2 value for smart current control and smart current enable
            uint8_t : 1;
            uint8_t seup : 2;  // Current increment step width
            uint8_t : 1;
            uint8_t semax : 4;  // stallGuard2 hysteresis value for smart current control
            uint8_t : 1;
            uint8_t sedn : 2;    // Current decrement step speed
            uint8_t seimin : 1;  // Minimum current for smart current control
            uint8_t sgt : 7;     // stallGuard2 threshold value
            uint8_t : 1;
            uint8_t sfilt : 1;  // Enable stallGuard2 filter
            uint8_t : 7;
        } __attribute__((packed)) fields;
    };
    union reg_drv_status {
        uint32_t raw;
        struct {
            uint16_t sg_result : 9;  // stallGuard2 result or motor temperature estimation in stand still (bits 0..8)
            uint8_t : 3;
            uint8_t s2vsa : 1;      // Short to supply indicator phase A
            uint8_t s2vsb : 1;      // Short to supply indicator phase B
            uint8_t stealth : 1;    // StealthChop indicator
            uint8_t fsactive : 1;   // Full step active indicator
            uint8_t cs_actual : 5;  // Actual motor current / smart energy current (bits 16..20)
            uint8_t : 3;
            uint8_t stallguard : 1;  // stallGuard2 status (bit 24)
            uint8_t ot : 1;          // Overtemperature flag (bit 25)
            uint8_t otpw : 1;        // Overtemperature pre-warning flag (bit 26)
            uint8_t s2ga : 1;        // Short to ground indicator phase A (bit 27)
            uint8_t s2gb : 1;        // Short to ground indicator phase B (bit 28)
            uint8_t ola : 1;         // Open load indicator phase A (bit 29)
            uint8_t olb : 1;         // Open load indicator phase B (bit 30)
            uint8_t stst : 1;        // Standstill indicator (bit 31)
        } __attribute__((packed)) fields;
    };
    union reg_pwmconf {
        uint32_t raw;
        struct {
            uint8_t pwm_ofs : 8;        // User defined PWM amplitude (offset)
            uint8_t pwm_grad : 8;       // User defined PWM amplitude (gradient)
            uint8_t pwm_freq : 2;       // PWM frequency selection
            uint8_t pwm_autoscale : 1;  // Enable PWM automatic amplitude scaling
            uint8_t pwm_autograd : 1;   // PWM automatic gradient adaptation
            uint8_t freewheel : 2;      // Stand still option when IHOLD=0
            uint8_t : 2;
            uint8_t pwm_reg : 4;  // Regulation loop gradient
            uint8_t pwm_lim : 4;  // PWM automatic scale amplitude limit when switching on
        } __attribute__((packed)) fields;
    };

    /* Register access (NVI: caching in register_read / register_write, transport in *_transport) */
    virtual int status_read(uint8_t &status) = 0;
    int register_read(const uint8_t address, uint32_t &data);
    int register_write(const uint8_t address, const uint32_t data);

    /* Setup */
    struct config {
        float rsense = 0.075f;                                      //!< Sense resistor value in ohms. Default is 75 mΩ (as on the TMC5160-BOB evaluation board).
        union reg_gconf reg_gconf = {.raw = 0x00000004};            // EN_PWM_MODE=1 enables StealthChop (with default PWMCONF)
        union reg_drv_conf reg_drv_conf = {.raw = 0x00080400};      // DRV_CONF: BBMTIME=0, BBMCLKS=4, OTSELECT=0, DRVSTRENGTH=2, FILT_ISENSE=0
        uint32_t reg_global_scaler = 128;                           // Global current scaling (32 to 256, 0 disables scaling and uses full scale)
        union reg_ihold_irun reg_ihold_irun = {.raw = 0x00071F0A};  // IHOLD=10, IRUN=31, IHOLDDELAY=7
        union reg_chopconf reg_chopconf = {.raw = 0x000100C3};      // TOFF=3, HSTRT=4, HEND=1, TBL=2, CHM=0 (SpreadCycle)
        union reg_tpowerdown reg_tpowerdown = {.raw = 0x0000000A};  // TPOWERDOWN=10
        union reg_tpwmthrs reg_tpwmthrs = {.raw = 0x000001F4};      // TPWMTHRS=500
        union reg_pwmconf reg_pwmconf = {.raw = 0xC40C001E};        // Reset default with pwm_autoscale=1, pwm_autograd=1
        union reg_short_conf reg_short_conf = {.raw = 0x00010606};  // S2VS_LEVEL=6, S2G_LEVEL=6, SHORTFILTER=1, SHORTDELAY=0
    };
    int setup(struct config &config);
    int speed_ramp_set(const float vstart, const float vstop, const float vtrans);
    int speed_limit_set(const float speed);
    int acceleration_limit_set(const float acceleration);

    /**
     * Parameters controlling the SpreadCycle chopper.
     * Reset/default values taken from the TMC5160 quick config guide.
     * @see Datasheet §7 "SpreadCycle and Classic Chopper" for tuning guidance.
     */
    struct spreadcycle_params {
        uint8_t toff = 3;   //!< Off-time (1..15). Controls the chopper frequency. 0 would disable the driver entirely.
        uint8_t tbl = 2;    //!< Blank time (0..3). Should typically stay at 2.
        uint8_t hstrt = 4;  //!< Hysteresis start value (0..7).
        uint8_t hend = 1;   //!< Hysteresis end value, offset by -3 (0..15 means -3..+12). Must satisfy hstrt + hend <= 16.
        uint8_t tpfd = 4;   //!< Passive fast decay time (0..15). TMC5160-specific, helps dampen motor resonances at mid velocities.
    };

    /* Current control (in amps RMS) */
    int current_set(const float irun_amps_rms, const float ihold_amps_rms, const uint8_t iholddelay = 7);
    int current_get(float &irun_amps_rms, float &ihold_amps_rms, uint8_t &iholddelay);

    /* Chopper tuning */
    int spreadcycle_set(const struct spreadcycle_params &params);

    /* StealthChop control */
    int stealthchop_enable_always(void);
    int stealthchop_enable_under(const float speed);
    int stealthchop_disable(void);

    /* Movement start or stop */
    int move_to_position(const float position);
    int move_at_velocity(const float velocity);
    int move_stop(void);

    /* Position */
    int position_current_get(float &position);
    int position_current_set(const float position);
    int position_latched_get(float &position);
    int position_target_get(float &position);

    /* Velocity */
    int velocity_current_get(float &velocity);

    /* Driver enable or disable */
    int driver_enable(void);
    int driver_disable(void);

    /* Target reached or not */
    int target_position_reached_is(void);
    int target_velocity_reached_is(void);

    /* Reference switches */
    int reference_swap(bool swap);
    int reference_l_polarity_set(bool active_high);
    int reference_r_polarity_set(bool active_high);
    int reference_l_active_get(void);
    int reference_r_active_get(void);
    int reference_l_latch_enable(bool polarity);
    int reference_r_latch_enable(bool polarity);
    int reference_l_latch_get(float &position);
    int reference_r_latch_get(float &position);

    /* Encoder */
    int encoder_position_get(float &position);
    int encoder_position_set(const float position);
    int encoder_resolution_set(const int32_t motor_steps, const int32_t encoder_resolution, const bool inverted = false);
    int encoder_deviation_set(const int32_t steps);
    int encoder_deviation_detected_is(void);
    int encoder_deviation_clear(void);

    /* Driver status */
    enum driver_status {
        DRIVER_STATUS_OK,         // No error condition
        DRIVER_STATUS_CP_UV,      // Charge pump undervoltage
        DRIVER_STATUS_S2VSA,      // Short to supply phase A
        DRIVER_STATUS_S2VSB,      // Short to supply phase B
        DRIVER_STATUS_S2GA,       // Short to ground phase A
        DRIVER_STATUS_S2GB,       // Short to ground phase B
        DRIVER_STATUS_OT,         // Overtemperature (error)
        DRIVER_STATUS_OTHER_ERR,  // GSTAT drv_err is set but none of the above conditions is found
        DRIVER_STATUS_OTPW,       // Overtemperature pre-warning
    };
    int driver_status_get(enum driver_status &status);

   protected:
    /* Conversion functions */
    uint32_t convert_velocity_to_tmc(const float velocity);
    uint32_t convert_acceleration_to_tmc(const float acceleration);
    uint32_t convert_speed_to_tstep(const float speed);

    /* Transport functions */
    virtual int register_read_transport(const uint8_t address, uint32_t &data) = 0;
    virtual int register_write_transport(const uint8_t address, const uint32_t data) = 0;

    /* Internal states */
    uint8_t m_status_byte = 0x00;
    uint32_t m_fclk = 12000000;           //!< Frequency at which the driver is running in Hz (TMC5160 default is 12 MHz internal clock)
    uint16_t m_ustep_per_step = 256;      //!< Number of microsteps per step
    float m_rsense = 0.075f;              //!< Sense resistor value in ohms, copied from the config at setup() time. Used by current_set / current_get.
    bool m_reference_l_latched = false;   //!<
    bool m_reference_r_latched = false;   //!<
    uint8_t m_chopconf_toff_restore = 0;  //!< TOFF value to restore when driver_enable() is called

    /* Write-only register shadows: value + flag set after a successful register_write from this driver
     * For some of them, the datasheet provides a POR value, so we can set the shadow to the POR value. */
    union reg_short_conf m_reg_short_conf_cache = {.raw = 0};
    bool m_reg_short_conf_cache_valid = false;
    union reg_drv_conf m_reg_drv_conf_cache = {.raw = 0};
    bool m_reg_drv_conf_cache_valid = false;
    uint32_t m_reg_global_scaler_cache = 0;
    bool m_reg_global_scaler_cache_valid = true;
    union reg_ihold_irun m_reg_ihold_irun_cache = {.raw = 0};
    bool m_reg_ihold_irun_cache_valid = false;
    union reg_tpowerdown m_reg_tpowerdown_cache = {.raw = 0x0000000A};
    bool m_reg_tpowerdown_cache_valid = true;
    union reg_tpwmthrs m_reg_tpwmthrs_cache = {.raw = 0};
    bool m_reg_tpwmthrs_cache_valid = false;
    uint32_t m_reg_vstart_cache = 0;
    bool m_reg_vstart_cache_valid = false;
    uint32_t m_reg_a_1_cache = 0;
    bool m_reg_a_1_cache_valid = false;
    uint32_t m_reg_v_1_cache = 0;
    bool m_reg_v_1_cache_valid = false;
    uint32_t m_reg_amax_cache = 0;
    bool m_reg_amax_cache_valid = false;
    uint32_t m_reg_vmax_cache = 0;
    bool m_reg_vmax_cache_valid = false;
    uint32_t m_reg_dmax_cache = 0;
    bool m_reg_dmax_cache_valid = false;
    uint32_t m_reg_d_1_cache = 0;
    bool m_reg_d_1_cache_valid = false;
    uint32_t m_reg_vstop_cache = 1;
    bool m_reg_vstop_cache_valid = true;
    uint32_t m_reg_enc_const_cache = 0;
    bool m_reg_enc_const_cache_valid = false;
    uint32_t m_reg_enc_deviation_cache = 0;
    bool m_reg_enc_deviation_cache_valid = false;
    union reg_pwmconf m_reg_pwmconf_cache = {.raw = 0xC40C001E};
    bool m_reg_pwmconf_cache_valid = true;
};

/**
 *
 */
class tmc5160_spi : public tmc5160 {

   public:
    int setup(struct config &config, SPIClass &spi_library, const int spi_cs_pin, const int spi_speed = 4000000);
    int status_read(uint8_t &status);

   protected:
    int register_read_transport(const uint8_t address, uint32_t &data) override;
    int register_write_transport(const uint8_t address, const uint32_t data) override;

    SPIClass *m_spi_library = NULL;
    uint8_t m_spi_cs_pin;
    SPISettings m_spi_settings;
};

#endif

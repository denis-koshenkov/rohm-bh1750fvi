#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "bh1750.h"
#include "bh1750_private.h"

/** BH1750 I2C address when the ADDR pin is low. */
#define BH1750_I2C_ADDR_ADDR_LOW 0x23
/** BH1750 I2C address when the ADDR pin is high. */
#define BH1750_I2C_ADDR_ADDR_HIGH 0x5C

/** Result of (1.0f / 1.2f). Stored in a constant so that this math is not performed for every raw meas -> lx
 * conversion. */
#define BH1750_CONVERSION_MAGIC 0.8333333f

/** Maximum time it takes to make a measurement in low resolution mode when measurement time is set to default (69) in
 * Mtreg. Taken from the "electrical characteristics" section of the datasheet, p. 2. */
#define BH1750_MAX_L_RES_MEAS_TIME_MS 24
/** Maximum time it takes to make a measurement in high resolution mode or high resolution mode 2 when measurement time
 * is set to default (69) in Mtreg. Taken from the "electrical characteristics" section of the datasheet, p. 2. */
#define BH1750_MAX_H_RES_MEAS_TIME_MS 180

#define BH1750_POWER_DOWN_CMD 0x0
#define BH1750_POWER_ON_CMD 0x01
#define BH1750_RESET_CMD 0x07
#define BH1750_START_CONTINUOUS_MEAS_H_RES_CMD 0x10
#define BH1750_START_CONTINUOUS_MEAS_H_RES2_CMD 0x11
#define BH1750_START_CONTINUOUS_MEAS_L_RES_CMD 0x13
#define BH1750_ONE_TIME_MEAS_H_RES_CMD 0x20
#define BH1750_ONE_TIME_MEAS_H_RES2_CMD 0x21
#define BH1750_ONE_TIME_MEAS_L_RES_CMD 0x23
/* 01000000 in binary. The 5 MSbs are a fixed command to set 3 MSBs of MTreg. */
#define BH1750_SET_MTREG_HIGH_BIT_CMD 0x40U
/* 01100000 in binary. The 3 MSbs are a fixed command to set 5 LSBs of MTreg. */
#define BH1750_SET_MTREG_LOW_BIT_CMD 0x60U

/* Taken from the BH1750 datasheet, p. 11 */
#define BH1750_MIN_MEAS_TIME 31
#define BH1750_MAX_MEAS_TIME 254

#define BH1750_DEFAULT_MEAS_TIME 69
/* Default measurement time is 69 (0x45), in bin: 01000101 */
#define BH1750_DEFAULT_MEAS_TIME_THREE_MSB 0x2U // bin: 010
#define BH1750_DEFAULT_MEAS_TIME_FIVE_LSB 0x5U  // bin: 00101

/**
 * @brief Check if I2C address is a valid BH1750 I2C address.
 *
 * @param i2c_addr I2C address.
 *
 * @retval true I2C address is valid.
 * @retval false I2C address is invalid.
 */
static bool is_valid_i2c_addr(uint8_t i2c_addr)
{
    return (i2c_addr == BH1750_I2C_ADDR_ADDR_LOW) || (i2c_addr == BH1750_I2C_ADDR_ADDR_HIGH);
}

/**
 * @brief Check whether init config is valid.
 *
 * @param[in] cfg Init config.
 *
 * @retval true Init config is valid.
 * @retval false Init config is invalid.
 */
static bool is_valid_init_cfg(const BH1750InitConfig *const cfg)
{
    // clang-format off
    return (
        (cfg->get_instance_memory)
        && (cfg->i2c_write)
        && (cfg->i2c_read)
        && (cfg->start_timer)
        && is_valid_i2c_addr(cfg->i2c_addr)
    );
    // clang-format on
}

/**
 * @brief Check whether measurement mode is valid.
 *
 * @param[in] meas_mode Measurement mode.
 *
 * @retval true Measurement mode is valid.
 * @retval false Measurement mode is invalid.
 */
static bool is_valid_meas_mode(uint8_t meas_mode)
{
    // clang-format off
    return (
        (meas_mode == BH1750_MEAS_MODE_H_RES)
        || (meas_mode == BH1750_MEAS_MODE_H_RES2)
        || (meas_mode == BH1750_MEAS_MODE_L_RES)
    );
    // clang-format on
}

/**
 * @brief Check whether measurement time is within allowed range.
 *
 * @param[in] meas_time Measurement time.
 *
 * @retval true Measurement time is valid.
 * @retval false Measurement time is invalid.
 */
static bool is_valid_meas_time(uint8_t meas_time)
{
    return ((meas_time >= BH1750_MIN_MEAS_TIME) && (meas_time <= BH1750_MAX_MEAS_TIME));
}

/**
 * @brief Convert two bytes in big endian to an integer of type uint16_t.
 *
 * @param[in] bytes The two bytes at this address are used for conversion.
 *
 * @return uint16_t Resulting integer.
 */
static uint16_t two_big_endian_bytes_to_uint16(const uint8_t *const bytes)
{
    return (((uint16_t)(bytes[0])) << 8) | ((uint16_t)(bytes[1]));
}

/**
 * @brief Get three most significant bits of measurement time.
 *
 * @param[in] meas_time Measurement time.
 *
 * @return uint8_t Three most significant bits of @p meas_time. Value between 0 and 7 (both including).
 */
static uint8_t get_three_msb_of_meas_time(uint8_t meas_time)
{
    return meas_time >> 5;
}

/**
 * @brief Get five least significant bits of measurement time.
 *
 * @param[in] meas_time Measurement time.
 *
 * @return uint8_t Five least significant bits of @p meas_time. Value between 0 and 31 (both including).
 */
static uint8_t get_five_lsb_of_meas_time(uint8_t meas_time)
{
    return meas_time & ((uint8_t)0x1F);
}

/**
 * @brief Get "start continuous measurement" command code.
 *
 * @pre @p meas_mode has been validated to have one of the valid values from @ref BH1750MeasMode.
 *
 * @param[in] meas_mode Measurement mode. One of @ref BH1750MeasMode.
 *
 * @return uint8_t Corresponding command code.
 */
static uint8_t get_start_cont_meas_cmd_code(uint8_t meas_mode)
{
    uint8_t cmd_code;
    switch (meas_mode) {
    case BH1750_MEAS_MODE_H_RES:
        cmd_code = BH1750_START_CONTINUOUS_MEAS_H_RES_CMD;
        break;
    case BH1750_MEAS_MODE_H_RES2:
        cmd_code = BH1750_START_CONTINUOUS_MEAS_H_RES2_CMD;
        break;
    case BH1750_MEAS_MODE_L_RES:
        cmd_code = BH1750_START_CONTINUOUS_MEAS_L_RES_CMD;
        break;
    default:
        /* We should never end up here, because prior to calling this function, meas_mode must always be validated */
        cmd_code = BH1750_START_CONTINUOUS_MEAS_H_RES_CMD;
        break;
    }
    return cmd_code;
}

/**
 * @brief Get "one time measurement" command code.
 *
 * @pre @p meas_mode has been validated to have one of the valid values from @ref BH1750MeasMode.
 *
 * @param[in] meas_mode Measurement mode. One of @ref BH1750MeasMode.
 *
 * @return uint8_t Corresponding command code.
 */
static uint8_t get_one_time_meas_cmd_code(uint8_t meas_mode)
{
    uint8_t cmd_code;
    switch (meas_mode) {
    case BH1750_MEAS_MODE_H_RES:
        cmd_code = BH1750_ONE_TIME_MEAS_H_RES_CMD;
        break;
    case BH1750_MEAS_MODE_H_RES2:
        cmd_code = BH1750_ONE_TIME_MEAS_H_RES2_CMD;
        break;
    case BH1750_MEAS_MODE_L_RES:
        cmd_code = BH1750_ONE_TIME_MEAS_L_RES_CMD;
        break;
    default:
        /* We should never end up here, because prior to calling this function, meas_mode must always be validated */
        cmd_code = BH1750_ONE_TIME_MEAS_H_RES_CMD;
        break;
    }
    return cmd_code;
}

/**
 * @brief Start a sequence.
 *
 * Saves @p cb and @p user_data as sequence callback and user data, so that they can be executed once the sequence is
 * complete.
 *
 * @param[in] self BH1750 instance.
 * @param[in] cb Callback to execute once the sequence is complete.
 * @param[in] user_data User data to pass to @p cb.
 */
static void start_sequence(BH1750 self, void *cb, void *user_data)
{
    self->seq_cb = cb;
    self->seq_cb_user_data = user_data;
    self->is_seq_ongoing = true;
}

/**
 * @brief End the ongoing sequence.
 *
 * Must be called whenever a sequence is ended, so that other public functions can be called again. Otherwise, public
 * function will keep returning @ref BH1750_RESULT_CODE_BUSY.
 *
 * @param[in] self BH1750 instance.
 */
static void end_sequence(BH1750 self)
{
    self->is_seq_ongoing = false;
}

/**
 * @brief Interpret self->seq_cb as BH1750CompleteCb and execute it, if present.
 *
 * @param[in] self BH1750 instance.
 * @param[in] rc Result code to pass to the complete cb.
 */
static void execute_complete_cb(BH1750 self, uint8_t rc)
{
    end_sequence(self);
    BH1750CompleteCb cb = (BH1750CompleteCb)self->seq_cb;
    if (cb) {
        cb(rc, self->seq_cb_user_data);
    }
}

/**
 * @brief I2C callback to execute when the last I2C transaction in the sequence is complete.
 *
 * Interprets seq_cb as BH1750CompleteCb and executes it. Passes BH1750_RESULT_CODE_OK to seq_cb if I2C transaction was
 * successful. Passes BH1750_RESULT_CODE_IO_ERR to seq_cb if I2C transaction failed.
 *
 * @param[in] result_code I2C transaction result code. One of @ref BH1750_I2CResultCode.
 * @param[in] user_data User data.
 */
static void generic_i2c_complete_cb(uint8_t result_code, void *user_data)
{
    BH1750 self = (BH1750)user_data;
    if (!self) {
        return;
    }

    uint8_t rc = (result_code == BH1750_I2C_RESULT_CODE_OK) ? BH1750_RESULT_CODE_OK : BH1750_RESULT_CODE_IO_ERR;
    execute_complete_cb(self, rc);
}

/**
 * @brief Send power on command.
 *
 * @param[in] self BH1750 instance.
 * @param[in] cb Callback to execute once the command is sent.
 * @param[in] user_data User data to pass to @p cb.
 */
static void send_power_on_cmd(BH1750 self, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = BH1750_POWER_ON_CMD;
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
}

/**
 * @brief Send power down command.
 *
 * @param[in] self BH1750 instance.
 * @param[in] cb Callback to execute once the command is sent.
 * @param[in] user_data User data to pass to @p cb.
 */
static void send_power_down_cmd(BH1750 self, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = BH1750_POWER_DOWN_CMD;
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
}

/**
 * @brief Send reset command.
 *
 * @param[in] self BH1750 instance.
 * @param[in] cb Callback to execute once the command is sent.
 * @param[in] user_data User data to pass to @p cb.
 */
static void send_reset_cmd(BH1750 self, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = BH1750_RESET_CMD;
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
}

/**
 * @brief Send a I2C read command to read light intensity measurement.
 *
 * The resulting 2 bytes of raw measurement are read into self->read_buf.
 *
 * @param[in] self BH1750 instance.
 * @param[in] cb Callback to execute once the read command is sent.
 * @param[in] user_data User data to pass to @p cb.
 */
static void send_read_meas_cmd(BH1750 self, BH1750_I2CCompleteCb cb, void *user_data)
{
    self->i2c_read(self->read_buf, 2, self->i2c_addr, self->i2c_read_user_data, cb, user_data);
}

/**
 * @brief Send start continuous measurement command.
 *
 * @param[in] self BH1750 instance.
 * @param[in] meas_mode Measurement mode. One of @ref BH1750MeasMode.
 * @param[in] cb Callback to execute once the command is sent.
 * @param[in] user_data User data to pass to @p cb.
 */
static void send_start_continuous_meas_cmd(BH1750 self, uint8_t meas_mode, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = get_start_cont_meas_cmd_code(meas_mode);
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
}

/**
 * @brief Send one time measurement command.
 *
 * @param[in] self BH1750 instance.
 * @param[in] meas_mode Measurement mode. One of @ref BH1750MeasMode.
 * @param[in] cb Callback to execute once the command is sent.
 * @param[in] user_data User data to pass to @p cb.
 */
static void send_one_time_meas_cmd(BH1750 self, uint8_t meas_mode, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = get_one_time_meas_cmd_code(meas_mode);
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
}

/**
 * @brief Set the three MSbs of MTreg register.
 *
 * @param[in] self BH1750 instance.
 * @param[in] val Value to set the three most significant bits of MTreg to. Must be a value between 0 and 7 to fit into
 * three bits.
 * @param[in] cb Callback to execute once the command is sent.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully sent the command to set high bits of MTreg.
 * @retval BH1750_RESULT_CODE_INVALID_ARG @p val is > 7, so it cannot fit into three bits. @p cb will not be executed,
 * because the command was not sent.
 */
static uint8_t set_mtreg_high_bit(BH1750 self, uint8_t val, BH1750_I2CCompleteCb cb, void *user_data)
{
    if (val > 7) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    uint8_t cmd = ((uint8_t)BH1750_SET_MTREG_HIGH_BIT_CMD) | val;
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
    return BH1750_RESULT_CODE_OK;
}

/**
 * @brief Set the five MSbs of MTreg register.
 *
 * @param[in] self BH1750 instance.
 * @param[in] val Value to set the five least significant bits of MTreg to. Must be a value between 0 and 31 to fit into
 * five bits.
 * @param[in] cb Callback to execute once the command is sent.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully sent the command to set low bits of MTreg.
 * @retval BH1750_RESULT_CODE_INVALID_ARG @p val is > 31, so it cannot fit into five bits. @p cb will not be executed,
 * because the command was not sent.
 */
static uint8_t set_mtreg_low_bit(BH1750 self, uint8_t val, BH1750_I2CCompleteCb cb, void *user_data)
{
    if (val > 31) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    uint8_t cmd = ((uint8_t)BH1750_SET_MTREG_LOW_BIT_CMD) | val;
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
    return BH1750_I2C_RESULT_CODE_OK;
}

/**
 * @brief Convert raw measurement to illuminance in lx.
 *
 * @param[in] self BH1750 instance.
 * @param[in] raw_meas Raw measurement
 * @param[out] meas_lx Resulting measurement in lx is written here.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully converted raw measurement to illuminance in lx.
 * @retval BH1750_RESULT_CODE_INVALID_USAGE self->meas_time is 0. Cannot convert, because we need to divide by
 * self->meas_time. self->meas_time should never be 0.
 * @retval BH1750_RESULT_CODE_DRIVER_ERR Something went wrong with the code of this driver.
 */
static uint8_t convert_raw_meas_to_lx(BH1750 self, uint16_t raw_meas, uint32_t *const meas_lx)
{
    if (self->meas_time == 0) {
        /* Division by 0 safety check */
        return BH1750_RESULT_CODE_INVALID_USAGE;
    }
    switch (self->meas_mode) {
    case BH1750_MEAS_MODE_H_RES:
        *meas_lx = lroundf(raw_meas * (BH1750_CONVERSION_MAGIC * (69.0f / (self->meas_time))));
        break;
    case BH1750_MEAS_MODE_H_RES2:
        *meas_lx = lroundf(raw_meas * ((BH1750_CONVERSION_MAGIC * (69.0f / (self->meas_time))) / 2.0f));
        break;
    case BH1750_MEAS_MODE_L_RES:
        *meas_lx = lroundf(raw_meas * BH1750_CONVERSION_MAGIC);
        break;
    default:
        /* Invalid measurement mode */
        return BH1750_RESULT_CODE_DRIVER_ERR;
    }

    return BH1750_RESULT_CODE_OK;
}

static void set_meas_time_part_3(uint8_t result_code, void *user_data)
{
    BH1750 self = (BH1750)user_data;
    if (!self) {
        return;
    }

    if (result_code != BH1750_I2C_RESULT_CODE_OK) {
        execute_complete_cb(self, BH1750_RESULT_CODE_IO_ERR);
        return;
    }

    self->meas_time = self->meas_time_to_set;
    /* This function is the last part of two sequences: init sequence and set measurement time sequence. At the end of
     * successful init sequence, we need to set the initialized flag to true. In theory, we do not need to do it at the
     * end of the set measurement time sequence.
     * However, if we are performing the set measurement time sequence, that means the instance is initialized. This
     * means there is no harm in setting the initialized flag again.
     * The alternative is to introduce state to check whether we are performing init sequence or set measurement time
     * sequence, and only set this flag in case of init sequence. We do not do this, since this adds complexity, and we
     * can get away with setting the flag in both sequences. */
    self->initialized = true;
    execute_complete_cb(self, BH1750_RESULT_CODE_OK);
}

static void set_meas_time_part_2(uint8_t result_code, void *user_data)
{
    BH1750 self = (BH1750)user_data;
    if (!self) {
        return;
    }

    if (result_code != BH1750_I2C_RESULT_CODE_OK) {
        execute_complete_cb(self, BH1750_RESULT_CODE_IO_ERR);
        return;
    }

    /* The first three bits of Mtreg have been set. Update the first three bits in our local ram copy of Mtreg. Even if
     * the second write fails, then the local ram copy will still be valid - given that the second write did not modify
     * the register content. */
    self->meas_time =
        ((self->meas_time & ((uint8_t)0x1FU))) | (get_three_msb_of_meas_time(self->meas_time_to_set) << 5);

    uint8_t meas_time_five_lsb = get_five_lsb_of_meas_time(self->meas_time_to_set);
    uint8_t rc = set_mtreg_low_bit(self, meas_time_five_lsb, set_meas_time_part_3, (void *)self);
    if (rc != BH1750_RESULT_CODE_OK) {
        /* get_five_lsb_of_meas_time returned a value > 31. This should never happen. */
        execute_complete_cb(self, BH1750_RESULT_CODE_DRIVER_ERR);
    }
}

/**
 * @brief Initiate first operation of set_meas_time sequence.
 *
 * @pre @p self is validated to be not NULL.
 * @pre @p meas_time is validated to be a valid measurement time.
 *
 * @param[in] self BH1750 instance.
 * @param[in] meas_time Measurement time to set.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully initiated the first operation of set_meas_time sequence.
 * @retval BH1750_RESULT_CODE_DRIVER_ERR Something went wrong in the code of this driver.
 */
static void set_meas_time_part_1(BH1750 self, uint8_t meas_time)
{
    self->meas_time_to_set = meas_time;

    uint8_t meas_time_three_msb = get_three_msb_of_meas_time(meas_time);
    /* Ignore return value. Since meas_time has been validated, it should always return OK. It is difficult to handle
     * the case if the return code is not OK, because this function can be called from the first part of the
     * set_meas_time sequence, and also from a later part of init sequence. We would need to return an error code, and
     * handle these different scenarios in the callers of this function. This adds complexity and is not necessary,
     * since meas_time has been validated. */
    set_mtreg_high_bit(self, meas_time_three_msb, set_meas_time_part_2, (void *)self);
}

static void init_part_2(uint8_t result_code, void *user_data)
{
    BH1750 self = (BH1750)user_data;
    if (!self) {
        return;
    }

    if (result_code != BH1750_I2C_RESULT_CODE_OK) {
        execute_complete_cb(self, BH1750_RESULT_CODE_IO_ERR);
        return;
    }

    set_meas_time_part_1(self, self->meas_time_to_set);
}

static void read_meas_final_part(uint8_t result_code, void *user_data)
{
    BH1750 self = (BH1750)user_data;
    if (!self) {
        return;
    }

    if (result_code != BH1750_I2C_RESULT_CODE_OK) {
        execute_complete_cb(self, BH1750_RESULT_CODE_IO_ERR);
        return;
    }

    uint16_t raw_meas = two_big_endian_bytes_to_uint16(self->read_buf);
    uint8_t rc = convert_raw_meas_to_lx(self, raw_meas, self->meas_p);
    if (rc != BH1750_RESULT_CODE_OK) {
        /* self->meas_time is 0, this should never happen */
        execute_complete_cb(self, BH1750_RESULT_CODE_DRIVER_ERR);
        return;
    }

    execute_complete_cb(self, BH1750_RESULT_CODE_OK);
}

static void start_continuous_measurement_part_2(uint8_t result_code, void *user_data)
{
    BH1750 self = (BH1750)user_data;
    if (!self) {
        return;
    }

    uint8_t rc;
    if (result_code == BH1750_I2C_RESULT_CODE_OK) {
        rc = BH1750_RESULT_CODE_OK;
        self->cont_meas_ongoing = true;
    } else {
        rc = BH1750_RESULT_CODE_IO_ERR;
    }
    execute_complete_cb(self, rc);
}

static void read_one_time_meas_part_3(void *user_data)
{
    BH1750 self = (BH1750)user_data;
    if (!self) {
        return;
    }

    send_read_meas_cmd(self, read_meas_final_part, (void *)self);
}

static void read_one_time_meas_part_2(uint8_t result_code, void *user_data)
{
    BH1750 self = (BH1750)user_data;
    if (!self) {
        return;
    }

    if (result_code != BH1750_I2C_RESULT_CODE_OK) {
        execute_complete_cb(self, BH1750_RESULT_CODE_IO_ERR);
        return;
    }

    uint32_t timer_period;
    if (self->meas_mode == BH1750_MEAS_MODE_L_RES) {
        timer_period = BH1750_MAX_L_RES_MEAS_TIME_MS;
    } else {
        /* In high res mdoes, the time we need to wait depends on the meas time currently set in Mtreg. We keep a RAM
         * copy of that value in self->meas_time. The higher self->meas_time, the longer it will take to make a
         * measurement. For example:
         * Meas time in Mtreg is 138. Default meas time is 69. 138/69 = 2. This means that we should wait twice as long
         * compared to if meas time were 69.
         * It takes 180 ms to make a measurement in high res mode when meas time in Mtreg is 69. This means that we
         * should wait for 180 * 2 = 360 ms - that's how long it will take to make a measurement when meas time in Mtreg
         * is 138. */
        float timer_period_multiplier = ((float)self->meas_time) / BH1750_DEFAULT_MEAS_TIME;
        /* Ceil timer period instead of rounding to be sure that measurement is ready after timer expires */
        timer_period = ceilf(BH1750_MAX_H_RES_MEAS_TIME_MS * timer_period_multiplier);
    }
    self->start_timer(timer_period, self->start_timer_user_data, read_one_time_meas_part_3, (void *)self);
}

uint8_t bh1750_create(BH1750 *const inst, const BH1750InitConfig *const cfg)
{
    if (!inst || !cfg || !is_valid_init_cfg(cfg)) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }

    *inst = cfg->get_instance_memory(cfg->get_instance_memory_user_data);
    if (!(*inst)) {
        return BH1750_RESULT_CODE_OUT_OF_MEMORY;
    }

    (*inst)->i2c_write = cfg->i2c_write;
    (*inst)->i2c_write_user_data = cfg->i2c_write_user_data;
    (*inst)->i2c_read = cfg->i2c_read;
    (*inst)->i2c_read_user_data = cfg->i2c_read_user_data;
    (*inst)->start_timer = cfg->start_timer;
    (*inst)->start_timer_user_data = cfg->start_timer_user_data;
    (*inst)->i2c_addr = cfg->i2c_addr;
    (*inst)->cont_meas_ongoing = false;
    /* Will be populated during init where we set the default measurement time (69). Initialized here as a safety
     * measure so that we do not access an uninitialized variable. */
    (*inst)->meas_time = 0;
    (*inst)->initialized = false;
    (*inst)->is_seq_ongoing = false;

    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_init(BH1750 self, BH1750CompleteCb cb, void *user_data)
{
    if (!self) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    if (self->initialized) {
        /* Already initialized, this function should only be called once per instance. */
        return BH1750_RESULT_CODE_INVALID_USAGE;
    }

    start_sequence(self, (void *)cb, user_data);
    self->meas_time_to_set = BH1750_DEFAULT_MEAS_TIME;
    send_power_on_cmd(self, init_part_2, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_power_on(BH1750 self, BH1750CompleteCb cb, void *user_data)
{
    if (!self) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    if (!self->initialized) {
        return BH1750_RESULT_CODE_INVALID_USAGE;
    }
    if (self->is_seq_ongoing) {
        return BH1750_RESULT_CODE_BUSY;
    }

    start_sequence(self, (void *)cb, user_data);
    send_power_on_cmd(self, generic_i2c_complete_cb, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_power_down(BH1750 self, BH1750CompleteCb cb, void *user_data)
{
    if (!self) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    if (!self->initialized) {
        return BH1750_RESULT_CODE_INVALID_USAGE;
    }
    if (self->is_seq_ongoing) {
        return BH1750_RESULT_CODE_BUSY;
    }

    start_sequence(self, (void *)cb, user_data);
    send_power_down_cmd(self, generic_i2c_complete_cb, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_reset(BH1750 self, BH1750CompleteCb cb, void *user_data)
{
    if (!self) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    if (!self->initialized) {
        return BH1750_RESULT_CODE_INVALID_USAGE;
    }
    if (self->is_seq_ongoing) {
        return BH1750_RESULT_CODE_BUSY;
    }

    start_sequence(self, (void *)cb, user_data);
    send_reset_cmd(self, generic_i2c_complete_cb, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_start_continuous_measurement(BH1750 self, uint8_t meas_mode, BH1750CompleteCb cb, void *user_data)
{
    if (!self || !is_valid_meas_mode(meas_mode)) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    if (!self->initialized) {
        return BH1750_RESULT_CODE_INVALID_USAGE;
    }
    if (self->is_seq_ongoing) {
        return BH1750_RESULT_CODE_BUSY;
    }

    start_sequence(self, (void *)cb, user_data);
    self->meas_mode = meas_mode;
    send_start_continuous_meas_cmd(self, meas_mode, start_continuous_measurement_part_2, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_read_continuous_measurement(BH1750 self, uint32_t *const meas_lx, BH1750CompleteCb cb, void *user_data)
{
    if (!self || !meas_lx) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    /* Technically, initialized check is not necessary, because cont_meas_ongoing can become true only when the instance
     * is initialized. */
    if (!self->initialized || !self->cont_meas_ongoing) {
        return BH1750_RESULT_CODE_INVALID_USAGE;
    }
    if (self->is_seq_ongoing) {
        return BH1750_RESULT_CODE_BUSY;
    }

    start_sequence(self, (void *)cb, user_data);
    self->meas_p = meas_lx;
    send_read_meas_cmd(self, read_meas_final_part, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_read_one_time_measurement(BH1750 self, uint8_t meas_mode, uint32_t *const meas_lx, BH1750CompleteCb cb,
                                         void *user_data)
{
    if (!self || !meas_lx || !is_valid_meas_mode(meas_mode)) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    if (!self->initialized) {
        return BH1750_RESULT_CODE_INVALID_USAGE;
    }
    if (self->is_seq_ongoing) {
        return BH1750_RESULT_CODE_BUSY;
    }

    start_sequence(self, (void *)cb, user_data);
    /* So that the last part of the sequence can write the result to meas_lx */
    self->meas_p = meas_lx;
    /* So that the last part of the sequence can convert raw measurement to lx (mapping depends on meas mode) */
    self->meas_mode = meas_mode;
    send_one_time_meas_cmd(self, meas_mode, read_one_time_meas_part_2, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_set_measurement_time(BH1750 self, uint8_t meas_time, BH1750CompleteCb cb, void *user_data)
{
    if (!self || !is_valid_meas_time(meas_time)) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    if (!self->initialized || self->cont_meas_ongoing) {
        return BH1750_RESULT_CODE_INVALID_USAGE;
    }
    if (self->is_seq_ongoing) {
        return BH1750_RESULT_CODE_BUSY;
    }

    start_sequence(self, (void *)cb, user_data);
    set_meas_time_part_1(self, meas_time);
    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_destroy(BH1750 self, BH1750FreeInstanceMemory free_instance_memory, void *user_data)
{
    if (!self) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }
    if (self->is_seq_ongoing) {
        return BH1750_RESULT_CODE_BUSY;
    }

    if (free_instance_memory) {
        free_instance_memory((void *)self, user_data);
    }
    return BH1750_RESULT_CODE_OK;
}

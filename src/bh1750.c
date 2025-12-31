#include <stdint.h>
#include <stdbool.h>

#include "bh1750.h"
#include "bh1750_private.h"

#define BH1750_POWER_DOWN_CMD 0x0
#define BH1750_POWER_ON_CMD 0x01
#define BH1750_RESET_CMD 0x07
#define BH1750_START_CONTINUOUS_MEAS_H_RES_CMD 0x10
#define BH1750_START_CONTINUOUS_MEAS_H_RES2_CMD 0x11
#define BH1750_START_CONTINUOUS_MEAS_L_RES_CMD 0x13
/* 01000000 in binary. The 5 MSbs are a fixed command to set 3 MSBs of MTreg. */
#define BH1750_SET_MTREG_HIGH_BIT_CMD 0x40U
/* 01100000 in binary. The 3 MSbs are a fixed command to set 5 LSBs of MTreg. */
#define BH1750_SET_MTREG_LOW_BIT_CMD 0x60U

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
    return (cfg->get_instance_memory && cfg->i2c_write);
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
 * @brief Get three most significant bits of measurement time.
 *
 * @param meas_time Measurement time.
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
 * @param meas_time Measurement time.
 *
 * @return uint8_t Five least significant bits of @p meas_time. Value between 0 and 31 (both including).
 */
static uint8_t get_five_lsb_of_meas_time(uint8_t meas_time)
{
    return meas_time & ((uint8_t)0x1F);
}

/**
 * @brief Interpret self->seq_cb as BH1750CompleteCb and execute it, if present.
 *
 * @param self BH1750 instance.
 * @param rc Result code to pass to the complete cb.
 */
static void execute_complete_cb(BH1750 self, uint8_t rc)
{
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
 * @param result_code I2C transaction result code. One of @ref BH1750_I2CResultCode.
 * @param user_data User data.
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
 * @brief Start a sequence.
 *
 * @param self BH1750 instance.
 * @param cb Callback to execute once the sequence is complete.
 * @param user_data User data to pass to @p cb.
 */
static void start_sequence(BH1750 self, void *cb, void *user_data)
{
    self->seq_cb = cb;
    self->seq_cb_user_data = user_data;
}

/**
 * @brief Send power on command.
 *
 * @param self BH1750 instance.
 * @param cb Callback to execute once the command is sent.
 * @param user_data User data to pass to @p cb.
 */
static void send_power_on_cmd(BH1750 self, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = BH1750_POWER_ON_CMD;
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
}

/**
 * @brief Send power down command.
 *
 * @param self BH1750 instance.
 * @param cb Callback to execute once the command is sent.
 * @param user_data User data to pass to @p cb.
 */
static void send_power_down_cmd(BH1750 self, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = BH1750_POWER_DOWN_CMD;
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
}

/**
 * @brief Send reset command.
 *
 * @param self BH1750 instance.
 * @param cb Callback to execute once the command is sent.
 * @param user_data User data to pass to @p cb.
 */
static void send_reset_cmd(BH1750 self, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = BH1750_RESET_CMD;
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
}

/**
 * @brief Get "start continuous measurement" command code.
 *
 * @param meas_mode Measurement mode.
 *
 * @return uint8_t Corresponding command code.
 */
uint8_t get_start_cont_meas_cmd_code(uint8_t meas_mode)
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
 * @brief Send start continuous measurement command.
 *
 * @param self BH1750 instance.
 * @param meas_mode Measurement mode. One of @ref BH1750MeasMode.
 * @param cb Callback to execute once the command is sent.
 * @param user_data User data to pass to @p cb.
 */
static void send_start_continuous_meas_cmd(BH1750 self, uint8_t meas_mode, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = get_start_cont_meas_cmd_code(meas_mode);
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
}

/**
 * @brief Set the three MSbs of MTreg register.
 *
 * @param self BH1750 instance.
 * @param val Value to set the three most significant bits of MTreg to. Must be a value between 0 and 7 to fit into
 * three bits.
 * @param cb Callback to execute once the command is sent.
 * @param user_data User data to pass to @p cb.
 */
static void set_mtreg_high_bit(BH1750 self, uint8_t val, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = ((uint8_t)BH1750_SET_MTREG_HIGH_BIT_CMD) | val;
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
}

/**
 * @brief Set the five MSbs of MTreg register.
 *
 * @param self BH1750 instance.
 * @param val Value to set the five least significant bits of MTreg to. Must be a value between 0 and 31 to fit into
 * five bits.
 * @param cb Callback to execute once the command is sent.
 * @param user_data User data to pass to @p cb.
 */
static void set_mtreg_low_bit(BH1750 self, uint8_t val, BH1750_I2CCompleteCb cb, void *user_data)
{
    uint8_t cmd = ((uint8_t)BH1750_SET_MTREG_LOW_BIT_CMD) | val;
    self->i2c_write(&cmd, 1, self->i2c_addr, self->i2c_write_user_data, cb, user_data);
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

    uint8_t meas_time_five_lsb = get_five_lsb_of_meas_time(self->meas_time);
    set_mtreg_low_bit(self, meas_time_five_lsb, generic_i2c_complete_cb, (void *)self);
}

uint8_t bh1750_create(BH1750 *const inst, const BH1750InitConfig *const cfg)
{
    if (!inst || !is_valid_init_cfg(cfg)) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }

    *inst = cfg->get_instance_memory(cfg->get_instance_memory_user_data);
    if (!(*inst)) {
        return BH1750_RESULT_CODE_OUT_OF_MEMORY;
    }

    (*inst)->i2c_write = cfg->i2c_write;
    (*inst)->i2c_write_user_data = cfg->i2c_write_user_data;
    (*inst)->i2c_addr = cfg->i2c_addr;

    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_power_on(BH1750 self, BH1750CompleteCb cb, void *user_data)
{
    if (!self) {
        return BH1750_RESULT_CODE_INVALID_ARG;
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

    start_sequence(self, (void *)cb, user_data);
    send_power_down_cmd(self, generic_i2c_complete_cb, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_reset(BH1750 self, BH1750CompleteCb cb, void *user_data)
{
    if (!self) {
        return BH1750_RESULT_CODE_INVALID_ARG;
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

    start_sequence(self, (void *)cb, user_data);
    send_start_continuous_meas_cmd(self, meas_mode, generic_i2c_complete_cb, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_set_measurement_time(BH1750 self, uint8_t meas_time, BH1750CompleteCb cb, void *user_data)
{
    if (!self) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }

    start_sequence(self, (void *)cb, user_data);
    self->meas_time = meas_time;

    uint8_t meas_time_three_msb = get_three_msb_of_meas_time(meas_time);
    set_mtreg_high_bit(self, meas_time_three_msb, set_meas_time_part_2, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

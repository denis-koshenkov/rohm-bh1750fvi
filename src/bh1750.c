#include <stdint.h>
#include <stdbool.h>

#include "bh1750.h"
#include "bh1750_private.h"

#define BH1750_POWER_ON_CMD 0x01

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

    BH1750CompleteCb cb = (BH1750CompleteCb)self->seq_cb;
    cb(BH1750_RESULT_CODE_IO_ERR, self->seq_cb_user_data);
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
    self->i2c_write(&cmd, 1, 0x23, self->i2c_write_user_data, cb, user_data);
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

    return BH1750_RESULT_CODE_OK;
}

uint8_t bh1750_power_on(BH1750 self, BH1750CompleteCb cb, void *user_data)
{
    start_sequence(self, (void *)cb, user_data);
    send_power_on_cmd(self, generic_i2c_complete_cb, (void *)self);
    return BH1750_RESULT_CODE_OK;
}

#ifndef SRC_BH1750_DEFS_H
#define SRC_BH1750_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @brief BH1750 definitions.
 *
 * These definitions are placed in a separate header, so that they can be included both by bh1750_private.h and
 * bh1750.h. They need to be a part of the public interface, but they also need to be included in bh1750_private.h,
 * because they contain data types that are present in the struct BH1750Struct definition.
 */

/** Result codes describing outcomes of a I2C transaction. */
typedef enum {
    /** Successful I2C transaction. */
    BH1750_I2C_RESULT_CODE_OK = 0,
    /** I2C transaction failed. */
    BH1750_I2C_RESULT_CODE_ERR = 0,
} BH1750_I2CResultCode;

/**
 * @brief Callback type to execute when a I2C transaction to BH1750 is complete.
 *
 * @param[in] result_code Pass one of the values from @ref BH1750_I2CResultCode to describe the transaction result.
 * @param[in] user_data The caller must pass user_data parameter that the BH1750 driver passed to @ref BH1750_I2CRead or
 * @ref BH1750_I2CWrite.
 */
typedef void (*BH1750_I2CCompleteCb)(uint8_t result_code, void *user_data);

/**
 * @brief Perform a I2C write transaction to the BH1750 device.
 *
 * @param[in] data Data to write to the device.
 * @param[in] length Number of bytes in the @p data array.
 * @param[in] i2c_addr I2C address of the BH1750 device.
 * @param[in] user_data This parameter will be equal to i2c_write_user_data from the init config passed to @ref
 * sht3x_create.
 * @param[in] cb Callback to execute once the I2C transaction is complete. This callback must be executed from the
 * same context that the BH1750 driver API functions get called from.
 * @param[in] cb_user_data User data to pass to @p cb.
 */
typedef void (*BH1750_I2CWrite)(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data,
                                BH1750_I2CCompleteCb cb, void *cb_user_data);

#ifdef __cplusplus
}
#endif

#endif /* SRC_BH1750_DEFS_H */

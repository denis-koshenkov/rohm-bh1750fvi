#ifndef SRC_BH1750_H
#define SRC_BH1750_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "bh1750_defs.h"

typedef struct BH1750Struct *BH1750;

/**
 * @brief Gets called in @ref bh1750_create to get memory for a BH1750 instance.
 *
 * The implementation of this function should return a pointer to memory of size sizeof(struct BH1750Struct). All
 * private data for the created BH1750 instance will reside in that memory.
 *
 * The implementation of this function should be defined in a separate source file. That source file should include
 * bh1750_private.h, which contains the definition of struct BH1750Struct. The implementation of this function then
 * knows at compile time the size of memory that it needs to provide.
 *
 * This function will be called as many times as @ref bh1750_create is called (given that all parameters passed to @ref
 * bh1750_create are valid). The implementation should be capable of returning memory for that many distinct instances.
 *
 * Implementation example - two statically allocated instances:
 * ```
 * void *get_instance_memory(void *user_data) {
 *     static struct BH1750Struct instances[2];
 *     static size_t idx = 0;
 *     return (idx < 2) ? (&(instances[idx++])) : NULL;
 * }
 * ```
 *
 * If the application uses dynamic memory allocation, another implementation option is to allocate sizeof(struct
 * BH1750Struct) bytes dynamically.
 *
 * @param user_data When this function is called, this parameter will be equal to the get_instance_memory_user_data
 * field in the BH1750InitConfig passed to @ref bh1750_create.
 *
 * @return void * Pointer to instance memory of size sizeof(struct BH1750Struct). If failed to get memory, should return
 * NULL. In that case, @ref bh1750_create will return @ref BH1750_RESULT_CODE_OUT_OF_MEMORY.
 */
typedef void *(*BH1750GetInstanceMemory)(void *user_data);

/**
 * @brief Callback type to execute when the BH1750 driver finishes an operation.
 *
 * @param result_code Indicates success or the reason for failure. One of @ref BH1750ResultCode.
 * @param user_data User data.
 */
typedef void (*BH1750CompleteCb)(uint8_t result_code, void *user_data);

typedef enum {
    BH1750_RESULT_CODE_OK = 0,
    BH1750_RESULT_CODE_INVALID_ARG,
    BH1750_RESULT_CODE_OUT_OF_MEMORY,
    BH1750_RESULT_CODE_IO_ERR,
    BH1750_RESULT_CODE_DRIVER_ERR,
    BH1750_RESULT_CODE_INVALID_USAGE,
} BH1750ResultCode;

typedef enum {
    BH1750_MEAS_MODE_H_RES,
    BH1750_MEAS_MODE_H_RES2,
    BH1750_MEAS_MODE_L_RES,
} BH1750MeasMode;

typedef struct {
    BH1750GetInstanceMemory get_instance_memory;
    void *get_instance_memory_user_data;
    BH1750_I2CWrite i2c_write;
    void *i2c_write_user_data;
    BH1750_I2CRead i2c_read;
    void *i2c_read_user_data;
    BH1750StartTimer start_timer;
    void *start_timer_user_data;
    uint8_t i2c_addr;
} BH1750InitConfig;

/**
 * @brief Create a BH1750 instance.
 *
 * @param[out] inst The created instance is written to this parameter in case of success.
 * @param[in] cfg Initialization config.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully created instance.
 * @retval BH1750_RESULT_CODE_INVALID_ARG Failed to create instance. @p inst is NULL, or @p cfg is not a valid init
 config.
 * @retval BH1750_RESULT_CODE_OUT_OF_MEMORY Failed to create instance. Used-defined function cfg->get_instance_memory
 returned NULL.
 */
uint8_t bh1750_create(BH1750 *const inst, const BH1750InitConfig *const cfg);

/**
 * @brief Initialize a BH1750 instance.
 *
 * Performs the following steps:
 * 1. Powers on BH1750 (equivalent to calling @ref bh1750_power_on).
 * 2. Sets measurement time to 69 (default). Equivalent to calling @ref bh1750_set_measurement_time with "meas_time"
 * parameter equal to 69.
 *
 * Once the init sequence is complete, or an error occurs, @p cb is executed. "result_code" parameter of @p cb indicates
 * success or reason for failure of the init sequence:
 * - @ref SHT3X_RESULT_CODE_OK Successfully performed the init sequence.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C One of the I2C transactions in the init sequence failed.
 * - @ref BH1750_RESULT_CODE_DRIVER_ERR Something went wrong with the code of this driver.
 *
 * @param[in] self BH1750 instance created by @ref bh1750_create.
 * @param[in] cb Callback to execute once init is complete.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully initiated the first step of init sequence.
 * @retval BH1750_RESULT_CODE_INVALID_ARG @p self is NULL.
 */
uint8_t bh1750_init(BH1750 self, BH1750CompleteCb cb, void *user_data);

/**
 * @brief Power on BH1750 device.
 *
 * Sends the "power on" command to BH1750.
 *
 * Once the power on sequence is complete, or an error occurs, @p cb is executed. "result_code" parameter of @p cb
 * indicates success or reason for failure of the power on sequence:
 * - @ref SHT3X_RESULT_CODE_OK Successfully performed the power on sequence.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction to send the power on command failed.
 *
 * @param[in] self BH1750 instance created by @ref bh1750_create.
 * @param[in] cb Callback to execute once power on is complete.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully initiated power on.
 * @retval BH1750_RESULT_CODE_INVALID_ARG @p self is NULL.
 */
uint8_t bh1750_power_on(BH1750 self, BH1750CompleteCb cb, void *user_data);

/**
 * @brief Power down BH1750 device.
 *
 * Sends the "power down" command to BH1750.
 *
 * Once the power down sequence is complete, or an error occurs, @p cb is executed. "result_code" parameter of @p cb
 * indicates success or reason for failure of the power down sequence:
 * - @ref SHT3X_RESULT_CODE_OK Successfully performed the power down sequence.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction to send the power down command failed.
 *
 * @param[in] self BH1750 instance created by @ref bh1750_create.
 * @param[in] cb Callback to execute once power down is complete.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully initiated power down.
 * @retval BH1750_RESULT_CODE_INVALID_ARG @p self is NULL.
 */
uint8_t bh1750_power_down(BH1750 self, BH1750CompleteCb cb, void *user_data);

/**
 * @brief Reset data register value.
 *
 * This function removes the previous measurement result. It resets the internal illuminance data register in the BH1750
 * device.
 *
 * Sends the "reset" command to BH1750.
 *
 * Once the reset sequence is complete, or an error occurs, @p cb is executed. "result_code" parameter of @p cb
 * indicates success or reason for failure of the reset sequence:
 * - @ref SHT3X_RESULT_CODE_OK Successfully performed the reset sequence.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction to send the reset command failed.
 *
 * @param[in] self BH1750 instance created by @ref bh1750_create.
 * @param[in] cb Callback to execute once reset is complete.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully initiated reset.
 * @retval BH1750_RESULT_CODE_INVALID_ARG @p self is NULL.
 *
 * @note This command does not work in power down mode. Make sure the device is in power on mode before calling this
 * function.
 */
uint8_t bh1750_reset(BH1750 self, BH1750CompleteCb cb, void *user_data);

/**
 * @brief Start continuous measurement of light intensity.
 *
 * Sends a command to BH1750 to start continuous measurement. @p meas_mode determines the mode of continuous
 * measurement.
 *
 * Once the start continuous measurement sequence is complete, or an error occurs, @p cb is executed. "result_code"
 * parameter of @p cb indicates success or reason for failure of the start continuous measurement sequence:
 * - @ref SHT3X_RESULT_CODE_OK Successfully performed the start continuous measurement sequence.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction to send the start continuous measurement command failed.
 *
 * @param[in] self BH1750 instance created by @ref bh1750_create.
 * @param[in] meas_mode Continuous measurement mode. Use @ref BH1750MeasMode.
 * @param[in] cb Callback to execute once start continuous measurement sequence is complete.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully initiated start continuous measurement.
 * @retval BH1750_RESULT_CODE_INVALID_ARG @p self is NULL, or @p meas_mode is not a valid measurement mode.
 */
uint8_t bh1750_start_continuous_measurement(BH1750 self, uint8_t meas_mode, BH1750CompleteCb cb, void *user_data);

/**
 * @brief Read illuminance in lx when continuous measurement is ongoing.
 *
 * Reads current light intensity measurement when continuous measurement is ongoing. This function should only be called
 * if continuous measurement is currently ongoing. Continuous measurement can be started by calling @ref
 * bh1750_start_continuous_measurement.
 *
 * If this function is called two times, and the measurement did not get updated in between the two calls, @p meas_lx
 * will have the same value for both calls. The rate at which the measurement gets updated depends on the measurement
 * mode passed to @ref bh1750_start_continuous_measurement and the measurement time set via @ref
 * bh1750_set_measurement_time.
 *
 * Once the read continuous measurement sequence is complete, or an error occurs, @p cb is executed. "result_code"
 * parameter of @p cb indicates success or reason for failure of the read continuous measurement sequence:
 * - @ref SHT3X_RESULT_CODE_OK Successfully performed the read continuous measurement sequence.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction to read the measurement failed.
 * - @ref SHT3X_RESULT_CODE_DRIVER_ERR Something went wrong in the code of this driver.
 *
 * @param[in] self BH1750 instance created by @ref bh1750_create.
 * @param[out] meas_lx Resulting illuminance measurement in lx.
 * @param[in] cb Callback to execute once the measurement is read out. @p meas_lx has a valid value when this callback
 * is being executed, not before that.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully initiated reading continuous measurement.
 * @retval BH1750_RESULT_CODE_INVALID_ARG @p self or @p meas_lx is NULL.
 * @retval BH1750_RESULT_CODE_INVALID_USAGE Cannot read measurement, because continuous measurement is not ongoing.
 */
uint8_t bh1750_read_continuous_measurement(BH1750 self, uint32_t *const meas_lx, BH1750CompleteCb cb, void *user_data);

/**
 * @brief Read one-time illuminance measurement in lx.
 *
 * Steps:
 * 1. Send "one-time measurement" command for the @p meas_mode provided by the caller.
 * 2. Wait until the measurement is ready using a timer. The time it takes to take a measurement depends on @p meas_mode
 * and currently set measurement time (makes a difference only in high res modes).
 * 3. Read measurement result from the device and convert it to illuminance measurement in lx.
 *
 * Once the sequence described above is complete, or an error occurs, @p cb is executed. "result_code" parameter of @p
 * cb indicates success or reason for failure:
 * - @ref SHT3X_RESULT_CODE_OK Successfully performed the read one time measurement sequence.
 * - @ref SHT3X_RESULT_CODE_IO_ERR One of the I2C transactions in the sequence failed.
 * - @ref SHT3X_RESULT_CODE_DRIVER_ERR Something went wrong in the code of this driver.
 *
 * @param[in] self BH1750 instance created by @ref bh1750_create.
 * @param[in] meas_mode Measurement mode to use. Use one of @ref BH1750MeasMode.
 * @param[out] meas_lx Resulting illuminance measurement in lx.
 * @param[in] cb Callback to execute once the measurement is read out. @p meas_lx has a valid value when this callback
 * is being executed, not before that.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully initiated reading a one-time measurement.
 * @retval BH1750_RESULT_CODE_INVALID_ARG @p self is NULL, @p meas_lx is NULL, or @p meas_mode is not a valid
 * measurement mode.
 */
uint8_t bh1750_read_one_time_measurement(BH1750 self, uint8_t meas_mode, uint32_t *const meas_lx, BH1750CompleteCb cb,
                                         void *user_data);

/**
 * @brief Set measurement time.
 *
 * This function sends two commands:
 * 1. Set three high bits of measurement time register.
 * 2. Set five low bits of measurement time register.
 *
 * The three high bits and five low bits are taken from the @p meas_time value.
 *
 * Once the set measurement time sequence is complete, or an error occurs, @p cb is executed. "result_code"
 * parameter of @p cb indicates success or reason for failure of the set measurement time sequence:
 * - @ref SHT3X_RESULT_CODE_OK Successfully performed the set measurement time sequence.
 * - @ref SHT3X_RESULT_CODE_IO_ERR One of the I2C transactions failed.
 * - @ref BH1750_RESULT_CODE_DRIVER_ERR Something went wrong in the code of this driver.
 *
 * @param[in] self BH1750 instance created by @ref bh1750_create.
 * @param[in] meas_time Measurement time to set. According to the datasheet: 31 <= @p meas_time <= 254.
 * @param[in] cb Callback to execute once set measurement time sequence is complete.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BH1750_RESULT_CODE_OK Successfully initiated set measurement time.
 * @retval BH1750_RESULT_CODE_INVALID_ARG @p self is NULL, or @p meas_time is not within the allowed range.
 * @retval BH1750_RESULT_CODE_DRIVER_ERR Something went weong in the code of this driver.
 */
uint8_t bh1750_set_measurement_time(BH1750 self, uint8_t meas_time, BH1750CompleteCb cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* SRC_BH1750_H */

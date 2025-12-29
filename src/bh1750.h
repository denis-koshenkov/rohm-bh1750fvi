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
 * @brief Power on BH1750 device.
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

uint8_t bh1750_start_continuous_measurement(BH1750 self, uint8_t meas_mode, BH1750CompleteCb cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* SRC_BH1750_H */

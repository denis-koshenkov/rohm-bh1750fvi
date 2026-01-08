#ifndef SRC_BH1750_PRIVATE_H
#define SRC_BH1750_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "bh1750_defs.h"

/* This header should be included only by the user module implementing the BH1750GetInstanceMemory callback which is a
 * part of InitConfig passed to bh1750_create. All other user modules are not allowed to include this header, because
 * otherwise they would know about the BH1750Struct struct definition and can manipulate private data of a BH1750
 * instance directly. */

/* Defined in a separate header, so that both bh1750.c and the user module implementing BH1750GetInstanceMemory callback
 * can include this header. The user module needs to know sizeof(BH1750Struct), so that it knows the size of BH1750
 * instances at compile time. This way, it has an option to allocate a static array with size equal to the required
 * number of instances. */
struct BH1750Struct {
    /** @brief User-defined I2C write function that was passed to bh1750_create. */
    BH1750_I2CWrite i2c_write;
    /** @brief User data to pass to i2c_write. */
    void *i2c_write_user_data;
    /** @brief User-defined I2C read function that was passed to bh1750_create. */
    BH1750_I2CRead i2c_read;
    /** @brief User data to pass to i2c_read. */
    void *i2c_read_user_data;
    /** @brief User-defined start timer function that was passed to bh1750_create. */
    BH1750StartTimer start_timer;
    /** @brief User data to pass to start_timer. */
    void *start_timer_user_data;
    /** @brief Callback to execute once current sequence is complete.
     *
     * void * because different sequences types might require different callback types to be executed.
     */
    void *seq_cb;
    /** @brief User data to pass to seq_cb. */
    void *seq_cb_user_data;
    /** @brief Address to write measurement in lx. Only valid for read_cont_meas sequence. */
    uint32_t *meas_p;
    /** @brief I2C address of this BH1750 instance. */
    uint8_t i2c_addr;
    /** @brief Used only in the set_meas_time sequence. */
    uint8_t meas_time_to_set;
    /** @brief This buffer is passed to i2c_read function to save the received data. */
    uint8_t read_buf[2];
    /** @brief Whether continuous measurement is currently ongoing. */
    bool cont_meas_ongoing;
    /** @brief Current measurement mode. One of @ref BH1750MeasMode.
     *
     * Can be used in two ways:
     * - Set at the beginning of start continuous measurement sequence, so that subsequent calls to
     * bh1750_read_continuous_measurement can correctly convert raw measurement to lx.
     * - Set at the beginning of one time measurement sequence, so that the last part of the sequence can correctly
     * convert raw measurement to lx. */
    uint8_t meas_mode;
    /** @brief Current measurement time set in Mtreg.
     *
     * Used for converting raw measurements to light intensity in lx.
     */
    uint8_t meas_time;
    /** @brief Whether the instance is initialized. Set to true after init is called successfully. */
    bool initialized;
};

#ifdef __cplusplus
}
#endif

#endif /* SRC_BH1750_PRIVATE_H */

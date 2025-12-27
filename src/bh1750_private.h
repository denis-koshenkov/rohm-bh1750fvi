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
    /** @brief Callback to execute once current sequence is complete.
     *
     * void * because different sequences types might require different callback types to be executed.
     */
    void *seq_cb;
    /** @brief User data to pass to seq_cb. */
    void *seq_cb_user_data;
};

#ifdef __cplusplus
}
#endif

#endif /* SRC_BH1750_PRIVATE_H */

#ifndef TEST_MOCK_MOCK_CFG_FUNCTIONS_H
#define TEST_MOCK_MOCK_CFG_FUNCTIONS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "bh1750.h"

void *mock_bh1750_get_instance_memory(void *user_data);

void mock_bh1750_free_instance_memory(void *instance_memory, void *user_data);

void mock_bh1750_i2c_write(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data, BH1750_I2CCompleteCb cb,
                           void *cb_user_data);

void mock_bh1750_i2c_read(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data, BH1750_I2CCompleteCb cb,
                          void *cb_user_data);

void mock_bh1750_start_timer(uint32_t duration_ms, void *user_data, BH1750TimerExpiredCb cb, void *cb_user_data);

#ifdef __cplusplus
}
#endif

#endif /* TEST_MOCK_MOCK_CFG_FUNCTIONS_H */

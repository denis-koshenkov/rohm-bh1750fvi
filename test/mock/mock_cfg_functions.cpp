#include "CppUTestExt/MockSupport.h"
#include "mock_cfg_functions.h"

void *mock_bh1750_get_instance_memory(void *user_data)
{
    mock().actualCall("mock_bh1750_get_instance_memory").withParameter("user_data", user_data);
    return mock().pointerReturnValue();
}

void mock_bh1750_free_instance_memory(void *inst_mem, void *user_data)
{
    mock()
        .actualCall("mock_bh1750_free_instance_memory")
        .withParameter("inst_mem", inst_mem)
        .withParameter("user_data", user_data);
}

void mock_bh1750_i2c_write(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data, BH1750_I2CCompleteCb cb,
                           void *cb_user_data)
{
    BH1750_I2CCompleteCb *cb_p = (BH1750_I2CCompleteCb *)mock().getData("i2cWriteCompleteCb").getPointerValue();
    void **cb_user_data_p = (void **)mock().getData("i2cWriteCompleteCbUserData").getPointerValue();
    *cb_p = cb;
    *cb_user_data_p = cb_user_data;

    mock()
        .actualCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", data, length)
        .withParameter("length", length)
        .withParameter("i2c_addr", i2c_addr)
        .withParameter("user_data", user_data)
        .withParameter("cb", cb)
        .withParameter("cb_user_data", cb_user_data);
}

void mock_bh1750_i2c_read(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data, BH1750_I2CCompleteCb cb,
                          void *cb_user_data)
{
    BH1750_I2CCompleteCb *cb_p = (BH1750_I2CCompleteCb *)mock().getData("i2cReadCompleteCb").getPointerValue();
    void **cb_user_data_p = (void **)mock().getData("i2cReadCompleteCbUserData").getPointerValue();
    *cb_p = cb;
    *cb_user_data_p = cb_user_data;

    mock()
        .actualCall("mock_bh1750_i2c_read")
        .withOutputParameter("data", data)
        .withParameter("length", length)
        .withParameter("i2c_addr", i2c_addr)
        .withParameter("user_data", user_data)
        .withParameter("cb", cb)
        .withParameter("cb_user_data", cb_user_data);
}

void mock_bh1750_start_timer(uint32_t duration_ms, void *user_data, BH1750TimerExpiredCb cb, void *cb_user_data)
{
    BH1750TimerExpiredCb *cb_p = (BH1750TimerExpiredCb *)mock().getData("timerExpiredCb").getPointerValue();
    void **cb_user_data_p = (void **)mock().getData("timerExpiredCbUserData").getPointerValue();
    *cb_p = cb;
    *cb_user_data_p = cb_user_data;

    mock()
        .actualCall("mock_bh1750_start_timer")
        .withParameter("duration_ms", duration_ms)
        .withParameter("user_data", user_data)
        .withParameter("cb", cb)
        .withParameter("cb_user_data", cb_user_data);
}

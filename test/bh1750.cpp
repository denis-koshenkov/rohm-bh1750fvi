#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "bh1750.h"
/* Included to know the size of BH1750 instance we need to define to return from mock_bh1750_get_instance_memory. */
#include "bh1750_private.h"
#include "mock_cfg_functions.h"

#define BH1750_TEST_DEFAULT_I2C_ADDR 0x23
#define BH1750_TEST_ALT_I2C_ADDR 0x5C

/* To return from mock_bh1750_get_instance_memory */
static struct BH1750Struct instance_memory;

static BH1750 bh1750;
/* Init cfg used in all tests. It is populated in the setup before each test with default values. The test has the
 * option to adjust the values before calling bh1750_create. */
static BH1750InitConfig init_cfg;

/* Populated by mock object whenever mock_bh1750_i2c_write is called */
static BH1750_I2CCompleteCb i2c_write_complete_cb;
static void *i2c_write_complete_cb_user_data;

/* Populated by mock object whenever mock_bh1750_i2c_read is called */
static BH1750_I2CCompleteCb i2c_read_complete_cb;
static void *i2c_read_complete_cb_user_data;

/* User data parameters to pass to bh1750_create in the init cfg */
static void *get_instance_memory_user_data = (void *)0xDE;
static void *i2c_write_user_data = (void *)0x78;
static void *i2c_read_user_data = (void *)0x9A;
static void *start_timer_user_data = (void *)0xBC;

static size_t complete_cb_call_count;
static uint8_t complete_cb_result_code;
static void *complete_cb_user_data;

static void bh1750_complete_cb(uint8_t result_code, void *user_data)
{
    complete_cb_call_count++;
    complete_cb_result_code = result_code;
    complete_cb_user_data = user_data;
}

static void populate_default_init_cfg(BH1750InitConfig *const cfg)
{
    cfg->get_instance_memory = mock_bh1750_get_instance_memory;
    cfg->get_instance_memory_user_data = get_instance_memory_user_data;
    cfg->i2c_write = mock_bh1750_i2c_write;
    cfg->i2c_write_user_data = i2c_write_user_data;
    cfg->i2c_read = mock_bh1750_i2c_read;
    cfg->i2c_read_user_data = i2c_read_user_data;
    cfg->i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR;
}

// clang-format off
TEST_GROUP(BH1750)
{
    void setup() {
        /* Order of expected calls is important for these tests. Fail the test if the expected mock calls do not happen
        in the specified order. */
        mock().strictOrder();

        /* Reset all values populated by mock object */
        i2c_write_complete_cb = NULL;
        i2c_write_complete_cb_user_data = NULL;
        i2c_read_complete_cb = NULL;
        i2c_read_complete_cb_user_data = NULL;

        /* Pass pointers so that the mock object populates them with callbacks and user data, so that the test can simulate
        calling these callbacks. */
        mock().setData("i2cWriteCompleteCb", (void *)&i2c_write_complete_cb);
        mock().setData("i2cWriteCompleteCbUserData", &i2c_write_complete_cb_user_data);
        mock().setData("i2cReadCompleteCb", (void *)&i2c_read_complete_cb);
        mock().setData("i2cReadCompleteCbUserData", &i2c_read_complete_cb_user_data);

        /* Reset variables populated from inside bh1750_complete_cb */
        complete_cb_call_count = 0;
        complete_cb_result_code = 0xFF;
        complete_cb_user_data = NULL;

        bh1750 = NULL;
        memset(&init_cfg, 0, sizeof(BH1750InitConfig));
        memset(&instance_memory, 0, sizeof(struct BH1750Struct));

        /* Every test should call bh1750_create at the beginning, which will call this mock */
        mock()
            .expectOneCall("mock_bh1750_get_instance_memory")
            .withParameter("user_data", get_instance_memory_user_data)
            .andReturnValue((void *)&instance_memory);

        populate_default_init_cfg(&init_cfg);
    }
};
// clang-format on

typedef uint8_t (*SendCmdFunc)(BH1750 self, BH1750CompleteCb cb, void *user_data);

/* Macros to pass to is_start_meas_cmd of test_send_cmd_func and test_invalid_arg */
#define BH1750_TEST_START_MEAS_CMD true
#define BH1750_TEST_SEND_FUNC_CMD false

typedef enum {
    /** Test a send command function. Use if the function to be tested has the same signature as @ref SendCmdFunc. */
    INVALID_ARG_TEST_TYPE_SEND_CMD_FUNC,
    /** Test bh1750_start_continuous_measurement function. */
    INVALID_ARG_TEST_TYPE_START_CONT_MEAS,
    /** Test bh1750_set_measurement_time function. */
    INVALID_ARG_TEST_TYPE_SET_MEAS_TIME,
} InvalidArgTestType;

/**
 * @brief Test a function that sends a command to BH1750.
 *
 * @param is_start_meas_cmd True if the function being tested is bh1750_start_continuous_measurement, false otherwise.
 * @param meas_mode Measurement mode to test bh1750_start_continuous_measurement. If @p is_start_meas_cmd is false, this
 * parameter is ignored.
 * @param send_cmd_func Function to test. This parameter is ignored if @p is_start_meas_cmd is true, because then we
 * know that bh1750_start_continuous_measurement is being tested.
 * @param i2c_addr I2C address to create BH1750 instance with.
 * @param complete_cb Complete callback to execute once the command is sent.
 * @param i2c_write_data I2C write data that the test should expect to be passed to mock_bh1750_i2c_write as "data"
 * parameter.
 * @param i2c_write_rc I2C result code that the test should invoke i2c_write_complete_cb with.
 * @param expected_complete_cb_rc Result code that the tests expects to be returned from @p complete_cb.
 */
static void test_send_cmd_func(bool is_start_meas_cmd, uint8_t meas_mode, SendCmdFunc send_cmd_func, uint8_t i2c_addr,
                               BH1750CompleteCb complete_cb, uint8_t *i2c_write_data, uint8_t i2c_write_rc,
                               uint8_t expected_complete_cb_rc)
{
    if (!is_start_meas_cmd && !send_cmd_func) {
        /* send_cmd_func arg is only used when is_start_meas_cmd if false */
        FAIL_TEST("send_cmd_func is NULL");
    }

    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    void *complete_cb_user_data_expected = (void *)0x10;
    uint8_t rc;
    if (is_start_meas_cmd) {
        rc = bh1750_start_continuous_measurement(bh1750, meas_mode, complete_cb, complete_cb_user_data_expected);
    } else {
        rc = send_cmd_func(bh1750, complete_cb, complete_cb_user_data_expected);
    }
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    if (complete_cb) {
        CHECK_EQUAL(1, complete_cb_call_count);
        CHECK_EQUAL(expected_complete_cb_rc, complete_cb_result_code);
        CHECK_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
    }
}

/**
 * @brief Test a function when @ref BH1750_RESULT_CODE_INVALID_ARG is expected to be returned.
 *
 * @param inst_p Pointer to BH1750 instance to be passed to @p send_cmd_func or @ref
 * bh1750_start_continuous_measurement. NULL if NULL should be passed as instance to the "self" parameter.
 * @param test_type Defines the test type. One of @ref InvalidArgTestType.
 * @param test_type_context Test-type specific parameters. Should point to different data depending on the value of @p
 * test_type:
 * - INVALID_ARG_TEST_TYPE_SEND_CMD_FUNC: Should be a function pointer of type SendCmdFunc. The test calls this function
 * and expects it to return @ref BH1750_RESULT_CODE_INVALID_ARG.
 * - INVALID_ARG_TEST_TYPE_START_CONT_MEAS: Should point to a uint8_t which has one of the values from @ref
 * BH1750MeasMode. This defines the value of meas_mode argument passed to bh1750_start_continuous_measurement.
 * - INVALID_ARG_TEST_TYPE_SET_MEAS_TIME: Should be a pointer to uint8_t. The uint8_t value is interpreted as the
 * measurement time to pass to bh1750_set_measurement_time as the "meas_time" parameter.
 */
static void test_invalid_arg(BH1750 *inst_p, uint8_t test_type, void *test_type_context)
{
    /* bh1750_create should be called even if inst_p is NULL, because setup function expects a call to the
     * get_instance_memory mock. In that case, pass the static global bh1750 to bh1750_create. */
    BH1750 *inst_p_to_create = inst_p ? inst_p : &bh1750;
    uint8_t rc_create = bh1750_create(inst_p_to_create, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    void *complete_cb_user_data_expected = (void *)0x11;
    BH1750 inst = inst_p ? *inst_p : NULL;
    uint8_t rc;
    switch (test_type) {
    case INVALID_ARG_TEST_TYPE_SEND_CMD_FUNC: {
        SendCmdFunc send_cmd_func = (SendCmdFunc)test_type_context;
        rc = send_cmd_func(inst, bh1750_complete_cb, complete_cb_user_data_expected);
        break;
    }
    case INVALID_ARG_TEST_TYPE_START_CONT_MEAS: {
        uint8_t *meas_mode = (uint8_t *)test_type_context;
        rc = bh1750_start_continuous_measurement(inst, *meas_mode, bh1750_complete_cb, complete_cb_user_data_expected);
        break;
    }
    case INVALID_ARG_TEST_TYPE_SET_MEAS_TIME: {
        uint8_t *meas_time = (uint8_t *)test_type_context;
        rc = bh1750_set_measurement_time(inst, *meas_time, bh1750_complete_cb, complete_cb_user_data_expected);
        break;
    }
    }

    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

TEST(BH1750, PowerOnWriteFail)
{
    /* Power on command */
    uint8_t i2c_write_data = 0x1;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_power_on, BH1750_TEST_DEFAULT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_ERR, BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, PowerOnWriteSuccess)
{
    /* Power on command */
    uint8_t i2c_write_data = 0x1;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_power_on, BH1750_TEST_DEFAULT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerOnCbNull)
{
    /* Power on command */
    uint8_t i2c_write_data = 0x1;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_power_on, BH1750_TEST_DEFAULT_I2C_ADDR,
                       NULL, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerOnSelfNull)
{
    test_invalid_arg(NULL, INVALID_ARG_TEST_TYPE_SEND_CMD_FUNC, (void *)bh1750_power_on);
}

TEST(BH1750, PowerOnWriteSuccessAltI2cAddr)
{
    /* Power on command */
    uint8_t i2c_write_data = 0x1;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_power_on, BH1750_TEST_ALT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerDownWriteFail)
{
    /* Power down command */
    uint8_t i2c_write_data = 0x0;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_power_down,
                       BH1750_TEST_DEFAULT_I2C_ADDR, bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_ERR,
                       BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, PowerDownWriteSuccess)
{
    /* Power down command */
    uint8_t i2c_write_data = 0x0;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_power_down,
                       BH1750_TEST_DEFAULT_I2C_ADDR, bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK,
                       BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerDownCbNullSuccess)
{
    /* Power down command */
    uint8_t i2c_write_data = 0x0;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_power_down,
                       BH1750_TEST_DEFAULT_I2C_ADDR, NULL, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK,
                       BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerDownAltI2cAddr)
{
    /* Power down command */
    uint8_t i2c_write_data = 0x0;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_power_down, BH1750_TEST_ALT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerDownSelfNull)
{
    test_invalid_arg(NULL, INVALID_ARG_TEST_TYPE_SEND_CMD_FUNC, (void *)bh1750_power_down);
}

TEST(BH1750, ResetWriteFail)
{
    /* Reset command */
    uint8_t i2c_write_data = 0x07;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_reset, BH1750_TEST_DEFAULT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_ERR, BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, ResetWriteSuccess)
{
    /* Reset command */
    uint8_t i2c_write_data = 0x07;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_reset, BH1750_TEST_DEFAULT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, ResetCbNull)
{
    /* Reset command */
    uint8_t i2c_write_data = 0x07;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_reset, BH1750_TEST_DEFAULT_I2C_ADDR,
                       NULL, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, ResetAltI2cAddr)
{
    /* Reset command */
    uint8_t i2c_write_data = 0x07;
    test_send_cmd_func(BH1750_TEST_SEND_FUNC_CMD, BH1750_MEAS_MODE_H_RES, bh1750_reset, BH1750_TEST_ALT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, ResetSelfNull)
{
    test_invalid_arg(NULL, INVALID_ARG_TEST_TYPE_SEND_CMD_FUNC, (void *)bh1750_reset);
}

TEST(BH1750, StartContMeasWriteFail)
{
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    test_send_cmd_func(BH1750_TEST_START_MEAS_CMD, BH1750_MEAS_MODE_H_RES, NULL, BH1750_TEST_DEFAULT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_ERR, BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, StartContMeasWriteSuccess)
{
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    test_send_cmd_func(BH1750_TEST_START_MEAS_CMD, BH1750_MEAS_MODE_H_RES, NULL, BH1750_TEST_DEFAULT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, StartContMeasCbNull)
{
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    test_send_cmd_func(BH1750_TEST_START_MEAS_CMD, BH1750_MEAS_MODE_H_RES, NULL, BH1750_TEST_DEFAULT_I2C_ADDR, NULL,
                       &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, StartContMeasAltI2cAddr)
{
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    test_send_cmd_func(BH1750_TEST_START_MEAS_CMD, BH1750_MEAS_MODE_H_RES, NULL, BH1750_TEST_ALT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, StartContMeasSelfNull)
{
    uint8_t meas_mode = BH1750_MEAS_MODE_H_RES;
    test_invalid_arg(NULL, INVALID_ARG_TEST_TYPE_START_CONT_MEAS, (void *)&meas_mode);
}

TEST(BH1750, StartContMeasHResMode2)
{
    /* Start continuous measurement in H-resolution mode 2 cmd */
    uint8_t i2c_write_data = 0x11;
    test_send_cmd_func(BH1750_TEST_START_MEAS_CMD, BH1750_MEAS_MODE_H_RES2, NULL, BH1750_TEST_DEFAULT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, StartContMeasLResMode)
{
    /* Start continuous measurement in L-resolution mode cmd */
    uint8_t i2c_write_data = 0x13;
    test_send_cmd_func(BH1750_TEST_START_MEAS_CMD, BH1750_MEAS_MODE_L_RES, NULL, BH1750_TEST_DEFAULT_I2C_ADDR,
                       bh1750_complete_cb, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, StartContMeasInvalidMeasMode)
{
    uint8_t invalid_meas_mode = 0xFB;
    test_invalid_arg(&bh1750, INVALID_ARG_TEST_TYPE_START_CONT_MEAS, (void *)&invalid_meas_mode);
}

/**
 * @brief Test bh1750_set_measurement_time function.
 *
 * @param i2c_addr I2C address to create BH1750 instance with.
 * @param meas_time Measurement time to pass to bh1750_set_measurement_time function.
 * @param i2c_write_data_1 I2C write data that the test should expect to be passed to the first call to
 * mock_bh1750_i2c_write as "data" parameter. Should point to 1 byte.
 * @param i2c_write_rc_1 I2C result code that the test should invoke the first i2c_write_complete_cb with.
 * @param i2c_write_data_1 I2C write data that the test should expect to be passed to the second call to
 * mock_bh1750_i2c_write as "data" parameter. Should point to 1 byte.
 * @param i2c_write_rc_2 I2C result code that the test should invoke the second i2c_write_complete_cb with.
 * @param complete_cb Complete callback to execute once the command is sent.
 * @param expected_complete_cb_rc Result code that the tests expects to be returned from @p complete_cb.
 */
static void test_set_meas_time(uint8_t i2c_addr, uint8_t meas_time, uint8_t *i2c_write_data_1, uint8_t i2c_write_rc_1,
                               uint8_t *i2c_write_data_2, uint8_t i2c_write_rc_2, BH1750CompleteCb complete_cb,
                               uint8_t expected_complete_cb_rc)
{
    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data_1, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    if (i2c_write_rc_1 == BH1750_I2C_RESULT_CODE_OK) {
        mock()
            .expectOneCall("mock_bh1750_i2c_write")
            .withMemoryBufferParameter("data", i2c_write_data_2, 1)
            .withParameter("length", 1)
            .withParameter("i2c_addr", i2c_addr)
            .withParameter("user_data", i2c_write_user_data)
            .ignoreOtherParameters();
    }

    void *complete_cb_user_data_expected = (void *)0x12;
    uint8_t rc = bh1750_set_measurement_time(bh1750, meas_time, complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc_1, i2c_write_complete_cb_user_data);
    if (i2c_write_rc_1 == BH1750_I2C_RESULT_CODE_OK) {
        i2c_write_complete_cb(i2c_write_rc_2, i2c_write_complete_cb_user_data);
    }

    if (complete_cb) {
        CHECK_EQUAL(1, complete_cb_call_count);
        CHECK_EQUAL(expected_complete_cb_rc, complete_cb_result_code);
        CHECK_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
    }
}

TEST(BH1750, SetMeasTimeWrite1Fail)
{
    /* bin: 01000101 */
    uint8_t meas_time = 69;
    /* Set three most significant bits of MTreg to 010 */
    uint8_t i2c_write_data_1 = 0x42;
    test_set_meas_time(BH1750_TEST_DEFAULT_I2C_ADDR, meas_time, &i2c_write_data_1, BH1750_I2C_RESULT_CODE_ERR, NULL,
                       BH1750_I2C_RESULT_CODE_ERR, bh1750_complete_cb, BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, SetMeasTimeWrite2Fail)
{
    /* bin: 10001010 */
    uint8_t meas_time = 138;
    /* Set three most significant bits of MTreg to 100 */
    uint8_t i2c_write_data_1 = 0x44;
    /* Set five least significant bits of MTreg to 01010 */
    uint8_t i2c_write_data_2 = 0x6A;
    test_set_meas_time(BH1750_TEST_DEFAULT_I2C_ADDR, meas_time, &i2c_write_data_1, BH1750_I2C_RESULT_CODE_OK,
                       &i2c_write_data_2, BH1750_I2C_RESULT_CODE_ERR, bh1750_complete_cb, BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, SetMeasTimeSuccess)
{
    /* bin: 00011111 */
    uint8_t meas_time = 31;
    /* Set three most significant bits of MTreg to 000 */
    uint8_t i2c_write_data_1 = 0x40;
    /* Set five least significant bits of MTreg to 11111 */
    uint8_t i2c_write_data_2 = 0x7F;
    test_set_meas_time(BH1750_TEST_DEFAULT_I2C_ADDR, meas_time, &i2c_write_data_1, BH1750_I2C_RESULT_CODE_OK,
                       &i2c_write_data_2, BH1750_I2C_RESULT_CODE_OK, bh1750_complete_cb, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, SetMeasTimeSuccess254)
{
    /* bin: 11111110 */
    uint8_t meas_time = 254;
    /* Set three most significant bits of MTreg to 111 */
    uint8_t i2c_write_data_1 = 0x47;
    /* Set five least significant bits of MTreg to 11110 */
    uint8_t i2c_write_data_2 = 0x7E;
    test_set_meas_time(BH1750_TEST_DEFAULT_I2C_ADDR, meas_time, &i2c_write_data_1, BH1750_I2C_RESULT_CODE_OK,
                       &i2c_write_data_2, BH1750_I2C_RESULT_CODE_OK, bh1750_complete_cb, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, SetMeasTimeSuccess44)
{
    /* bin: 00101100 */
    uint8_t meas_time = 44;
    /* Set three most significant bits of MTreg to 001 */
    uint8_t i2c_write_data_1 = 0x41;
    /* Set five least significant bits of MTreg to 01100 */
    uint8_t i2c_write_data_2 = 0x6C;
    test_set_meas_time(BH1750_TEST_DEFAULT_I2C_ADDR, meas_time, &i2c_write_data_1, BH1750_I2C_RESULT_CODE_OK,
                       &i2c_write_data_2, BH1750_I2C_RESULT_CODE_OK, bh1750_complete_cb, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, SetMeasTimeCbNull)
{
    /* bin: 00011111 */
    uint8_t meas_time = 31;
    /* Set three most significant bits of MTreg to 000 */
    uint8_t i2c_write_data_1 = 0x40;
    /* Set five least significant bits of MTreg to 11111 */
    uint8_t i2c_write_data_2 = 0x7F;
    test_set_meas_time(BH1750_TEST_DEFAULT_I2C_ADDR, meas_time, &i2c_write_data_1, BH1750_I2C_RESULT_CODE_OK,
                       &i2c_write_data_2, BH1750_I2C_RESULT_CODE_OK, NULL, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, SetMeasTimeAltI2cAddr)
{
    /* bin: 00011111 */
    uint8_t meas_time = 31;
    /* Set three most significant bits of MTreg to 000 */
    uint8_t i2c_write_data_1 = 0x40;
    /* Set five least significant bits of MTreg to 11111 */
    uint8_t i2c_write_data_2 = 0x7F;
    test_set_meas_time(BH1750_TEST_ALT_I2C_ADDR, meas_time, &i2c_write_data_1, BH1750_I2C_RESULT_CODE_OK,
                       &i2c_write_data_2, BH1750_I2C_RESULT_CODE_OK, NULL, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, SetMeasTimeSelfNull)
{
    uint8_t meas_time = 69;
    test_invalid_arg(NULL, INVALID_ARG_TEST_TYPE_SET_MEAS_TIME, (void *)&meas_time);
}

TEST(BH1750, SetMeasTimeInvalMeasTime)
{
    uint8_t meas_time = 30;
    test_invalid_arg(&bh1750, INVALID_ARG_TEST_TYPE_SET_MEAS_TIME, (void *)&meas_time);
}

TEST(BH1750, SetMeasTimeInvalMeasTime2)
{
    uint8_t meas_time = 255;
    test_invalid_arg(&bh1750, INVALID_ARG_TEST_TYPE_SET_MEAS_TIME, (void *)&meas_time);
}

TEST(BH1750, SetMeasTimeInvalMeasTime3)
{
    uint8_t meas_time = 0;
    test_invalid_arg(&bh1750, INVALID_ARG_TEST_TYPE_SET_MEAS_TIME, (void *)&meas_time);
}

static void test_init(uint8_t i2c_addr, uint8_t i2c_write_rc_1, uint8_t i2c_write_rc_2, uint8_t i2c_write_rc_3,
                      BH1750CompleteCb complete_cb, uint8_t expected_complete_cb_rc)
{
    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    /* Power on command */
    uint8_t i2c_write_data_1 = 0x01;
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &i2c_write_data_1, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    if (i2c_write_rc_1 == BH1750_I2C_RESULT_CODE_OK) {
        /* Set three most significant bits of MTreg to 010 */
        uint8_t i2c_write_data_2 = 0x42;
        mock()
            .expectOneCall("mock_bh1750_i2c_write")
            .withMemoryBufferParameter("data", &i2c_write_data_2, 1)
            .withParameter("length", 1)
            .withParameter("i2c_addr", i2c_addr)
            .withParameter("user_data", i2c_write_user_data)
            .ignoreOtherParameters();
        if (i2c_write_rc_2 == BH1750_I2C_RESULT_CODE_OK) {
            /* Set five least significant bits of MTreg to 00101 */
            uint8_t i2c_write_data_3 = 0x65;
            mock()
                .expectOneCall("mock_bh1750_i2c_write")
                .withMemoryBufferParameter("data", &i2c_write_data_3, 1)
                .withParameter("length", 1)
                .withParameter("i2c_addr", i2c_addr)
                .withParameter("user_data", i2c_write_user_data)
                .ignoreOtherParameters();
        }
    }

    void *complete_cb_user_data_expected = (void *)0x14;
    uint8_t rc = bh1750_init(bh1750, complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc_1, i2c_write_complete_cb_user_data);
    if (i2c_write_rc_1 == BH1750_I2C_RESULT_CODE_OK) {
        i2c_write_complete_cb(i2c_write_rc_2, i2c_write_complete_cb_user_data);
        if (i2c_write_rc_2 == BH1750_I2C_RESULT_CODE_OK) {
            i2c_write_complete_cb(i2c_write_rc_3, i2c_write_complete_cb_user_data);
        }
    }

    if (complete_cb) {
        CHECK_EQUAL(1, complete_cb_call_count);
        CHECK_EQUAL(expected_complete_cb_rc, complete_cb_result_code);
        CHECK_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
    }
}

TEST(BH1750, InitWrite1Fail)
{
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_ERR;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_ERR;
    uint8_t i2c_write_rc_3 = BH1750_I2C_RESULT_CODE_ERR;
    test_init(BH1750_TEST_DEFAULT_I2C_ADDR, i2c_write_rc_1, i2c_write_rc_2, i2c_write_rc_3, bh1750_complete_cb,
              BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, InitWrite2Fail)
{
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_ERR;
    uint8_t i2c_write_rc_3 = BH1750_I2C_RESULT_CODE_ERR;
    test_init(BH1750_TEST_DEFAULT_I2C_ADDR, i2c_write_rc_1, i2c_write_rc_2, i2c_write_rc_3, bh1750_complete_cb,
              BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, InitWrite3Fail)
{
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t i2c_write_rc_3 = BH1750_I2C_RESULT_CODE_ERR;
    test_init(BH1750_TEST_DEFAULT_I2C_ADDR, i2c_write_rc_1, i2c_write_rc_2, i2c_write_rc_3, bh1750_complete_cb,
              BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, InitSuccess)
{
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t i2c_write_rc_3 = BH1750_I2C_RESULT_CODE_OK;
    test_init(BH1750_TEST_DEFAULT_I2C_ADDR, i2c_write_rc_1, i2c_write_rc_2, i2c_write_rc_3, bh1750_complete_cb,
              BH1750_RESULT_CODE_OK);
}

TEST(BH1750, InitCbNull)
{
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t i2c_write_rc_3 = BH1750_I2C_RESULT_CODE_OK;
    test_init(BH1750_TEST_DEFAULT_I2C_ADDR, i2c_write_rc_1, i2c_write_rc_2, i2c_write_rc_3, NULL,
              BH1750_RESULT_CODE_OK);
}

TEST(BH1750, InitAltI2cAddr)
{
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t i2c_write_rc_3 = BH1750_I2C_RESULT_CODE_OK;
    test_init(BH1750_TEST_ALT_I2C_ADDR, i2c_write_rc_1, i2c_write_rc_2, i2c_write_rc_3, bh1750_complete_cb,
              BH1750_RESULT_CODE_OK);
}

TEST(BH1750, InitSelfNull)
{
    test_invalid_arg(NULL, INVALID_ARG_TEST_TYPE_SEND_CMD_FUNC, (void *)bh1750_init);
}

typedef struct {
    /** I2C address to pass to bh1750_create init cfg. */
    uint8_t i2c_addr;
    /** Must point to 2 bytes that will be copied to the "data" parameter of i2c_read. */
    uint8_t *i2c_read_data;
    /** I2C return code to execute I2C read complete callback with. */
    uint8_t i2c_read_rc;
    /** Expected measurement in lx. It is only checked if expected_complete_cb_rc is BH1750_RESULT_CODE_OK. */
    uint32_t expected_meas_lx;
    /** Complete callback to execute once bh1750_read_continuous_measurement is complete. */
    BH1750CompleteCb complete_cb;
    /** Expected complete callback rc. Only checked if complete_cb is not NULL. */
    uint8_t expected_complete_cb_rc;
} TestReadContMeasCfg;

static void test_read_cont_meas(const TestReadContMeasCfg *const cfg)
{
    init_cfg.i2c_addr = cfg->i2c_addr;
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    /* bh1750_start_continuous_measurement */
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &i2c_write_data, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", cfg->i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    /* bh1750_read_continuous_measurement */
    mock()
        .expectOneCall("mock_bh1750_i2c_read")
        .withOutputParameterReturning("data", cfg->i2c_read_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", cfg->i2c_addr)
        .withParameter("user_data", i2c_read_user_data)
        .ignoreOtherParameters();

    /* Before reading continuous measurement, it needs to be started */
    uint8_t rc_start_meas = bh1750_start_continuous_measurement(bh1750, BH1750_MEAS_MODE_H_RES, NULL, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_start_meas);
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);

    void *complete_cb_user_data_expected = (void *)0x15;
    uint32_t meas_lx;
    uint8_t rc = bh1750_read_continuous_measurement(bh1750, &meas_lx, cfg->complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(cfg->i2c_read_rc, i2c_read_complete_cb_user_data);

    if (cfg->complete_cb) {
        CHECK_EQUAL(1, complete_cb_call_count);
        CHECK_EQUAL(cfg->expected_complete_cb_rc, complete_cb_result_code);
        CHECK_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
    }
    if (cfg->expected_complete_cb_rc == BH1750_RESULT_CODE_OK) {
        CHECK_EQUAL(cfg->expected_meas_lx, meas_lx);
    }
}

TEST(BH1750, ReadContMeasReadFail)
{
    /* Read fails, data does not matter */
    uint8_t i2c_read_data[] = {0x0, 0x0};
    TestReadContMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_ERR,
        .expected_meas_lx = 0, /* Don't care */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_IO_ERR,
    };
    test_read_cont_meas(&cfg);
}

TEST(BH1750, ReadContMeasSuccess)
{
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadContMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 28067,
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_cont_meas(&cfg);
}

TEST(BH1750, ReadContMeasSuccess2)
{
    uint8_t i2c_read_data[] = {0x75, 0x4F};
    TestReadContMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 25026,
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_cont_meas(&cfg);
}

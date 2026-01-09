#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "bh1750.h"
/* Included to know the size of BH1750 instance we need to define to return from mock_bh1750_get_instance_memory. */
#include "bh1750_private.h"
#include "mock_cfg_functions.h"

#define BH1750_TEST_DEFAULT_I2C_ADDR 0x23
#define BH1750_TEST_ALT_I2C_ADDR 0x5C

#define BH1750_TEST_DEFAULT_MEAS_TIME 69

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

/* Populated by mock object whenever mock_bh1750_start_timer is called */
static BH1750TimerExpiredCb timer_expired_cb;
static void *timer_expired_cb_user_data;

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
    cfg->start_timer = mock_bh1750_start_timer;
    cfg->start_timer_user_data = start_timer_user_data;
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
        timer_expired_cb = NULL;
        timer_expired_cb_user_data = NULL;

        /* Pass pointers so that the mock object populates them with callbacks and user data, so that the test can simulate
        calling these callbacks. */
        mock().setData("i2cWriteCompleteCb", (void *)&i2c_write_complete_cb);
        mock().setData("i2cWriteCompleteCbUserData", &i2c_write_complete_cb_user_data);
        mock().setData("i2cReadCompleteCb", (void *)&i2c_read_complete_cb);
        mock().setData("i2cReadCompleteCbUserData", &i2c_read_complete_cb_user_data);
        mock().setData("timerExpiredCb", (void *)&timer_expired_cb);
        mock().setData("timerExpiredCbUserData", &timer_expired_cb_user_data);

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

/**
 * @brief Records expected mock calls for bh1750_init and calls bh1750_init.
 *
 * This function cannot be in setup, because then we do not know yet the I2C address to expect for calls to
 * mock_bh1750_i2c_write. Every test can set the I2C address to a different value before calling bh1750_create. This is
 * why this function must be called in every test after bh1750_create.
 */
static void call_init()
{
    /* Power on command */
    uint8_t init_i2c_write_data_1 = 0x01;
    /* Set three most significant bits of MTreg to 010 */
    uint8_t init_i2c_write_data_2 = 0x42;
    /* Set five least significant bits of MTreg to 00101 */
    uint8_t init_i2c_write_data_3 = 0x65;
    /* bh1750_init */
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &init_i2c_write_data_1, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", init_cfg.i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &init_i2c_write_data_2, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", init_cfg.i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &init_i2c_write_data_3, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", init_cfg.i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc_init = bh1750_init(bh1750, NULL, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_init);
    /* init performs three writes - one for power on cmd, two for setting meas time in Mtreg */
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
}

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
    /** Test bh1750_read_continuous_measurement function. */
    INVALID_ARG_TEST_TYPE_READ_CONT_MEAS,
    /** Test bh1750_read_one_time_measurement function. */
    INVALID_ARG_TEST_TYPE_READ_ONE_TIME_MEAS,
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
    call_init();

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

/** Context for testing invalid argument scenarios of bh1750_read_one_time_measurement. */
typedef struct {
    /** Passed to meas_mode parameter of bh1750_read_one_time_measurement. */
    uint8_t meas_mode;
    /** Passed to meas_lx parameter of bh1750_read_one_time_measurement. */
    uint32_t *meas_lx_p;
} ReadOneTimeMeasInvalidArgTestCtxt;

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
 * - INVALID_ARG_TEST_TYPE_READ_CONT_MEAS: Should be a pointer to uint32_t. This pointer is passed to the meas_lx
 * parameter of bh1750_read_continuous_measurement. Can be NULL if needed for the test.
 * - INVALID_ARG_TEST_TYPE_READ_ONE_TIME_MEAS: Should be a pointer to ReadOneTimeMeasInvalidArgTestCtxt. meas_mode and
 * meas_lx_p are passed as arguments to the meas_mode and meas_lx parameters of bh1750_read_one_time_measurement
 * respectively.
 */
static void test_invalid_arg(BH1750 *inst_p, uint8_t test_type, void *test_type_context)
{
    /* bh1750_create should be called even if inst_p is NULL, because setup function expects a call to the
     * get_instance_memory mock. In that case, pass the static global bh1750 to bh1750_create. */
    BH1750 *inst_p_to_create = inst_p ? inst_p : &bh1750;
    uint8_t rc_create = bh1750_create(inst_p_to_create, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();

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
    case INVALID_ARG_TEST_TYPE_READ_CONT_MEAS: {
        /* Start continuous measurement in H-resolution mode cmd */
        uint8_t i2c_write_data = 0x10;
        /* bh1750_start_continuous_measurement */
        mock()
            .expectOneCall("mock_bh1750_i2c_write")
            .withMemoryBufferParameter("data", &i2c_write_data, 1)
            .withParameter("length", 1)
            .withParameter("i2c_addr", init_cfg.i2c_addr)
            .withParameter("user_data", i2c_write_user_data)
            .ignoreOtherParameters();

        /* Before reading continuous measurement, it needs to be started */
        uint8_t rc_start_meas = bh1750_start_continuous_measurement(bh1750, BH1750_MEAS_MODE_H_RES, NULL, NULL);
        CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_start_meas);
        i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);

        uint32_t *meas_lx = (uint32_t *)test_type_context;
        rc = bh1750_read_continuous_measurement(inst, meas_lx, bh1750_complete_cb, complete_cb_user_data_expected);
        break;
    }
    case INVALID_ARG_TEST_TYPE_READ_ONE_TIME_MEAS:
        ReadOneTimeMeasInvalidArgTestCtxt *ctxt = (ReadOneTimeMeasInvalidArgTestCtxt *)test_type_context;
        rc = bh1750_read_one_time_measurement(inst, ctxt->meas_mode, ctxt->meas_lx_p, bh1750_complete_cb,
                                              complete_cb_user_data_expected);
        break;
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
    call_init();

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
    /** Measurement mode to pass to bh1750_start_continuous_measurement. */
    uint8_t meas_mode;
    /** Must point to 1 bytes that will be expected in the "data" parameter of i2c_write. */
    uint8_t *i2c_write_data;
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
    call_init();

    /* bh1750_start_continuous_measurement */
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", cfg->i2c_write_data, 1)
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
    uint8_t rc_start_meas = bh1750_start_continuous_measurement(bh1750, cfg->meas_mode, NULL, NULL);
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
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    /* Read fails, data does not matter */
    uint8_t i2c_read_data[] = {0x0, 0x0};
    TestReadContMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_ERR,
        .expected_meas_lx = 0, /* Don't care */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_IO_ERR,
    };
    test_read_cont_meas(&cfg);
}

TEST(BH1750, ReadContMeasHResSuccess)
{
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadContMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 28067,
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_cont_meas(&cfg);
}

TEST(BH1750, ReadContMeasHResSuccess2)
{
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    uint8_t i2c_read_data[] = {0x75, 0x4F};
    TestReadContMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 25026,
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_cont_meas(&cfg);
}

TEST(BH1750, ReadContMeasHRes2Success)
{
    /* Start continuous measurement in H-resolution 2 mode cmd */
    uint8_t i2c_write_data = 0x11;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadContMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_mode = BH1750_MEAS_MODE_H_RES2,
        .i2c_write_data = &i2c_write_data,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 14033,
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_cont_meas(&cfg);
}

TEST(BH1750, ReadContMeasLResSuccess)
{
    /* Start continuous measurement in L-resolution mode cmd */
    uint8_t i2c_write_data = 0x13;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadContMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_mode = BH1750_MEAS_MODE_L_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 28067,
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_cont_meas(&cfg);
}

TEST(BH1750, ReadContMeasCbNull)
{
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadContMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 28067,
        .complete_cb = NULL,
        /* OK so that meas_lx is checked against the expected one */
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_cont_meas(&cfg);
}

TEST(BH1750, ReadContMeasAltI2cAddr)
{
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadContMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_ALT_I2C_ADDR,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 28067,
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_cont_meas(&cfg);
}

TEST(BH1750, ReadContMeasSelfNull)
{
    uint32_t meas_lx;
    test_invalid_arg(NULL, INVALID_ARG_TEST_TYPE_READ_CONT_MEAS, &meas_lx);
}

TEST(BH1750, ReadContMeasMeasLxNull)
{
    test_invalid_arg(&bh1750, INVALID_ARG_TEST_TYPE_READ_CONT_MEAS, NULL);
}

TEST(BH1750, ReadContMeasCalledBeforeStartContMeas)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();

    uint32_t meas_lx;
    uint8_t rc = bh1750_read_continuous_measurement(bh1750, &meas_lx, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

TEST(BH1750, ReadContMeasCalledAfterFailedStartContMeas)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();

    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    /* bh1750_start_continuous_measurement */
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &i2c_write_data, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", init_cfg.i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc_start_meas = bh1750_start_continuous_measurement(bh1750, BH1750_MEAS_MODE_H_RES, NULL, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_start_meas);
    /* Code ERR, so continuous measurement should not be marked as ongoing */
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_ERR, i2c_write_complete_cb_user_data);

    uint32_t meas_lx;
    uint8_t rc = bh1750_read_continuous_measurement(bh1750, &meas_lx, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

static void test_cont_meas_changes_with_meas_time(uint32_t meas_time, uint8_t *i2c_write_data_1, uint8_t i2c_write_rc_1,
                                                  uint8_t *i2c_write_data_2, uint8_t i2c_write_rc_2,
                                                  uint8_t cont_meas_mode, uint8_t *i2c_write_data_3,
                                                  uint8_t *i2c_read_data, uint32_t expected_meas_lx)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();

    /* bh1750_set_measurement_time */
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data_1, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", BH1750_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    if (i2c_write_rc_1 == BH1750_I2C_RESULT_CODE_OK) {
        mock()
            .expectOneCall("mock_bh1750_i2c_write")
            .withMemoryBufferParameter("data", i2c_write_data_2, 1)
            .withParameter("length", 1)
            .withParameter("i2c_addr", BH1750_TEST_DEFAULT_I2C_ADDR)
            .withParameter("user_data", i2c_write_user_data)
            .ignoreOtherParameters();
    }
    /* bh1750_start_continuous_measurement */
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data_3, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", BH1750_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    /* bh1750_read_continuous_measurement */
    mock()
        .expectOneCall("mock_bh1750_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", BH1750_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_read_user_data)
        .ignoreOtherParameters();

    uint8_t rc_set_meas_time = bh1750_set_measurement_time(bh1750, meas_time, NULL, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_set_meas_time);
    /* set_measurement_time writes two registers */
    i2c_write_complete_cb(i2c_write_rc_1, i2c_write_complete_cb_user_data);
    if (i2c_write_rc_1 == BH1750_I2C_RESULT_CODE_OK) {
        i2c_write_complete_cb(i2c_write_rc_2, i2c_write_complete_cb_user_data);
    }

    uint8_t rc_start_meas = bh1750_start_continuous_measurement(bh1750, cont_meas_mode, NULL, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_start_meas);
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);

    void *complete_cb_user_data_expected = (void *)0x16;
    uint32_t meas_lx;
    uint8_t rc =
        bh1750_read_continuous_measurement(bh1750, &meas_lx, bh1750_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, complete_cb_result_code);
    CHECK_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
    CHECK_EQUAL(expected_meas_lx, meas_lx);
}

TEST(BH1750, ReadContMeasHResMeasTime138)
{
    /* bin: 10001010 */
    uint8_t meas_time = 138;
    /* Set three most significant bits of MTreg to 100 */
    uint8_t i2c_write_data_1 = 0x44;
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_OK;
    /* Set five least significant bits of MTreg to 01010 */
    uint8_t i2c_write_data_2 = 0x6A;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t meas_mode = BH1750_MEAS_MODE_H_RES;
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data_3 = 0x10;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    uint32_t expected_meas_lx = 14033;
    test_cont_meas_changes_with_meas_time(meas_time, &i2c_write_data_1, i2c_write_rc_1, &i2c_write_data_2,
                                          i2c_write_rc_2, meas_mode, &i2c_write_data_3, i2c_read_data,
                                          expected_meas_lx);
}

TEST(BH1750, ReadContMeasHRes2MeasTime138)
{
    /* bin: 10001010 */
    uint8_t meas_time = 138;
    /* Set three most significant bits of MTreg to 100 */
    uint8_t i2c_write_data_1 = 0x44;
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_OK;
    /* Set five least significant bits of MTreg to 01010 */
    uint8_t i2c_write_data_2 = 0x6A;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t meas_mode = BH1750_MEAS_MODE_H_RES2;
    /* Start continuous measurement in H-resolution mode 2 cmd */
    uint8_t i2c_write_data_3 = 0x11;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    uint32_t expected_meas_lx = 7017;
    test_cont_meas_changes_with_meas_time(meas_time, &i2c_write_data_1, i2c_write_rc_1, &i2c_write_data_2,
                                          i2c_write_rc_2, meas_mode, &i2c_write_data_3, i2c_read_data,
                                          expected_meas_lx);
}

TEST(BH1750, ReadContMeasLResMeasTime138)
{
    /* bin: 10001010 */
    uint8_t meas_time = 138;
    /* Set three most significant bits of MTreg to 100 */
    uint8_t i2c_write_data_1 = 0x44;
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_OK;
    /* Set five least significant bits of MTreg to 01010 */
    uint8_t i2c_write_data_2 = 0x6A;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_OK;
    uint8_t meas_mode = BH1750_MEAS_MODE_L_RES;
    /* Start continuous measurement in L-resolution mode cmd */
    uint8_t i2c_write_data_3 = 0x13;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    /* Different meas time does not apply to low res mode. This is simply (0x8390 / 1.2f) - raw meas divided by 1.2. */
    uint32_t expected_meas_lx = 28067;
    test_cont_meas_changes_with_meas_time(meas_time, &i2c_write_data_1, i2c_write_rc_1, &i2c_write_data_2,
                                          i2c_write_rc_2, meas_mode, &i2c_write_data_3, i2c_read_data,
                                          expected_meas_lx);
}

TEST(BH1750, ReadContMeasSetMeasTimeWrite1Fail)
{
    /* bin: 10001010 */
    uint8_t meas_time = 138;
    /* Set three most significant bits of MTreg to 100 */
    uint8_t i2c_write_data_1 = 0x44;
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_ERR;
    /* Set five least significant bits of MTreg to 01010 */
    uint8_t i2c_write_data_2 = 0x6A;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_ERR;
    uint8_t meas_mode = BH1750_MEAS_MODE_H_RES;
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data_3 = 0x10;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    /* Setting meas time failed, so this is (0x8390 / 1.2f) -> using default meas time (69) */
    uint32_t expected_meas_lx = 28067;
    test_cont_meas_changes_with_meas_time(meas_time, &i2c_write_data_1, i2c_write_rc_1, &i2c_write_data_2,
                                          i2c_write_rc_2, meas_mode, &i2c_write_data_3, i2c_read_data,
                                          expected_meas_lx);
}

TEST(BH1750, ReadContMeasSetMeasTimeWrite2Fail)
{
    /* bin: 10001010 */
    uint8_t meas_time = 138;
    /* Set three most significant bits of MTreg to 100 */
    uint8_t i2c_write_data_1 = 0x44;
    uint8_t i2c_write_rc_1 = BH1750_I2C_RESULT_CODE_OK;
    /* Set five least significant bits of MTreg to 01010 */
    uint8_t i2c_write_data_2 = 0x6A;
    uint8_t i2c_write_rc_2 = BH1750_I2C_RESULT_CODE_ERR;
    uint8_t meas_mode = BH1750_MEAS_MODE_H_RES;
    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data_3 = 0x10;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    /* Meas time is 133 - 10000101 in binary. First three bits are from 138, because the first i2c write succeeded (it
     * sets the first three bits). The last five bits are from 69, because the second write failed (it sets the last
     * five bits). This value is: (0x8390 * (1 / 1.2) * (69 / 133)) */
    uint32_t expected_meas_lx = 14561;
    test_cont_meas_changes_with_meas_time(meas_time, &i2c_write_data_1, i2c_write_rc_1, &i2c_write_data_2,
                                          i2c_write_rc_2, meas_mode, &i2c_write_data_3, i2c_read_data,
                                          expected_meas_lx);
}

typedef struct {
    /** I2C address to pass to bh1750_create init cfg. */
    uint8_t i2c_addr;
    /* Measurement time to set before calling bh1750_read_one_time_measurement. If default (69) is passed, nothing is
     * done. Otherwise, bh1750_set_measurement_time is called with the specified meas time. */
    uint8_t meas_time;
    /** Must point to 1 byte that will be expected in the "data" parameter of first call of i2c_write triggered by
     * bh1750_set_measurement_time. This data should be the command to set the three high bits of Mtreg. Can be NULL if
     * meas_time is equal to default (69). */
    uint8_t *meas_time_i2c_write_data_1;
    /** Must point to 1 byte that will be expected in the "data" parameter of second call of i2c_write triggered by
     * bh1750_set_measurement_time. This data should be the command to set the five low bits of Mtreg. Can be NULL if
     * meas_time is equal to default (69). */
    uint8_t *meas_time_i2c_write_data_2;
    /** Measurement mode to pass to bh1750_read_one_time_measurement. */
    uint8_t meas_mode;
    /** Must point to 1 byte that will be expected in the "data" parameter of i2c_write. */
    uint8_t *i2c_write_data;
    /** Result code to call the I2C write complete callback with. */
    uint8_t i2c_write_rc;
    /** Timer period with which start_timer is expected to be called. */
    uint32_t timer_period;
    /** Must point to 2 bytes that will be copied to the "data" parameter of i2c_read. */
    uint8_t *i2c_read_data;
    /** I2C return code to execute I2C read complete callback with. */
    uint8_t i2c_read_rc;
    /** Expected measurement in lx. It is only checked if expected_complete_cb_rc is BH1750_RESULT_CODE_OK. */
    uint32_t expected_meas_lx;
    /** Complete callback to execute once bh1750_read_one_time_measurement is complete. */
    BH1750CompleteCb complete_cb;
    /** Expected complete callback rc. Only checked if complete_cb is not NULL. */
    uint8_t expected_complete_cb_rc;
} TestReadOneTimeMeasCfg;

static void set_meas_time(uint8_t i2c_addr, uint8_t meas_time, uint8_t *i2c_write_data_1, uint8_t *i2c_write_data_2)
{
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data_1, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data_2, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc_set_meas_time = bh1750_set_measurement_time(bh1750, meas_time, NULL, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_set_meas_time);

    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
}

static void test_read_one_time_meas(const TestReadOneTimeMeasCfg *const cfg)
{
    init_cfg.i2c_addr = cfg->i2c_addr;
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();
    if (cfg->meas_time != BH1750_TEST_DEFAULT_MEAS_TIME) {
        /* Default meas time is already set as a part of init, not necessary to set it again */
        set_meas_time(cfg->i2c_addr, cfg->meas_time, cfg->meas_time_i2c_write_data_1, cfg->meas_time_i2c_write_data_2);
    }

    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", cfg->i2c_write_data, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", cfg->i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    if (cfg->i2c_write_rc == BH1750_I2C_RESULT_CODE_OK) {
        mock()
            .expectOneCall("mock_bh1750_start_timer")
            .withParameter("duration_ms", cfg->timer_period)
            .withParameter("user_data", start_timer_user_data)
            .ignoreOtherParameters();
        mock()
            .expectOneCall("mock_bh1750_i2c_read")
            .withOutputParameterReturning("data", cfg->i2c_read_data, 2)
            .withParameter("length", 2)
            .withParameter("i2c_addr", cfg->i2c_addr)
            .withParameter("user_data", i2c_read_user_data)
            .ignoreOtherParameters();
    }

    uint32_t meas_lx;
    void *complete_cb_user_data_expected = (void *)0x17;
    uint8_t rc = bh1750_read_one_time_measurement(bh1750, cfg->meas_mode, &meas_lx, cfg->complete_cb,
                                                  complete_cb_user_data_expected);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(cfg->i2c_write_rc, i2c_write_complete_cb_user_data);
    if (cfg->i2c_write_rc == BH1750_I2C_RESULT_CODE_OK) {
        timer_expired_cb(timer_expired_cb_user_data);
        i2c_read_complete_cb(cfg->i2c_read_rc, i2c_read_complete_cb_user_data);
    }

    if (cfg->complete_cb) {
        CHECK_EQUAL(1, complete_cb_call_count);
        CHECK_EQUAL(cfg->expected_complete_cb_rc, complete_cb_result_code);
        CHECK_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
    }
    if (cfg->expected_complete_cb_rc == BH1750_RESULT_CODE_OK) {
        CHECK_EQUAL(cfg->expected_meas_lx, meas_lx);
    }
}

TEST(BH1750, ReadOneTimeMeasWriteFail)
{
    /* One-time measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x20;
    /* Garbage data, not used in this test */
    uint8_t i2c_read_data[] = {0xAB, 0xCD};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = BH1750_TEST_DEFAULT_MEAS_TIME,
        .meas_time_i2c_write_data_1 = NULL,
        .meas_time_i2c_write_data_2 = NULL,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_ERR,
        .timer_period = 0,                         /* Don't care */
        .i2c_read_data = i2c_read_data,            /* Don't care */
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_ERR, /* Don't care */
        .expected_meas_lx = 0,                     /* Don't care */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_IO_ERR,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasReadFail)
{
    /* One-time measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x20;
    /* Garbage data, not used in this test because read fails */
    uint8_t i2c_read_data[] = {0xAB, 0xCD};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = BH1750_TEST_DEFAULT_MEAS_TIME,
        .meas_time_i2c_write_data_1 = NULL,
        .meas_time_i2c_write_data_2 = NULL,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 180,            /* Max time it takes to make a measurement in H-resolution mode */
        .i2c_read_data = i2c_read_data, /* Don't care */
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_ERR,
        .expected_meas_lx = 0, /* Don't care */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_IO_ERR,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasHResMode)
{
    /* One-time measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x20;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = BH1750_TEST_DEFAULT_MEAS_TIME,
        .meas_time_i2c_write_data_1 = NULL,
        .meas_time_i2c_write_data_2 = NULL,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 180, /* Max time it takes to make a measurement in H-resolution mode */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 28067, /* (0x8390 / 1.2) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasHResMode2)
{
    /* One-time measurement in H-resolution mode 2 cmd */
    uint8_t i2c_write_data = 0x21;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = BH1750_TEST_DEFAULT_MEAS_TIME,
        .meas_time_i2c_write_data_1 = NULL,
        .meas_time_i2c_write_data_2 = NULL,
        .meas_mode = BH1750_MEAS_MODE_H_RES2,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 180, /* Max time it takes to make a measurement in H-resolution mode */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 14033, /* ((0x8390 / 1.2) / 2) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasLResMode)
{
    /* One-time measurement in L-resolution mode cmd */
    uint8_t i2c_write_data = 0x23;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = BH1750_TEST_DEFAULT_MEAS_TIME,
        .meas_time_i2c_write_data_1 = NULL,
        .meas_time_i2c_write_data_2 = NULL,
        .meas_mode = BH1750_MEAS_MODE_L_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 24, /* Max time it takes to make a measurement in L-resolution mode */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 28067, /* (0x8390 / 1.2) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasCbNull)
{
    /* One-time measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x20;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = BH1750_TEST_DEFAULT_MEAS_TIME,
        .meas_time_i2c_write_data_1 = NULL,
        .meas_time_i2c_write_data_2 = NULL,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 180, /* Max time it takes to make a measurement in H-resolution mode */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 28067, /* (0x8390 / 1.2) */
        .complete_cb = NULL,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK, /* So that meas_lx is checked against expected_meas_lx */
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasAltI2cAddr)
{
    /* One-time measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x20;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_ALT_I2C_ADDR,
        .meas_time = BH1750_TEST_DEFAULT_MEAS_TIME,
        .meas_time_i2c_write_data_1 = NULL,
        .meas_time_i2c_write_data_2 = NULL,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 180, /* Max time it takes to make a measurement in H-resolution mode */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 28067, /* (0x8390 / 1.2) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasSelfNull)
{
    uint32_t meas_lx;
    ReadOneTimeMeasInvalidArgTestCtxt ctxt = {
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .meas_lx_p = &meas_lx,
    };
    test_invalid_arg(NULL, INVALID_ARG_TEST_TYPE_READ_ONE_TIME_MEAS, &ctxt);
}

TEST(BH1750, ReadOneTimeMeasMeasLxNull)
{
    ReadOneTimeMeasInvalidArgTestCtxt ctxt = {
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .meas_lx_p = NULL,
    };
    test_invalid_arg(&bh1750, INVALID_ARG_TEST_TYPE_READ_ONE_TIME_MEAS, &ctxt);
}

TEST(BH1750, ReadOneTimeMeasInvalidMeasMode)
{
    uint32_t meas_lx;
    ReadOneTimeMeasInvalidArgTestCtxt ctxt = {
        .meas_mode = 0xF2, /* Invalid meas mode */
        .meas_lx_p = &meas_lx,
    };
    test_invalid_arg(&bh1750, INVALID_ARG_TEST_TYPE_READ_ONE_TIME_MEAS, &ctxt);
}

TEST(BH1750, ReadOneTimeMeasHResModeMeasTime138)
{
    /* Set three most significant bits of MTreg to 100 */
    uint8_t meas_time_i2c_write_data_1 = 0x44;
    /* Set five least significant bits of MTreg to 01010 */
    uint8_t meas_time_i2c_write_data_2 = 0x6A;
    /* One-time measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x20;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = 138, /* bin: 10001010 */
        .meas_time_i2c_write_data_1 = &meas_time_i2c_write_data_1,
        .meas_time_i2c_write_data_2 = &meas_time_i2c_write_data_2,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 360, /* 180 * 2, because meas time is 2 timer higher than default (69) */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 14033, /* (0x8390 * ((1 / 1.2) * (69 / 138))) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasHResModeMeasTime254)
{
    /* Set three most significant bits of MTreg to 111 */
    uint8_t meas_time_i2c_write_data_1 = 0x47;
    /* Set five least significant bits of MTreg to 11110 */
    uint8_t meas_time_i2c_write_data_2 = 0x7E;
    /* One-time measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x20;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = 254, /* bin: 11111110 */
        .meas_time_i2c_write_data_1 = &meas_time_i2c_write_data_1,
        .meas_time_i2c_write_data_2 = &meas_time_i2c_write_data_2,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 663, /* 180 * (254 / 69) */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 7624, /* (0x8390 * ((1 / 1.2) * (69 / 254))) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasHResModeMeasTime31)
{
    /* Set three most significant bits of MTreg to 000 */
    uint8_t meas_time_i2c_write_data_1 = 0x40;
    /* Set five least significant bits of MTreg to 11111 */
    uint8_t meas_time_i2c_write_data_2 = 0x7F;
    /* One-time measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x20;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = 31, /* bin: 00011111 */
        .meas_time_i2c_write_data_1 = &meas_time_i2c_write_data_1,
        .meas_time_i2c_write_data_2 = &meas_time_i2c_write_data_2,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 81, /* 180 * (31 / 69) */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 62471, /* (0x8390 * ((1 / 1.2) * (69 / 31))) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasHResModeMeasTime32TimerPeriodCeil)
{
    /* This test verifies that we ceil the timer period in the calculation instead of rounding it. The timer period will
     * be ~83.47, and we expect it to be 84 (ceil, not round). */

    /* Set three most significant bits of MTreg to 001 */
    uint8_t meas_time_i2c_write_data_1 = 0x41;
    /* Set five least significant bits of MTreg to 00000 */
    uint8_t meas_time_i2c_write_data_2 = 0x60;
    /* One-time measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x20;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = 32, /* bin: 00100000 */
        .meas_time_i2c_write_data_1 = &meas_time_i2c_write_data_1,
        .meas_time_i2c_write_data_2 = &meas_time_i2c_write_data_2,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 84, /* 180 * (32 / 69) */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 60519, /* (0x8390 * ((1 / 1.2) * (69 / 32))) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasHResModeMeasTime138DiffMeas)
{
    /* Set three most significant bits of MTreg to 100 */
    uint8_t meas_time_i2c_write_data_1 = 0x44;
    /* Set five least significant bits of MTreg to 01010 */
    uint8_t meas_time_i2c_write_data_2 = 0x6A;
    /* One-time measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x20;
    uint8_t i2c_read_data[] = {0x0, 0x30}; /* 48 in decimal. Calculated to get 20 lx as a result. */
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = 138, /* bin: 10001010 */
        .meas_time_i2c_write_data_1 = &meas_time_i2c_write_data_1,
        .meas_time_i2c_write_data_2 = &meas_time_i2c_write_data_2,
        .meas_mode = BH1750_MEAS_MODE_H_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 360, /* 180 * 2, because meas time is 2 timer higher than default (69) */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 20, /* (0x0030 * ((1 / 1.2) * (69 / 138))) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasHRes2ModeMeasTime138)
{
    /* Set three most significant bits of MTreg to 100 */
    uint8_t meas_time_i2c_write_data_1 = 0x44;
    /* Set five least significant bits of MTreg to 01010 */
    uint8_t meas_time_i2c_write_data_2 = 0x6A;
    /* One-time measurement in H-resolution 2 mode cmd */
    uint8_t i2c_write_data = 0x21;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = 138, /* bin: 10001010 */
        .meas_time_i2c_write_data_1 = &meas_time_i2c_write_data_1,
        .meas_time_i2c_write_data_2 = &meas_time_i2c_write_data_2,
        .meas_mode = BH1750_MEAS_MODE_H_RES2,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        .timer_period = 360, /* 180 * 2, because meas time is 2 timer higher than default (69) */
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 7017, /* (0x8390 * ((1 / 1.2) * (69 / 138) / 2)) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, ReadOneTimeMeasLResModeMeasTime138)
{
    /* Set three most significant bits of MTreg to 100 */
    uint8_t meas_time_i2c_write_data_1 = 0x44;
    /* Set five least significant bits of MTreg to 01010 */
    uint8_t meas_time_i2c_write_data_2 = 0x6A;
    /* One-time measurement in H-resolution 2 mode cmd */
    uint8_t i2c_write_data = 0x23;
    /* Example from the datasheet, p. 7 */
    uint8_t i2c_read_data[] = {0x83, 0x90};
    TestReadOneTimeMeasCfg cfg = {
        .i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR,
        .meas_time = 138, /* bin: 10001010 */
        .meas_time_i2c_write_data_1 = &meas_time_i2c_write_data_1,
        .meas_time_i2c_write_data_2 = &meas_time_i2c_write_data_2,
        .meas_mode = BH1750_MEAS_MODE_L_RES,
        .i2c_write_data = &i2c_write_data,
        .i2c_write_rc = BH1750_I2C_RESULT_CODE_OK,
        /* In low res mode, meas time does not affect how long it takes to make a measurement. It is always 24 in low
           res mode. */
        .timer_period = 24,
        .i2c_read_data = i2c_read_data,
        .i2c_read_rc = BH1750_I2C_RESULT_CODE_OK,
        .expected_meas_lx = 28067, /* (0x8390 * (1 / 1.2)) */
        .complete_cb = bh1750_complete_cb,
        .expected_complete_cb_rc = BH1750_RESULT_CODE_OK,
    };
    test_read_one_time_meas(&cfg);
}

TEST(BH1750, CreateSuccessDefaultI2cAddr)
{
    init_cfg.i2c_addr = BH1750_TEST_DEFAULT_I2C_ADDR;
    uint8_t rc = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
}

TEST(BH1750, CreateSuccessAltI2cAddr)
{
    init_cfg.i2c_addr = BH1750_TEST_ALT_I2C_ADDR;
    uint8_t rc = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
}

TEST(BH1750, DestroyNoFreeCb)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();

    uint8_t rc = bh1750_destroy(bh1750, NULL, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
}

TEST(BH1750, DestroyCallsFreeCb)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();

    void *user_data = (void *)0x18;
    mock()
        .expectOneCall("mock_bh1750_free_instance_memory")
        .withParameter("inst_mem", (void *)&instance_memory)
        .withParameter("user_data", user_data);

    uint8_t rc = bh1750_destroy(bh1750, mock_bh1750_free_instance_memory, user_data);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
}

TEST(BH1750, DestroySelfNull)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();

    uint8_t rc = bh1750_destroy(NULL, mock_bh1750_free_instance_memory, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

TEST(BH1750, InitCalledTwice)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();

    uint8_t rc = bh1750_init(bh1750, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

TEST(BH1750, PowerOnBeforeInit)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    uint8_t rc = bh1750_power_on(bh1750, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

TEST(BH1750, PowerDownBeforeInit)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    uint8_t rc = bh1750_power_down(bh1750, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

TEST(BH1750, ResetBeforeInit)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    uint8_t rc = bh1750_reset(bh1750, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

TEST(BH1750, StartContMeasBeforeInit)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    uint8_t rc = bh1750_start_continuous_measurement(bh1750, BH1750_MEAS_MODE_H_RES, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

TEST(BH1750, ReadContMeasBeforeInit)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    uint32_t meas_lx;
    uint8_t rc = bh1750_read_continuous_measurement(bh1750, &meas_lx, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

TEST(BH1750, ReadOneTimeMeasBeforeInit)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    uint32_t meas_lx;
    uint8_t rc = bh1750_read_one_time_measurement(bh1750, BH1750_MEAS_MODE_L_RES, &meas_lx, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

TEST(BH1750, SetMeasTimeBeforeInit)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    uint8_t meas_time = 70;
    uint8_t rc = bh1750_set_measurement_time(bh1750, meas_time, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

TEST(BH1750, DestroyBeforeInitAllowed)
{
    mock()
        .expectOneCall("mock_bh1750_free_instance_memory")
        .withParameter("inst_mem", (void *)&instance_memory)
        .withParameter("user_data", (void *)NULL);

    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    uint8_t rc = bh1750_destroy(bh1750, mock_bh1750_free_instance_memory, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
}

TEST(BH1750, FunctionsCannotBeCalledAfterFailedInit)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    /* Power on command */
    uint8_t i2c_write_data_1 = 0x01;
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &i2c_write_data_1, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", init_cfg.i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    /* Set three most significant bits of MTreg to 010 */
    uint8_t i2c_write_data_2 = 0x42;
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &i2c_write_data_2, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", init_cfg.i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    /* Set five least significant bits of MTreg to 00101 */
    uint8_t i2c_write_data_3 = 0x65;
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &i2c_write_data_3, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", init_cfg.i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    void *complete_cb_user_data_expected = (void *)0x14;
    uint8_t rc_init = bh1750_init(bh1750, bh1750_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_init);
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Last I2C transaction fails */
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_ERR, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(BH1750_RESULT_CODE_IO_ERR, complete_cb_result_code);
    CHECK_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);

    /* Now, setting measurement time should fail with INVALID_USAGE because init did not complete successfully */
    uint8_t meas_time = 71;
    uint8_t rc = bh1750_set_measurement_time(bh1750, meas_time, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

TEST(BH1750, CannotSetMeasTimeWhenContMeasOngoing)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();

    /* Start continuous measurement in H-resolution mode cmd */
    uint8_t i2c_write_data = 0x10;
    /* bh1750_start_continuous_measurement */
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &i2c_write_data, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", init_cfg.i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc_start_meas = bh1750_start_continuous_measurement(bh1750, BH1750_MEAS_MODE_H_RES, NULL, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_start_meas);
    i2c_write_complete_cb(BH1750_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);

    /* Continuous measurement ongoing, setting measurement time is not allowed */
    uint8_t meas_time = 72;
    uint8_t rc = bh1750_set_measurement_time(bh1750, meas_time, bh1750_complete_cb, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_USAGE, rc);
}

typedef uint8_t (*BH1750Function)();

/**
 * @brief Test that when a public BH1750 function is called when another sequence is in progress, BUSY result code is
 * returned.
 *
 * Starts a "power down" sequence and does not execute the I2C write callback. Then, invokes @p function and expects
 * that it will return BUSY result code, because the "power down" sequence is still in progress.
 *
 * @param function Function that should return BUSY result code.
 */
static void test_busy_if_seq_in_progress(BH1750Function function)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);
    call_init();

    /* Power down cmd */
    uint8_t i2c_write_data = 0x0;
    mock()
        .expectOneCall("mock_bh1750_i2c_write")
        .withMemoryBufferParameter("data", &i2c_write_data, 1)
        .withParameter("length", 1)
        .withParameter("i2c_addr", init_cfg.i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc_power_down = bh1750_power_down(bh1750, NULL, NULL);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_power_down);
    /* I2C write callback is not executed yet, so enable power down sequence is still in progres. The driver should
     * reject attempts to start new sequences. */

    uint8_t rc = function();
    CHECK_EQUAL(BH1750_RESULT_CODE_BUSY, rc);
    /* User cb should not be called when busy is returned */
    CHECK_EQUAL(0, complete_cb_call_count);
}

static uint8_t power_on()
{
    return bh1750_power_on(bh1750, bh1750_complete_cb, NULL);
}

TEST(BH1750, PowerOnBusy)
{
    test_busy_if_seq_in_progress(power_on);
}

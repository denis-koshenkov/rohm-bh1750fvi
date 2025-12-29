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

        /* Pass pointers so that the mock object populates them with callbacks and user data, so that the test can simulate
        calling these callbacks. */
        mock().setData("i2cWriteCompleteCb", (void *)&i2c_write_complete_cb);
        mock().setData("i2cWriteCompleteCbUserData", &i2c_write_complete_cb_user_data);

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

static void test_send_cmd_func(SendCmdFunc send_cmd_func, uint8_t i2c_addr, BH1750CompleteCb complete_cb,
                               uint8_t *i2c_write_data, uint8_t i2c_write_rc, uint8_t expected_complete_cb_rc)
{
    if (!send_cmd_func) {
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
    uint8_t rc = send_cmd_func(bh1750, complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    if (complete_cb) {
        CHECK_EQUAL(1, complete_cb_call_count);
        CHECK_EQUAL(expected_complete_cb_rc, complete_cb_result_code);
        CHECK_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
    }
}

TEST(BH1750, PowerOnWriteFail)
{
    /* Power on command */
    uint8_t i2c_write_data = 0x1;
    test_send_cmd_func(bh1750_power_on, BH1750_TEST_DEFAULT_I2C_ADDR, bh1750_complete_cb, &i2c_write_data,
                       BH1750_I2C_RESULT_CODE_ERR, BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, PowerOnWriteSuccess)
{
    /* Power on command */
    uint8_t i2c_write_data = 0x1;
    test_send_cmd_func(bh1750_power_on, BH1750_TEST_DEFAULT_I2C_ADDR, bh1750_complete_cb, &i2c_write_data,
                       BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerOnCbNull)
{
    /* Power on command */
    uint8_t i2c_write_data = 0x1;
    test_send_cmd_func(bh1750_power_on, BH1750_TEST_DEFAULT_I2C_ADDR, NULL, &i2c_write_data, BH1750_I2C_RESULT_CODE_OK,
                       BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerOnSelfNull)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    void *complete_cb_user_data_expected = (void *)0x11;
    uint8_t rc = bh1750_power_on(NULL, bh1750_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

TEST(BH1750, PowerOnWriteSuccessAltI2cAddr)
{
    /* Power on command */
    uint8_t i2c_write_data = 0x1;
    test_send_cmd_func(bh1750_power_on, BH1750_TEST_ALT_I2C_ADDR, bh1750_complete_cb, &i2c_write_data,
                       BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerDownWriteFail)
{
    /* Power down command */
    uint8_t i2c_write_data = 0x0;
    test_send_cmd_func(bh1750_power_down, BH1750_TEST_DEFAULT_I2C_ADDR, bh1750_complete_cb, &i2c_write_data,
                       BH1750_I2C_RESULT_CODE_ERR, BH1750_RESULT_CODE_IO_ERR);
}

TEST(BH1750, PowerDownWriteSuccess)
{
    /* Power down command */
    uint8_t i2c_write_data = 0x0;
    test_send_cmd_func(bh1750_power_down, BH1750_TEST_DEFAULT_I2C_ADDR, bh1750_complete_cb, &i2c_write_data,
                       BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerDownCbNullSuccess)
{
    /* Power down command */
    uint8_t i2c_write_data = 0x0;
    test_send_cmd_func(bh1750_power_down, BH1750_TEST_DEFAULT_I2C_ADDR, NULL, &i2c_write_data,
                       BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerDownAltI2cAddr)
{
    /* Power down command */
    uint8_t i2c_write_data = 0x0;
    test_send_cmd_func(bh1750_power_down, BH1750_TEST_ALT_I2C_ADDR, bh1750_complete_cb, &i2c_write_data,
                       BH1750_I2C_RESULT_CODE_OK, BH1750_RESULT_CODE_OK);
}

TEST(BH1750, PowerDownSelfNull)
{
    uint8_t rc_create = bh1750_create(&bh1750, &init_cfg);
    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc_create);

    void *complete_cb_user_data_expected = (void *)0x12;
    uint8_t rc = bh1750_power_down(NULL, bh1750_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

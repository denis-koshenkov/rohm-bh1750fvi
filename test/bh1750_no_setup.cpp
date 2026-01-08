#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "bh1750.h"
/* Included to know the size of BH1750 instance we need to define to return from mock_bh1750_get_instance_memory. */
#include "bh1750_private.h"
#include "mock_cfg_functions.h"

#define BH1750_TEST_DEFAULT_I2C_ADDR 0x23

/* To return from mock_bh1750_get_instance_memory */
static struct BH1750Struct instance_memory;

/* User data parameters to pass to bh1750_create in the init cfg */
static void *get_instance_memory_user_data = (void *)0xDE;
static void *i2c_write_user_data = (void *)0x78;
static void *i2c_read_user_data = (void *)0x9A;
static void *start_timer_user_data = (void *)0xBC;

/* These tests are in a separate test group, because they test bh1750_create, including unhappy scenarios. In the
 * other test group, expected call to mock_bh1750_get_instance_memory is set in the common setup function before each
 * test. mock_bh1750_get_instance_memory only gets called from bh1750_create in the happy scenario. In order to test
 * unhappy scenarios, this test group is created, and there are no expected mock calls in the setup function of this
 * test group.
 */

// clang-format off
TEST_GROUP(BH1750NoSetup)
{
    void setup() {
        /* Order of expected calls is important for these tests. Fail the test if the expected mock calls do not happen
        in the specified order. */
        mock().strictOrder();
    }
};
// clang-format on

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

TEST(BH1750NoSetup, CreateReturnsBufReturnedFromGetInstanceMemory)
{
    mock()
        .expectOneCall("mock_bh1750_get_instance_memory")
        .withParameter("user_data", get_instance_memory_user_data)
        .andReturnValue((void *)&instance_memory);

    BH1750 bh1750;
    BH1750InitConfig cfg;
    populate_default_init_cfg(&cfg);
    uint8_t rc = bh1750_create(&bh1750, &cfg);

    CHECK_EQUAL(BH1750_RESULT_CODE_OK, rc);
    CHECK_EQUAL((void *)&instance_memory, (void *)bh1750);
}

TEST(BH1750NoSetup, CreateReturnsOutOfMemIfGetInstanceMemoryReturnsNull)
{
    mock()
        .expectOneCall("mock_bh1750_get_instance_memory")
        .withParameter("user_data", get_instance_memory_user_data)
        .andReturnValue((void *)NULL);

    BH1750 bh1750;
    BH1750InitConfig cfg;
    populate_default_init_cfg(&cfg);
    uint8_t rc = bh1750_create(&bh1750, &cfg);

    CHECK_EQUAL(BH1750_RESULT_CODE_OUT_OF_MEMORY, rc);
}

TEST(BH1750NoSetup, CreateReturnsInvalidArgInstNull)
{
    BH1750InitConfig cfg;
    populate_default_init_cfg(&cfg);
    uint8_t rc = bh1750_create(NULL, &cfg);

    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

TEST(BH1750NoSetup, CreateReturnsInvalidArgCfgNull)
{
    BH1750 bh1750;
    uint8_t rc = bh1750_create(&bh1750, NULL);

    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

TEST(BH1750NoSetup, CreateReturnsInvalidArgGetMemoryInstanceNull)
{
    BH1750 bh1750;
    BH1750InitConfig cfg;
    populate_default_init_cfg(&cfg);
    cfg.get_instance_memory = NULL;
    uint8_t rc = bh1750_create(&bh1750, &cfg);

    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

TEST(BH1750NoSetup, CreateReturnsInvalidArgI2cWriteNull)
{
    BH1750 bh1750;
    BH1750InitConfig cfg;
    populate_default_init_cfg(&cfg);
    cfg.i2c_write = NULL;
    uint8_t rc = bh1750_create(&bh1750, &cfg);

    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

TEST(BH1750NoSetup, CreateReturnsInvalidArgI2cReadNull)
{
    BH1750 bh1750;
    BH1750InitConfig cfg;
    populate_default_init_cfg(&cfg);
    cfg.i2c_read = NULL;
    uint8_t rc = bh1750_create(&bh1750, &cfg);

    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

TEST(BH1750NoSetup, CreateReturnsInvalidArgStartTimerNull)
{
    BH1750 bh1750;
    BH1750InitConfig cfg;
    populate_default_init_cfg(&cfg);
    cfg.start_timer = NULL;
    uint8_t rc = bh1750_create(&bh1750, &cfg);

    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

TEST(BH1750NoSetup, CreateReturnsInvalidArgInvalidI2cAddr)
{
    BH1750 bh1750;
    BH1750InitConfig cfg;
    populate_default_init_cfg(&cfg);
    cfg.i2c_addr = 0xFF; /* Invalid I2C address */
    uint8_t rc = bh1750_create(&bh1750, &cfg);

    CHECK_EQUAL(BH1750_RESULT_CODE_INVALID_ARG, rc);
}

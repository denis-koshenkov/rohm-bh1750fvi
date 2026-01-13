# Rohm BH1750FVI Driver
Non-blocking driver for Rohm BH1750FVI light intensity sensor in C language. It is intended to be used in asynchronous embedded systems.

This driver is implemented in a way that the driver user does not need to block at all, given that I2C transactions can be performed asynchronously.

This driver was developed with Test-Driven Development (TDD) using CppUTest framework.

# Integration Details
Add the following to your build:
- `src/bh1750.c` source file
- `src` directory as include directory

# Usage
In order to use the driver, you need to implement the folllowing functions:
```c
void *bh1750_get_instance_memory(void *user_data);
void bh1750_i2c_write(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data, BH1750_I2CCompleteCb cb, void *cb_user_data);
void bh1750_i2c_read(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data, BH1750_I2CCompleteCb cb, void *cb_user_data);
void bh1750_start_timer(uint32_t duration_ms, void *user_data, BH1750TimerExpiredCb cb, void *cb_user_data);
```
The function names can be different - you will pass function pointers of your implementations to the driver.

## Function Implementation Guidelines

### Get instance memory
`bh1750_get_instance_memory` must return a memory buffer that will be used for private data of a BH1750 instance. The memory buffer must remain valid as long as the instance is being used.

This function is called once per instance as a part of `bh1750_create`.

There are two main ways to implement this function:
1) Return pointer to statically allocated memory. If you need one driver instance:
```c
void *bh1750_get_instance_memory(void *user_data) {
    static struct BH1750Struct inst;
    return &inst;
}
```
If you need several driver instances:
```c
#define BH1750_NUM_INSTANCES 2

void *get_instance_memory(void *user_data) {
    static struct BH1750Struct instances[BH1750_NUM_INSTANCES];
    static size_t idx = 0;
    return (idx < BH1750_NUM_INSTANCES) ? (&(instances[idx++])) : NULL;
}
```

2) Dynamically allocate memory for an instance:
```c
void *bh1750_get_instance_memory(void *user_data) {
    return malloc(sizeof(struct BH1750Struct));
}
```

If you are using option 2, then you need to free the memory when the driver instance is destroyed. See [this section](#destroying-a-bh1750-instance) for how to do this.

`struct BH1750Struct` is a data type that defines the private data of a BH1750 instance. It is defined in the `bh1750_private.h` file.

That header must not be included by driver users. The only exception is a module that implements `bh1750_get_instance_memory`, because the implementation of that function needs to know about `struct BH1750Struct`, so that it knows the size of the memory buffer it needs to return.

It is recommended to define the implementation of `bh1750_get_instance_memory` in a separate .c file and do `#include "bh1750_private.h"` only in that .c file.

This way, other modules that interact with this driver do not have to include `bh1750_private.h`, which means they cannot access private data of the driver instance directly.

### I2C Read/Write
`bh1750_i2c_write` must write `length` bytes of `data` to I2C device with address `i2c_addr`. `user_data` parameter will be equal to the `i2c_write_user_data` pointer from the init config passed to `bh1750_create`.

`bh1750_i2c_read` must read `length` bytes from I2C device with address `i2c_addr`, and write the resulting bytes to `data` pointer. `user_data` parameter will be equal to the `i2c_read_user_data` pointer from the init config passed to `bh1750_create`.

The implementations must be asynchronous. Once the I2C transaction is complete, the implementations must invoke `cb`.

`cb` has two parameters: `result_code` and `user_data`.

When invoking `cb`, the implementation must pass `cb_user_data` from `bh1750_i2c_write/read` to the `user_data` parameter of `cb`.

`result_code` parameter of `cb` signifies whether I2C transaction was successful. The implementation must pass `BH1750_I2C_RESULT_CODE_OK` if the transaction was successful. Pass `BH1750_I2C_RESULT_CODE_ERR` if the transaction failed.

**Important rule**: `cb` must be invoked from the same thread/context as all other public driver functions of this driver. See [this section](#i2c-complete-and-timer-expired-callbacks-execution-context-rule) for more details.

### Start Timer
`bh1750_start_timer` must start a one-shot timer that invokes `cb` after at least `duration_ms` pass. `user_data` parameter of `bh1750_start_timer` will be equal to the `start_timer_user_data` pointer from the init config passed to `bh1750_create`.

`cb` has one parameter: `user_data`. The implementation must pass the `cb_user_data` parameter of `bh1750_start_timer` to as `user_data` parameter of `cb`.

**Important rule**: `cb` must be invoked from the same thread/context as all other public driver functions of this driver. See [this section](#i2c-complete-and-timer-expired-callbacks-execution-context-rule) for more details.

### I2C Complete and Timer Expired Callbacks Execution Context Rule
`bh1750_i2c_write`, `bh1750_i2c_read`, and `bh1750_start_timer` are asynchronous functions that need to execute a callback once the I2C transaction is complete or the timer expired.

**In order to prevent race conditions, these callbacks must be executed from the same context as other BH1750 public driver functions.**

Imagine that you are using the I2C asynchronous driver from the MCU vendor to implement `bh1750_i2c_write` and `bh1750_i2c_read`. The driver executes a user-defined callback once the I2C transaction is complete.

It is often the case that this user-defined callback is executed from ISR context. In that case, the implementation of `bh1750_i2c_write`/`bh1750_i2c_read` **must not directly invoke `cb`** from that user-defined callback.

This would result in `cb` being called from ISR context and may cause race conditions in the BH1750 driver.

It is the responsibility of the driver user to ensure that all public driver functions and all callbacks are executed from the same context.

An example implementation is to have a central event queue - a thread that is blocked on a message queue. Whehenver there is an event that needs to be handled in the system, an event is pushed to that message queue.

The thread then processes the event by calling an event handler for that event.

Every time a I2C transaction is complete or a timer expires, an event can be pushed to the central event queue. The thread will then invoke the handlers for these events - which will execute the `BH1750_I2CCompleteCb` and `BH1750TimerExpiredCb` callbacks.

This way, these callbacks are always invoked from the same execution context. Of course, all public BH1750 driver functions also need to be executed from the same event queue thread for this to work.

This is just an example - any implementation works as long as **all public BH1750 driver functions and callbacks are executed from the same context**.

## Driver Usage Example
This example creates and initializes a BH1750 driver instance, reads out a one-time measurement, starts a continuous measurement, and periodically reads out the results of the continuous measurement.

There is a lot of blocking in this example - this is probably not the way to go in an asynchronous system. The example is done this way so that it is easy to demonstrate driver functions usage.
```c
static bool is_init_complete = false;
static void init_complete_cb(uint8_t result_code, void *user_data) {
    if (result_code == BH1750_RESULT_CODE_OK) {
        is_init_complete = true;
    }
}

static bool one_time_meas_complete = false;
static void one_time_meas_complete_cb(uint8_t result_code, void *user_data) {
    if (result_code == BH1750_RESULT_CODE_OK) {
        one_time_meas_complete = true;
    }
}

static bool start_complete = false;
static void start_cont_meas_complete_cb(uint8_t result_code, void *user_data) {
    if (result_code == BH1750_RESULT_CODE_OK) {
        start_complete = true;
    }
}

static bool read_complete = false;
static void read_cont_meas_complete_cb(uint8_t result_code, void *user_data) {
    if (result_code == BH1750_RESULT_CODE_OK) {
        read_complete = true;
    }
}

BH1750InitConfig init_cfg = {
    .get_instance_memory = bh1750_get_instance_memory,
    .get_instance_memory_user_data = NULL, // Optional
    .i2c_write = bh1750_i2c_write,
    .i2c_write_user_data = NULL, // Optional
    .i2c_read = bh1750_i2c_read,
    .i2c_read_user_data = NULL, // Optional
    .start_timer = bh1750_start_timer,
    .start_timer_user_data = NULL, // Optional
    .i2c_addr = 0x23, // or 0x5C - depending on whether ADDR pin is high or low
};
BH1750 inst;
/* Creates an instance, does not interact with the sensor via I2C */
uint8_t rc_create = bh1750_create(&inst, &init_cfg);
if (rc_create != BH1750_RESULT_CODE_OK) {
    // Error handling
}

/* Sends power on command and sets measurement time in Mtreg to default (69) */
uint8_t rc_init = bh1750_init(inst, init_complete_cb, NULL);
if (rc_init != BH1750_RESULT_CODE_OK) {
    // Error handling
}
/* Busy wait until init is complete. This is not the way to go in an async system. This is an example to showcase that init_complete_cb must execute before anything else can be done. */
while (!is_init_complete) {
    ;
}

/* Perform a one-time measurement in high resolution mode */
uint32_t meas_lx_one_time;
uint8_t rc_one_time_meas = bh1750_read_one_time_measurement(inst, BH1750_MEAS_MODE_H_RES, &meas_lx_one_time, one_time_meas_complete_cb, NULL);
if (rc_one_time_meas != BH1750_RESULT_CODE_OK) {
    // Error handling
}
while (!one_time_meas_complete) {
    ;
}
/* The measurement in lx is now available in the meas_lx_one_time variable */

/* Start continuous measurement in high resolution mode and read it out periodically */
uint8_t rc_start = bh1750_start_continuous_measurement(inst, BH1750_MEAS_MODE_H_RES, start_cont_meas_complete_cb, NULL);
if (rc_start != BH1750_RESULT_CODE_OK) {
    // Error handling
}
while (!start_complete) {
    ;
}

/* Continuous measurement is now started. Read out the measurement periodically. */
uint8_t rc_read;
uint32_t meas_lx;
while (1) {
    read_complete = false;
    rc_read = bh1750_read_continuous_measurement(inst, &meas_lx, read_cont_meas_complete_cb, NULL);
    if (rc_read != BH1750_RESULT_CODE_OK) {
        // Error handling
    }

    /* Wait until measurement is read */
    while (!read_complete) {
        ;
    }
    /* Measurment in lx is now available in meas_lx variable */
    
    /* We are using high resolution mode and default measurement time. This means that the measurement will be updated every ~120 ms. It can take up to 180 ms to update the measurement, so there is a chance that we will read the same measurement twice. This may or may not be ok depending on your application. */
    sleep_ms(120);
}

```

## Destroying a BH1750 instance
If the BH1750 driver instance is not needed for the whole duration of the program, it can be destroyed by calling `bh1750_destroy`.

`bh1750_destroy` takes a `BH1750FreeInstanceMemory` function pointer as a parameter. This is an optional user-defined function that can be used to free the memory allocated in `bh1750_get_instance_memory`.

Imagine that your implementation of `bh1750_get_instance_memory` looked like this:
```c
void *bh1750_get_instance_memory(void *user_data) {
    return malloc(sizeof(struct BH1750Struct));
}
```

If at any point in the program the instance needs to be destroyed, `free` needs to be called for the instance memory.

`BH1750FreeInstanceMemory` allows to do exactly that. It is called as a part of `bh1750_destroy` with the `inst_mem` parameter equal to the pointer that was earlier returned from `bh1750_get_instance_memory`.

This way, the implementation of `BH1750FreeInstanceMemory` can free the instance memory. Continuing the example above:
```c
void bh1750_free_instance_memory(void *inst_mem, void *user_data) {
    return free(inst_mem);
}
```

Then, you would invoke `bh1750_destroy` and pass `bh1750_free_instance_memory` as a parameter:
```c
uint8_t rc = bh1750_destroy(inst, bh1750_free_instance_memory, NULL);
if (rc != BH1750_RESULT_CODE_OK) {
    // Error handling
}
```

# Running Tests
Follow these steps in order to run all unit tests for the driver source code.

Prerequisites:
- CMake
- C compiler (e.g. gcc)
- C++ compiler (e.g. g++)

Steps:
1. Clone this repository
2. Fetch the CppUTest submodule:
```
git submodule init
git submodule update
```
3. Build and run the tests:
```
./run_tests.sh
```

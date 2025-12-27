#ifndef TEST_MOCK_MOCK_CFG_FUNCTIONS_H
#define TEST_MOCK_MOCK_CFG_FUNCTIONS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "bh1750.h"

void *mock_bh1750_get_instance_memory(void *user_data);

void mock_bh1750_free_instance_memory(void *instance_memory, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* TEST_MOCK_MOCK_CFG_FUNCTIONS_H */

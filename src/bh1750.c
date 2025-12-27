#include <stdint.h>
#include <stdbool.h>

#include "bh1750.h"
#include "bh1750_private.h"

/**
 * @brief Check whether init config is valid.
 *
 * @param[in] cfg Init config.
 *
 * @retval true Init config is valid.
 * @retval false Init config is invalid.
 */
static bool is_valid_init_cfg(const BH1750InitConfig *const cfg)
{
    return (cfg->get_instance_memory);
}

uint8_t bh1750_create(BH1750 *const inst, const BH1750InitConfig *const cfg)
{
    if (!inst || !is_valid_init_cfg(cfg)) {
        return BH1750_RESULT_CODE_INVALID_ARG;
    }

    *inst = cfg->get_instance_memory(cfg->get_instance_memory_user_data);
    if (!(*inst)) {
        return BH1750_RESULT_CODE_OUT_OF_MEMORY;
    }

    return BH1750_RESULT_CODE_OK;
}

#ifndef SRC_BH1750_H
#define SRC_BH1750_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct BH1750truct *BH1750;

typedef enum {
    BH1750_RESULT_CODE_OK = 0,
    BH1750_RESULT_CODE_INVALID_ARG,
} BH1750ResultCode;

typedef struct {
} BH1750InitConfig;

uint8_t bh1750_create(BH1750 *const instance, const BH1750InitConfig *const cfg);

#ifdef __cplusplus
}
#endif

#endif /* SRC_BH1750_H */

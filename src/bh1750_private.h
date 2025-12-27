#ifndef SRC_BH1750_PRIVATE_H
#define SRC_BH1750_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

/* This header should be included only by the user module implementing the BH1750GetInstanceMemory callback which is a
 * part of InitConfig passed to bh1750_create. All other user modules are not allowed to include this header, because
 * otherwise they would know about the BH1750Struct struct definition and can manipulate private data of a BH1750
 * instance directly. */

/* Defined in a separate header, so that both bh1750.c and the user module implementing BH1750GetInstanceMemory callback
 * can include this header. The user module needs to know sizeof(BH1750Struct), so that it knows the size of BH1750
 * instances at compile time. This way, it has an option to allocate a static array with size equal to the required
 * number of instances. */
struct BH1750Struct {};

#ifdef __cplusplus
}
#endif

#endif /* SRC_BH1750_PRIVATE_H */

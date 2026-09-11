/* extract.h —— th06nc 抽取器：把游戏内存读成 ap_world_t。只读，不写任何游戏内存。
 * base = 实际 imagebase（GetModuleHandleW(NULL)）；测试时可以是一块伪造镜像。 */
#ifndef AP_EXTRACT_H
#define AP_EXTRACT_H
#include <stdint.h>
#include "world.h"
void th06nc_extract(const uint8_t *base, ap_world_t *w);
#endif

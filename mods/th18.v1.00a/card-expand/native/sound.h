/* sound.h —— 音效表扩容的 DLL 侧接口（sound.c）。 */
#pragma once

/* 在 SoundManager::init 入口（BP_ce_snd_gate）调一次。
 * 返回 1 = 新音效 id 可用；0 = 已把新行退回骨架，原生行为不变。 */
int ce_sound_init(void);

/* 已登记的语音条数（自检与日志用）。 */
int ce_sound_voice_count(void);

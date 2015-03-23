/* pimp_gba.c -- GameBoy Advance c-interface for Pimpmobile
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include <gba_base.h>

#define PIMP_TYPES_H /* prevent pimp_types.h from being included */
#include "pimp_render.h"
#include "pimp_debug.h"

#include <gba_dma.h>
#include <gba_sound.h>
#include <gba_timers.h>
#include <gba_systemcalls.h>

#include <stdio.h>

pimp_mixer       __pimp_mixer IWRAM_DATA;
pimp_mod_context __pimp_ctx   EWRAM_DATA;

/* setup some constants */
#define CYCLES_PR_FRAME 280896
#define SAMPLES_PR_FRAME  ((int)((1 << 24) / ((float)SAMPLERATE)))
#define SOUND_BUFFER_SIZE ((int)((float)CYCLES_PR_FRAME / SAMPLES_PR_FRAME))

static s8  __pimp_sound_buffers[2][SOUND_BUFFER_SIZE] IWRAM_DATA;
static u32 __pimp_sound_buffer_index = 0;
s32        __pimp_mix_buffer[SOUND_BUFFER_SIZE] IWRAM_DATA;

void pimp_init(const void *module, const void *sample_bank)
{
	__pimp_mixer.mix_buffer = __pimp_mix_buffer;
	__pimp_mod_context_init(&__pimp_ctx, (const pimp_module*)module, (const u8*)sample_bank, &__pimp_mixer);

	u32 zero = 0;
	CpuFastSet(&zero, &__pimp_sound_buffers[0][0], DMA_SRC_FIXED | ((SOUND_BUFFER_SIZE / 4) * 2));
	REG_SOUNDCNT_H = SNDA_VOL_100 | SNDA_L_ENABLE | SNDA_R_ENABLE | SNDA_RESET_FIFO;
	REG_SOUNDCNT_X = (1 << 7);
	
	DEBUG_PRINT(DEBUG_LEVEL_INFO, ("samples pr frame: 0x%x\nsound buffer size: %d\n", SAMPLES_PR_FRAME, SOUND_BUFFER_SIZE));

	/* setup timer */
	REG_TM0CNT_L = (1 << 16) - SAMPLES_PR_FRAME;
	REG_TM0CNT_H = TIMER_START;
}

void pimp_close()
{
	REG_SOUNDCNT_X = 0;
}

void pimp_vblank()
{
	if (__pimp_sound_buffer_index == 0)
	{
		REG_DMA1CNT = 0;
		REG_DMA1SAD = (u32) &(__pimp_sound_buffers[0][0]);
		REG_DMA1DAD = (u32) &REG_FIFO_A;
		REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA32 | DMA_SPECIAL | DMA_ENABLE;
	}
	__pimp_sound_buffer_index ^= 1;
}

void pimp_set_callback(pimp_callback in_callback)
{
	__pimp_ctx.callback = in_callback;
}

void pimp_set_pos(int row, int order)
{
	__pimp_mod_context_set_pos(&__pimp_ctx, row, order);
}

int pimp_get_row()
{
	return __pimp_mod_context_get_row(&__pimp_ctx);
}

int pimp_get_order()
{
	return __pimp_mod_context_get_order(&__pimp_ctx);
}

void pimp_frame()
{
	static volatile BOOL locked = FALSE;
	if (TRUE == locked) return; /* whops, we're in the middle of filling. sorry. you did something wrong! */
	locked = TRUE;
	
	__pimp_render(&__pimp_ctx, __pimp_sound_buffers[__pimp_sound_buffer_index], SOUND_BUFFER_SIZE);
	
	locked = FALSE;
}

#ifndef DEBUG
void __pimp_mixer_clear(s32 *target, u32 samples)
{
	int i;
	const u32 zero = 0;
	u32 *dst = (u32*)target;
	
	ASSERT(NULL != dst);
	
	for (i = samples &7; i; --i)
	{
		*dst++ = 0;
	}
	
	CpuFastSet(&zero, dst, DMA_SRC_FIXED | (samples & ~7));
}
#endif

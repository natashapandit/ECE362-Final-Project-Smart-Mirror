#include "hstx/hstx.h"
#include "alarm/alarm.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// 640x480 physical buffer — placed in RAM
// 307,200 bytes — check RAM usage after build
static uint8_t __attribute__((aligned(4))) physicalbuf[MODE_V_ACTIVE_LINES][MODE_H_ACTIVE_PIXELS];

// HSTX command lists
static uint32_t vblank_line_vsync_off[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH, SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,  SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS), SYNC_V1_H1,
    HSTX_CMD_NOP
};

static uint32_t vblank_line_vsync_on[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH, SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,  SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS), SYNC_V0_H1,
    HSTX_CMD_NOP
};

static uint32_t vactive_line[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH, SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH, SYNC_V1_H0,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_BACK_PORCH, SYNC_V1_H1,
    HSTX_CMD_TMDS | MODE_H_ACTIVE_PIXELS
};

static bool dma_pong              = false;
static uint v_scanline            = 2;
static bool vactive_cmdlist_posted = false;

void __scratch_x("") dma_irq_handler() {
    uint ch_num = dma_pong ? DMACH_PONG : DMACH_PING;
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    dma_pong = !dma_pong;

    if (v_scanline >= MODE_V_FRONT_PORCH &&
        v_scanline < (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH)) {
        ch->read_addr      = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);
    } else if (v_scanline < MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH) {
        ch->read_addr      = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);
    } else if (!vactive_cmdlist_posted) {
        ch->read_addr      = (uintptr_t)vactive_line;
        ch->transfer_count = count_of(vactive_line);
        vactive_cmdlist_posted = true;
    } else {
        // point directly at the pre-built physical row — no computation
        uint active_line = v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES);
        ch->read_addr      = (uintptr_t)physicalbuf[active_line];
        ch->transfer_count = MODE_H_ACTIVE_PIXELS / sizeof(uint32_t);
        vactive_cmdlist_posted = false;
    }

    if (!vactive_cmdlist_posted)
        v_scanline = (v_scanline + 1) % MODE_V_TOTAL_LINES;
}

// font and drawing code unchanged
static const uint8_t font8x16[11][16] = {
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C},
    {0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E},
    {0x3C,0x66,0x66,0x06,0x06,0x0C,0x18,0x30,0x60,0x60,0x60,0x60,0x60,0x66,0x66,0x7E},
    {0x3C,0x66,0x66,0x06,0x06,0x06,0x1C,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x66,0x3C},
    {0x0C,0x1C,0x3C,0x6C,0x6C,0x6C,0x6C,0x7E,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C},
    {0x7E,0x60,0x60,0x60,0x60,0x60,0x7C,0x66,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C},
    {0x1C,0x30,0x60,0x60,0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C},
    {0x7E,0x66,0x66,0x06,0x06,0x0C,0x0C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C},
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3E,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C},
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
};

// draw into physicalbuf directly, doubling pixels
static void draw_char(int c, int x, int y, int scale) {
    if (c < 0 || c > 10) return;
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            uint8_t pixel = (font8x16[c][row] >> (7 - col)) & 1;
            uint8_t color = pixel ? COLOR_WHITE : COLOR_BLACK;
            for (int sy = 0; sy < scale * 2; sy++) {
                for (int sx = 0; sx < scale * 2; sx++) {
                    int px = x * 2 + col * scale * 2 + sx;
                    int py = y * 2 + row * scale * 2 + sy;
                    if (px < MODE_H_ACTIVE_PIXELS && py < MODE_V_ACTIVE_LINES)
                        physicalbuf[py][px] = color;
                }
            }
        }
    }
}

static void draw_time(int hour, int min, int sec) {
    int scale  = 3;
    int char_w = 8 * scale;
    int total_w = 8 * char_w * 2;
    int x = (FB_WIDTH  - total_w / 2) / 2;
    int y = (FB_HEIGHT - 16 * scale) / 2;

    // only clear text region instead of full 307KB memset
    int clear_y_start = y * 2;
    int clear_y_end   = (y + 16 * scale) * 2;
    for (int row = clear_y_start; row < clear_y_end && row < MODE_V_ACTIVE_LINES; row++) {
        memset(physicalbuf[row], COLOR_BLACK, MODE_H_ACTIVE_PIXELS);
    }

    int digits[8] = {
        hour / 10, hour % 10,
        10,
        min / 10,  min % 10,
        10,
        sec / 10,  sec % 10
    };

    for (int i = 0; i < 8; i++)
        draw_char(digits[i], x + i * char_w, y, scale);
}

void display_update(void) {
    int h, m, s;
    clock_get(&h, &m, &s);
    draw_time(h, m, s);
}

void display_init(void) {
    memset(physicalbuf, COLOR_BLACK, sizeof(physicalbuf));

    hstx_ctrl_hw->expand_tmds =
        2 << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | 0 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB |
        2 << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | 29 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB |
        1 << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | 26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

    hstx_ctrl_hw->expand_shift =
        4 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB | 8 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB | 0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS | 5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB | 2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    hstx_ctrl_hw->bit[2] = HSTX_CTRL_BIT0_CLK_BITS;
    hstx_ctrl_hw->bit[3] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;

    for (uint lane = 0; lane < 3; ++lane) {
        static const int lane_to_output_bit[3] = {0, 6, 4};
        int bit = lane_to_output_bit[lane];
        uint32_t lane_data_sel_bits =
            (lane * 10) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        hstx_ctrl_hw->bit[bit]     = lane_data_sel_bits;
        hstx_ctrl_hw->bit[bit + 1] = lane_data_sel_bits | HSTX_CTRL_BIT0_INV_BITS;
    }

    for (int i = 12; i <= 19; ++i)
        gpio_set_function(i, 0);

    dma_channel_config c = dma_channel_get_default_config(DMACH_PING);
    channel_config_set_chain_to(&c, DMACH_PONG);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(DMACH_PING, &c, &hstx_fifo_hw->fifo,
        vblank_line_vsync_off, count_of(vblank_line_vsync_off), false);

    c = dma_channel_get_default_config(DMACH_PONG);
    channel_config_set_chain_to(&c, DMACH_PING);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(DMACH_PONG, &c, &hstx_fifo_hw->fifo,
        vblank_line_vsync_off, count_of(vblank_line_vsync_off), false);

    dma_hw->ints0 = (1u << DMACH_PING) | (1u << DMACH_PONG);
    dma_hw->inte0 = (1u << DMACH_PING) | (1u << DMACH_PONG);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    bus_ctrl_hw->priority =
        BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    dma_channel_start(DMACH_PING);
    printf("DISPLAY: init done\n");
}
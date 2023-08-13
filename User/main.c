/*

 VGA Output test
 32*24 chars = 256*192 pixels
 You must use HSE 144MHz clock.

     PA4: V-Sync
     PA5: B
     PA6: R
     PA7: G
    PA11: H-Sync

 */

#include "debug.h"
#include "msxfont.h"
#include <stdlib.h>
#include <string.h>

#define VGA_COUNT 4535 // = 31.75 us / 144MHz
#define VGA_HSYNC 570  // =  4.0  us / 144MHz
#define VGA_SCAN_DELAY 100 // Delay for video signal generation
#define VGA_SCAN_START 60 // Display start line

#define VGA_X_PIXELS 256
#define VGA_Y_PIXELS 384

#define VGA_X_CHARS (VGA_X_PIXELS/8)
#define VGA_Y_CHARS (VGA_Y_PIXELS/16)

volatile uint16_t vga_line;
volatile uint8_t vga_blank = 0;

// GPIO BHSR set data

const uint32_t vga_color[] = { 0x00e00000 , 0x00c00020 , 0x00a00040 , 0x00800060,
                      0x00600080 , 0x004000a0 , 0x002000c0 , 0x000000e0};

uint8_t *vram;
uint8_t *cvram;
uint32_t *scandata[2];

uint8_t cursor_x, cursor_y = 0;
uint8_t fbcolor=0x70;

void video_init() {

    TIM_OCInitTypeDef TIM_OCInitStructure = { 0 };
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = { 0 };
    GPIO_InitTypeDef GPIO_InitStructure = { 0 };
    NVIC_InitTypeDef NVIC_InitStructure = { 0 };

    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA |  RCC_APB2Periph_TIM1 , ENABLE);
    RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM2 , ENABLE);

    // PA11: H-Sync

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);

    // PA4: V-Sync

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);

    // PA5-7

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);

    // PA0: debug

//    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
//    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//    GPIO_Init( GPIOA, &GPIO_InitStructure);

    // Initalize TIM1

    TIM_TimeBaseInitStructure.TIM_Period = VGA_COUNT;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit( TIM1, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = VGA_HSYNC;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init( TIM1, &TIM_OCInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = VGA_HSYNC + VGA_SCAN_DELAY; // 9.4usec - delay
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init( TIM1, &TIM_OCInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 01;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init( TIM1, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_OC2PreloadConfig( TIM1, TIM_OCPreload_Disable);
    TIM_OC3PreloadConfig( TIM1, TIM_OCPreload_Disable);
    TIM_OC4PreloadConfig( TIM1, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig( TIM1, ENABLE);
    TIM_Cmd( TIM1, ENABLE);

    // Initialize TIM2

//    TIM_TimeBaseInitStructure.TIM_Period = 5;    // 144MHz/6 = 24MHz
    TIM_TimeBaseInitStructure.TIM_Period = 11;    // 144MHz/12 = 12MHz
    TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit( TIM2, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init( TIM2, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM2, ENABLE);
    TIM_OC1PreloadConfig( TIM2, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig( TIM2, ENABLE);
    TIM_Cmd( TIM2, ENABLE);

    TIM_DMACmd(TIM2, TIM_DMA_CC1, ENABLE);

    // NVIC

    NVIC_InitStructure.NVIC_IRQChannel = TIM1_CC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_ITConfig(TIM1, TIM_IT_CC2 , ENABLE);
    TIM_ITConfig(TIM1, TIM_IT_CC3  , ENABLE);

    // Init VRAM

    vram = malloc(VGA_X_CHARS * VGA_Y_CHARS);
    cvram = malloc(VGA_X_CHARS * VGA_Y_CHARS);

    scandata[0] = malloc((VGA_X_PIXELS + 1)*4);
    scandata[1] = malloc((VGA_X_PIXELS + 1)*4);

    scandata[0][VGA_X_PIXELS] = vga_color[0];
    scandata[1][VGA_X_PIXELS] = vga_color[0];

    //

}

static inline void video_cls() {
    memset(vram, 0, (VGA_X_CHARS * VGA_Y_CHARS));
    memset(cvram, fbcolor, (VGA_X_CHARS * VGA_Y_CHARS));
}

static inline void video_scroll() {

    memmove(vram, vram + VGA_X_CHARS, VGA_X_CHARS * (VGA_Y_CHARS - 1));
    memmove(cvram, cvram + VGA_X_CHARS, VGA_X_CHARS * (VGA_Y_CHARS - 1));

    memset(vram + VGA_X_CHARS * (VGA_Y_CHARS - 1), 0, VGA_X_CHARS);
    memset(cvram + VGA_X_CHARS * (VGA_Y_CHARS - 1), 0, VGA_X_CHARS);

}

static inline void video_print(uint8_t *string) {

    int len;

    len = strlen(string);

    for (int i = 0; i < len; i++) {
        vram[cursor_x + cursor_y * VGA_X_CHARS] = string[i];
        cvram[cursor_x + cursor_y * VGA_X_CHARS] = fbcolor;
        cursor_x++;
        if (cursor_x >= VGA_X_CHARS) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= VGA_Y_CHARS) {
                video_scroll();
                cursor_y = VGA_Y_CHARS - 1;
            }
        }
    }

}

void DMA_Tx_Init(DMA_Channel_TypeDef *DMA_CHx, u32 ppadr, u32 memadr,
        u16 bufsize) {
    DMA_InitTypeDef DMA_InitStructure = { 0 };

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA_CHx);

    DMA_InitStructure.DMA_PeripheralBaseAddr = ppadr;
    DMA_InitStructure.DMA_MemoryBaseAddr = memadr;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = bufsize;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA_CHx, &DMA_InitStructure);

}

void TIM1_CC_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void TIM1_CC_IRQHandler(void) {

    if(TIM_GetITStatus(TIM1, TIM_IT_CC2)==SET) {
        TIM_ClearFlag(TIM1, TIM_FLAG_CC2);
        if(vga_line==0) {
            GPIO_WriteBit(GPIOA, GPIO_Pin_4, RESET);
        }
        if(vga_line==3) {
            GPIO_WriteBit(GPIOA, GPIO_Pin_4, SET);
        }
        // V-SYNC
        return;
    }

    TIM_ClearFlag(TIM1, TIM_FLAG_CC3);
    uint8_t char_x, char_y, slice_y,ch,col,scanbuf;
    uint32_t fcol,bcol;
  //  uint8_t render_line;

//    GPIO_WriteBit(GPIOA, GPIO_Pin_0, SET);

    vga_line++;

    // Video Out

    if ((vga_line >= VGA_SCAN_START)
            && (vga_line < (VGA_SCAN_START + VGA_Y_PIXELS))) { // video out
//        DMA_Tx_Init(DMA1_Channel5, (u32) &(GPIOA->BSHR),
//                (u32) scandata[vga_line % 2], VGA_X_PIXELS + 1);
        DMA_Tx_Init(DMA1_Channel5, (u32) &(GPIOA->BSHR),
                (u32) scandata[(vga_line>>1) % 2], VGA_X_PIXELS + 1);
        DMA_Cmd(DMA1_Channel5, ENABLE);
    }

    // Redner fonts for next scanline

    if ((vga_line >= VGA_SCAN_START - 2)
            && (vga_line < (VGA_SCAN_START + VGA_Y_PIXELS - 2))) {

        if(vga_line%2==0) {
//            char_y = (vga_line + 1 - VGA_SCAN_START) / 8;
//            slice_y = (vga_line + 1 - VGA_SCAN_START) % 8;
            char_y = (vga_line + 2 - VGA_SCAN_START) / 16;
            slice_y = (vga_line + 2 - VGA_SCAN_START) % 16;
            slice_y>>=1;
            for (char_x = 0; char_x < VGA_X_CHARS; char_x++) {
                ch=msxfont[vram[char_x + char_y * VGA_X_CHARS] * 8 + slice_y];
                col=cvram[char_x + char_y * VGA_X_CHARS];
                scanbuf=((vga_line>>1) + 1) % 2;
                fcol=vga_color[col>>4];
                bcol=vga_color[col&7];

                // loop unrolling for speed up

                int slice_x=0;
                if((ch & 128)!=0) {
                    scandata[scanbuf][char_x*8 + slice_x] = fcol;
                } else {
                    scandata[scanbuf][char_x*8 + slice_x] = bcol;
                }

                slice_x++;
                if((ch & 64)!=0) {
                    scandata[scanbuf][char_x*8 + slice_x] = fcol;
                } else {
                    scandata[scanbuf][char_x*8 + slice_x] = bcol;
                }

                slice_x++;
                if((ch & 32)!=0) {
                    scandata[scanbuf][char_x*8 + slice_x] = fcol;
                } else {
                    scandata[scanbuf][char_x*8 + slice_x] = bcol;
                }

                slice_x++;
                if((ch & 16)!=0) {
                    scandata[scanbuf][char_x*8 + slice_x] = fcol;
                } else {
                    scandata[scanbuf][char_x*8 + slice_x] = bcol;
                }

                slice_x++;
                if((ch & 8)!=0) {
                    scandata[scanbuf][char_x*8 + slice_x] = fcol;
                } else {
                    scandata[scanbuf][char_x*8 + slice_x] = bcol;
                }

                slice_x++;
                if((ch & 4)!=0) {
                    scandata[scanbuf][char_x*8 + slice_x] = fcol;
                } else {
                    scandata[scanbuf][char_x*8 + slice_x] = bcol;
                }

                slice_x++;
                if((ch & 2)!=0) {
                    scandata[scanbuf][char_x*8 + slice_x] = fcol;
                } else {
                    scandata[scanbuf][char_x*8 + slice_x] = bcol;
                }

                slice_x++;
                if((ch & 1)!=0) {
                    scandata[scanbuf][char_x*8 + slice_x] = fcol;
                } else {
                    scandata[scanbuf][char_x*8 + slice_x] = bcol;
                }

            }
        }
    }

    if (vga_line > 525) {
        vga_line = 0;
    }

//    GPIO_WriteBit(GPIOA, GPIO_Pin_0, RESET);

}

int main(void) {

    uint8_t ii = 0;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    video_init();

    video_cls();

    Delay_Init();

    //  printf("SystemClk:%d\r\n", SystemCoreClock);

    while(1)
    {

        for (int y = 0; y < VGA_Y_CHARS; y++) {
            for (int x = 0; x < VGA_X_CHARS; x++) {
                vram[x + y * VGA_X_CHARS] = ii++;
                cvram[x + y * VGA_X_CHARS] = rand() & 0x77;
                Delay_Ms(10);
            }
        }

        Delay_Ms(1000);

        for (int y = 0; y < VGA_Y_CHARS; y++) {
            video_scroll();
            Delay_Ms(100);
        }

        video_cls();

        cursor_x=0;
        cursor_y=0;

        for(int i=0;i<500;i++) {

            fbcolor=((i%8)<<4)+((i>>3)%8);
            video_print("CH32V203 VGA Output demonstration ");
            Delay_Ms(30);

        }


        Delay_Ms(1000);

        fbcolor=0x71;

        video_cls();

        cursor_x=0;
        cursor_y=0;
        video_print("MSX BASIC version 1.0");

        cursor_x=0;
        cursor_y=1;
        video_print("Copyright 1983 by Microsoft");

        cursor_x=0;
        cursor_y=2;
        video_print("23430 Bytes free");

        cursor_x=0;
        cursor_y=3;
        video_print("Ok");

        cursor_x=0;
        cursor_y=VGA_Y_CHARS-1;
        video_print("color auto  goto  list  run");

        Delay_Ms(5000);

        fbcolor=0x70;

        video_cls();

    }
}

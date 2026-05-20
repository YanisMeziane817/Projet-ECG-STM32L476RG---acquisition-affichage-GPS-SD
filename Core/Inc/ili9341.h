/** 
 * ---------------------------------------------------------------+ 
 * @desc        ILI9341 LCD Driver 
 * ---------------------------------------------------------------+ 
 *              Copyright (C) 2020 Marian Hrinko.
 *              Written by Marian Hrinko (mato.hrinko@gmail.com)
 *
 * @author      Marian Hrinko
 * @datum       10.12.2020
 * @file        ili9341.h
 * @update      13.12.2020
 * @version     1.0
 * @tested      AVR Atmega328p
 *
 * @depend      font
 * ---------------------------------------------------------------+
 * @interface   8080-I Series Parallel Interface
 * @pins        5V, 3.3V -> NC, GND, RST, CS, RS, WR, RD, D[7:0] 
 *
 */

#ifndef __ILI9341_H__
#define __ILI9341_H__

#include "stm32l4xx_hal.h"
#include "main.h"
#include <stdint.h>


/* Helpers GPIO */
#define PIN_HIGH(p,n) HAL_GPIO_WritePin((p),(n),GPIO_PIN_SET)
#define PIN_LOW(p,n)  HAL_GPIO_WritePin((p),(n),GPIO_PIN_RESET)

/* Primitives bas-niveau (implémentées dans ili9341.c) */
void LCD_PortWrite(uint8_t d);
void WR_IMPULSE(void);

/*=======================  COMMANDES ILI9341  =======================*/
#define ILI9341_NOP           0x00
#define ILI9341_SWRESET       0x01
#define ILI9341_RDDIDIF       0x04
#define ILI9341_RDDST         0x09
#define ILI9341_RDDPM         0x0A
#define ILI9341_RDDMADCTL     0x0B
#define ILI9341_RDDCOLMOD     0x0C
#define ILI9341_RDDIM         0x0D
#define ILI9341_RDDSM         0x0E
#define ILI9341_RDDSDR        0x0F
#define ILI9341_SLPIN         0x10
#define ILI9341_SLPOUT        0x11
#define ILI9341_PTLON         0x12
#define ILI9341_NORON         0x13
#define ILI9341_DINVOFF       0x20
#define ILI9341_DINVON        0x21
#define ILI9341_GAMSET        0x26
#define ILI9341_DISPOFF       0x28
#define ILI9341_DISPON        0x29
#define ILI9341_CASET         0x2A
#define ILI9341_PASET         0x2B
#define ILI9341_RAMWR         0x2C
#define ILI9341_RGBSET        0x2D
#define ILI9341_RAMRD         0x2E
#define ILI9341_PLTAR         0x30
#define ILI9341_VSCRDEF       0x33
#define ILI9341_TEOFF         0x34
#define ILI9341_TEON          0x35
#define ILI9341_MADCTL        0x36
#define ILI9341_VSSAD         0x37
#define ILI9341_IDMOFF        0x38
#define ILI9341_IDMON         0x39
#define ILI9341_COLMOD        0x3A
#define ILI9341_WMCON         0x3C
#define ILI9341_RMCON         0x3E
#define ILI9341_IFMODE        0xB0
#define ILI9341_FRMCRN1       0xB1
#define ILI9341_FRMCRN2       0xB2
#define ILI9341_FRMCRN3       0xB3
#define ILI9341_INVTR         0xB4
#define ILI9341_PRCTR         0xB5
#define ILI9341_DISCTRL       0xB6
#define ILI9341_ETMOD         0xB7
#define ILI9341_BKCR1         0xB8
#define ILI9341_BKCR2         0xB9
#define ILI9341_BKCR3         0xBA
#define ILI9341_BKCR4         0xBB
#define ILI9341_BKCR5         0xBC
#define ILI9341_BKCR7         0xBE
#define ILI9341_BKCR8         0xBF
#define ILI9341_PWCTRL1       0xC0
#define ILI9341_PWCTRL2       0xC1
#define ILI9341_VCCR1         0xC5
#define ILI9341_VCCR2         0xC7
#define ILI9341_RDID1         0xDA
#define ILI9341_RDID2         0xDB
#define ILI9341_RDID3         0xDC
#define ILI9341_GMCTRP1       0xE0
#define ILI9341_GMCTRN1       0xE1
/* Registres étendus */
#define ILI9341_LCD_POWERA    0xCB
#define ILI9341_LCD_POWERB    0xCF
#define ILI9341_LCD_DTCA      0xE8
#define ILI9341_LCD_DTCB      0xEA
#define ILI9341_LCD_POWER_SEQ 0xED
#define ILI9341_LCD_3GAMMA_EN 0xF2
#define ILI9341_LCD_PRC       0xF7

/*======================  DÉFS LOGICIELLES / COULEURS  ==============*/
#define ILI9341_SUCCESS 0
#define ILI9341_ERROR   1

/* RGB565 */
#define ILI9341_BLACK   0x0000
#define ILI9341_WHITE   0xFFFF
#define ILI9341_RED     0xF800
#define ILI9341_GREEN     0x07C0

/* Dimensions (portrait 240x320) */
#define ILI9341_MAX_X   240UL
#define ILI9341_MAX_Y   320UL
#define ILI9341_SIZE_X  (ILI9341_MAX_X - 1)
#define ILI9341_SIZE_Y  (ILI9341_MAX_Y - 1)
#define ILI9341_CACHE_MEM (ILI9341_MAX_X * ILI9341_MAX_Y)

/* Tailles de police */
typedef enum {
  X1 = 0x00,  /* 1xH, 1xW */
  X2 = 0x80,  /* 2xH, 1xW */
  X3 = 0x81   /* 2xH, 2xW */
} ILI9341_Sizes;

/*===========================  API   ========================*/
extern const uint8_t INIT_ILI9341[];


void ILI9341_Init(void);
void ILI9341_HWReset(void);
void ILI9341_InitPorts(void);

void ILI9341_TransmitCmmd(uint8_t cmmd);
void ILI9341_Transmit8bitData(uint8_t data);
void ILI9341_Transmit16bitData(uint16_t data);
void ILI9341_Transmit16bitDataflux (uint16_t data);
void ILI9341_Transmit32bitData(uint32_t data);

char ILI9341_SetWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);
void ILI9341_SendColor565(uint16_t color, uint32_t count);

char ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI9341_ClearScreen(uint16_t color);
void ILI9341_InverseScreen(void);
void ILI9341_NormalScreen(void);
void ILI9341_UpdateScreen(void);

void ILI9341_FillScreen(uint16_t color);
void ILI9341_DrawLine(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, uint16_t color);
char ILI9341_DrawLineHorizontal(uint16_t xs, uint16_t xe, uint16_t y, uint16_t color);
char ILI9341_DrawLineVertical(uint16_t x, uint16_t ys, uint16_t ye, uint16_t color);

/* Texte */
char ILI9341_DrawChar(char character, uint16_t color, ILI9341_Sizes size);
char ILI9341_CheckPosition(uint16_t x, uint16_t y, uint16_t max_y, ILI9341_Sizes size);
char ILI9341_SetPosition(uint16_t x, uint16_t y);
void ILI9341_DrawString(char *str, uint16_t color, ILI9341_Sizes size);
void ILI9341_PrintValue(char *val, uint16_t x, uint16_t y);

/* Divers */
void ILI9341_Delay(uint16_t ms);

/* Cercle */
void ILI9341_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void ILI9341_FillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

#endif

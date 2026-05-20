/** 
 * -----------------------------------------------------------------------+ 
 * @desc        ILI9341 LCD Driver 
 * -----------------------------------------------------------------------+
 *              Copyright (C) 2020 Marian Hrinko.
 *              Written by Marian Hrinko (mato.hrinko@gmail.com)
 *
 * @author      Marian Hrinko
 * @datum       10.12.2020
 * @update      13.12.2020
 * @file        ili9341.c
 * @version     1.0
 * @tested      AVR Atmega328p
 *
 * @depend      font
 * -----------------------------------------------------------------------+
 * @interface   8080-I Series Parallel Interface
 * @pins        5V, 3.3V -> NC, GND, RST, CS, RS, WR, RD, D[7:0] 
 *
 */

#include <stdio.h>
#include "font.h"
#include "ili9341.h"

/*==============   BAS NIVEAU  ==============*/

void LCD_PortWrite(uint8_t d)
{
  HAL_GPIO_WritePin(LCD_D0_GPIO_Port, LCD_D0_Pin, (d & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_D1_GPIO_Port, LCD_D1_Pin, (d & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_D2_GPIO_Port, LCD_D2_Pin, (d & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_D3_GPIO_Port, LCD_D3_Pin, (d & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_D4_GPIO_Port, LCD_D4_Pin, (d & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_D5_GPIO_Port, LCD_D5_Pin, (d & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_D6_GPIO_Port, LCD_D6_Pin, (d & 0x40) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_D7_GPIO_Port, LCD_D7_Pin, (d & 0x80) ? GPIO_PIN_SET : GPIO_PIN_RESET);

  //  TEMPS DE STABILISATION OBLIGATOIRE
  for (volatile int i = 0; i < 10; i++) {
          __NOP();
      }
}

void WR_IMPULSE(void)
{
  PIN_LOW (LCD_WR_GPIO_Port, LCD_WR_Pin);   // WR actif bas
  // largeur WR garantie
  for (volatile int i = 0; i < 5; i++) {
      __NOP();
  }             // petit tWL
  PIN_HIGH(LCD_WR_GPIO_Port, LCD_WR_Pin);
  // largeur WR garantie
  for (volatile int i = 0; i < 5; i++) {
      __NOP();
  }
}


/*====================  TABLE D'INITIALISATION  ===================*/

const uint8_t INIT_ILI9341[] = {
  /* nombre d'initialisers */ 12,

  0,  50, ILI9341_SWRESET,          // reset soft
  0,   0, ILI9341_DISPOFF,          // écran OFF

  1,   0, ILI9341_PWCTRL1, 0x23,    // C0
  1,   0, ILI9341_PWCTRL2, 0x10,    // C1
  2,   0, ILI9341_VCCR1,  0x2B, 0x2B, // C5
  1,   0, ILI9341_VCCR2,  0xC0,    // C7

  1,   0, ILI9341_MADCTL, 0x48,     // rotation/axes
  1,   0, ILI9341_COLMOD, 0x55,     // RGB565 - 16 bits interface Format
  2,   0, ILI9341_FRMCRN1,0x00, 0x1B, // B1 - Frame rate 70 Hz

  1,   0, ILI9341_ETMOD,  0x07,     // B7 - Normal display
  0, 150, ILI9341_SLPOUT,           // sortir du sleep
  0, 200, ILI9341_DISPON            // ON
};

/*======================  VARIABLES INTERNES  =====================*/

static uint16_t _ili9341_cache_index_row = 0;
static uint16_t _ili9341_cache_index_col = 0;

/*======================  FONCTIONS DE BASE  ======================*/

void ILI9341_Delay (uint16_t ms) { HAL_Delay(ms); }

void ILI9341_InitPorts(void)
{
  PIN_HIGH(LCD_RD_GPIO_Port,  LCD_RD_Pin);
  PIN_HIGH(LCD_WR_GPIO_Port,  LCD_WR_Pin);
  PIN_LOW (LCD_CS_GPIO_Port,  LCD_CS_Pin);

  PIN_LOW (LCD_RST_GPIO_Port, LCD_RST_Pin);
  HAL_Delay(10);
  PIN_HIGH(LCD_RST_GPIO_Port, LCD_RST_Pin);
  HAL_Delay(120);
}

void ILI9341_HWReset(void)
{
  PIN_LOW (LCD_RST_GPIO_Port, LCD_RST_Pin);
  HAL_Delay(10);
  PIN_HIGH(LCD_RST_GPIO_Port, LCD_RST_Pin);
  HAL_Delay(200);
}


void ILI9341_TransmitCmmd (uint8_t cmmd)
{
  PIN_LOW (LCD_CS_GPIO_Port, LCD_CS_Pin);  // CS actif
  PIN_LOW (LCD_RS_GPIO_Port, LCD_RS_Pin);  // RS=0 => Command


  LCD_PortWrite(cmmd);
  WR_IMPULSE();

  //PIN_HIGH(LCD_RS_GPIO_Port, LCD_RS_Pin);  // RS=1 (data par défaut)
  //PIN_HIGH(LCD_CS_GPIO_Port, LCD_CS_Pin);  // CS inactif
}

void ILI9341_Transmit8bitData (uint8_t data)
{
  //PIN_LOW (LCD_CS_GPIO_Port, LCD_CS_Pin);
  PIN_HIGH(LCD_RS_GPIO_Port, LCD_RS_Pin);  // Data


  LCD_PortWrite(data);
  WR_IMPULSE();

  //PIN_HIGH(LCD_CS_GPIO_Port, LCD_CS_Pin);
}

void ILI9341_Transmit16bitDataflux (uint16_t data)
{
  PIN_LOW (LCD_CS_GPIO_Port, LCD_CS_Pin);
  PIN_HIGH(LCD_RS_GPIO_Port, LCD_RS_Pin);


  LCD_PortWrite((uint8_t)(data >> 8      )); WR_IMPULSE();
  LCD_PortWrite((uint8_t)(data >> 0  )); WR_IMPULSE();

  //PIN_HIGH(LCD_CS_GPIO_Port, LCD_CS_Pin);
}

void ILI9341_Transmit16bitData (uint16_t data)
{
  PIN_LOW (LCD_CS_GPIO_Port, LCD_CS_Pin);
  PIN_HIGH(LCD_RS_GPIO_Port, LCD_RS_Pin);


  LCD_PortWrite((uint8_t)(data >> 8      )); WR_IMPULSE();
  LCD_PortWrite((uint8_t)(data >> 0  )); WR_IMPULSE();

  //PIN_HIGH(LCD_CS_GPIO_Port, LCD_CS_Pin);
}


void ILI9341_Transmit32bitData (uint32_t data)
{
  //PIN_LOW (LCD_CS_GPIO_Port, LCD_CS_Pin);
  PIN_HIGH(LCD_RS_GPIO_Port, LCD_RS_Pin);


  LCD_PortWrite((uint8_t)(data >> 24)); WR_IMPULSE();
  LCD_PortWrite((uint8_t)(data >> 16)); WR_IMPULSE();
  LCD_PortWrite((uint8_t)(data >> 8)); WR_IMPULSE();
  LCD_PortWrite((uint8_t)(data >> 0)); WR_IMPULSE();

  //PIN_HIGH(LCD_CS_GPIO_Port, LCD_CS_Pin);
}

char ILI9341_SetWindow (uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
  if ((xs > xe) || (xe > ILI9341_SIZE_X) || (ys > ye) || (ye > ILI9341_SIZE_Y)) {
    return ILI9341_ERROR;
  }
  ILI9341_TransmitCmmd(ILI9341_CASET);                      // colonnes
  ILI9341_Transmit32bitData(((uint32_t)xs << 16) | xe);
  //PIN_HIGH(LCD_CS_GPIO_Port, LCD_CS_Pin);
  ILI9341_TransmitCmmd(ILI9341_PASET);                      // lignes
  ILI9341_Transmit32bitData(((uint32_t)ys << 16) | ye);
  //PIN_HIGH(LCD_CS_GPIO_Port, LCD_CS_Pin);
  return ILI9341_SUCCESS;
}

void ILI9341_SendColor565 (uint16_t color, uint32_t count)
{
  ILI9341_TransmitCmmd(ILI9341_RAMWR);
  while (count--) {
    ILI9341_Transmit16bitDataflux(color);
  }
  PIN_HIGH(LCD_CS_GPIO_Port, LCD_CS_Pin);
}

char ILI9341_DrawPixel (uint16_t x, uint16_t y, uint16_t color)
{
  if ((x > ILI9341_SIZE_X) || (y > ILI9341_SIZE_Y)) return ILI9341_ERROR;
  ILI9341_SetWindow(x, y, x, y);
  ILI9341_SendColor565(color, 1);
  return ILI9341_SUCCESS;
}

void ILI9341_ClearScreen (uint16_t color)
{
  ILI9341_SetWindow(0, 0, ILI9341_SIZE_X, ILI9341_SIZE_Y);
  ILI9341_SendColor565(color, ILI9341_CACHE_MEM);
}

void ILI9341_InverseScreen (void) { ILI9341_TransmitCmmd(ILI9341_DINVON);  }
void ILI9341_NormalScreen  (void) { ILI9341_TransmitCmmd(ILI9341_DINVOFF); }
void ILI9341_UpdateScreen  (void) { ILI9341_TransmitCmmd(ILI9341_DISPON);  }

/*=======================  INITIALISATION LCD  ====================*/

void ILI9341_Init (void)
{
  const uint8_t *p = INIT_ILI9341;
  uint16_t n_cmds = *p++;
  uint8_t  n_args;
  uint16_t dly;
  uint8_t  cmd;

  ILI9341_InitPorts();
  ILI9341_HWReset();

  while (n_cmds--) {
    n_args = *p++;
    dly    = *p++;
    cmd    = *p++;
    ILI9341_TransmitCmmd(cmd);
    while (n_args--) {
      ILI9341_Transmit8bitData(*p++);
    }
    PIN_HIGH(LCD_CS_GPIO_Port, LCD_CS_Pin);
    HAL_Delay(dly);
  }

  //ILI9341_SetWindow(0, 0, ILI9341_MAX_X - 1, ILI9341_MAX_Y - 1);
}

/*========================  PRIMITIVES GRAPHIQUES  =================*/
void ILI9341_FillScreen(uint16_t color)
{
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    PIN_LOW(LCD_CS_GPIO_Port, LCD_CS_Pin);

    // CASET
    ILI9341_TransmitCmmd(ILI9341_CASET);
    ILI9341_Transmit8bitData(0x00);
    ILI9341_Transmit8bitData(0x00);
    ILI9341_Transmit8bitData(0x00);
    ILI9341_Transmit8bitData(0xC8); // 239

    // PASET
    ILI9341_TransmitCmmd(ILI9341_PASET);
    ILI9341_Transmit8bitData(0x00);
    ILI9341_Transmit8bitData(0x00);
    ILI9341_Transmit8bitData(0x00);
    ILI9341_Transmit8bitData(0x0A); // 319

    // RAMWR
    ILI9341_TransmitCmmd(ILI9341_RAMWR);

    // Pixels
    PIN_HIGH(LCD_RS_GPIO_Port, LCD_RS_Pin);
    for (uint32_t i = 0; i < 240UL * 320UL; i++) {
        LCD_PortWrite(hi); WR_IMPULSE();
        LCD_PortWrite(lo); WR_IMPULSE();
    }

    PIN_HIGH(LCD_CS_GPIO_Port, LCD_CS_Pin);
}

void ILI9341_DrawLine(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, uint16_t color)
{
  int16_t D;
  int16_t dx = (int16_t)x2 - (int16_t)x1;
  int16_t dy = (int16_t)y2 - (int16_t)y1;
  int16_t sx = (dx >= 0) ? 1 : -1;
  int16_t sy = (dy >= 0) ? 1 : -1;
  dx = (dx >= 0) ? dx : -dx;
  dy = (dy >= 0) ? dy : -dy;

  if (dy < dx) {
    D = (dy << 1) - dx;
    ILI9341_DrawPixel(x1, y1, color);
    while (x1 != x2) {
      x1 += sx;
      if (D >= 0) { y1 += sy; D -= (dx << 1); }
      D += (dy << 1);
      ILI9341_DrawPixel(x1, y1, color);
    }
  } else {
    D = (dy) - (dx << 1);
    ILI9341_DrawPixel(x1, y1, color);
    while (y1 != y2) {
      y1 += sy;
      if (D <= 0) { x1 += sx; D += (dy << 1); }
      D -= (dx << 1);
      ILI9341_DrawPixel(x1, y1, color);
    }
  }
}

char ILI9341_DrawLineHorizontal (uint16_t xs, uint16_t xe, uint16_t y, uint16_t color)
{
  if ((xs > ILI9341_SIZE_X) || (xe > ILI9341_SIZE_X) || (y > ILI9341_SIZE_Y)) return ILI9341_ERROR;
  if (xs > xe) { uint16_t t = xs; xs = xe; xe = t; }
  ILI9341_SetWindow(xs, y, xe, y);
  ILI9341_SendColor565(color, (uint32_t)(xe - xs + 1));
  return ILI9341_SUCCESS;
}

char ILI9341_DrawLineVertical (uint16_t x, uint16_t ys, uint16_t ye, uint16_t color)
{
  if ((ys > ILI9341_SIZE_Y) || (ye > ILI9341_SIZE_Y) || (x > ILI9341_SIZE_X)) return ILI9341_ERROR;
  if (ys > ye) { uint16_t t = ys; ys = ye; ye = t; }
  ILI9341_SetWindow(x, ys, x, ye);
  ILI9341_SendColor565(color, (uint32_t)(ye - ys + 1));
  return ILI9341_SUCCESS;
}

/*===========================  TEXTE  ==============================*/

char ILI9341_CheckPosition (uint16_t x, uint16_t y, uint16_t max_y, ILI9341_Sizes size)
{
  (void)size;
  if ((x > ILI9341_SIZE_X) && (y > max_y)) return ILI9341_ERROR;
  if ((x > ILI9341_SIZE_X) && (y <= max_y)) { _ili9341_cache_index_row = y; _ili9341_cache_index_col = 2; }
  return ILI9341_SUCCESS;
}

char ILI9341_SetPosition (uint16_t x, uint16_t y)
{
  if ((x > ILI9341_SIZE_X) && (y > ILI9341_SIZE_Y)) return ILI9341_ERROR;
  if ((x > ILI9341_SIZE_X) && (y <= ILI9341_SIZE_Y)) { _ili9341_cache_index_row = y; _ili9341_cache_index_col = 2; }
  else { _ili9341_cache_index_row = y; _ili9341_cache_index_col = x; }
  return ILI9341_SUCCESS;
}

char ILI9341_DrawChar (char character, uint16_t color, ILI9341_Sizes size)
{
  if ((character < 0x20) || (character > 0x7f)) return ILI9341_ERROR;

  uint8_t idxCol = CHARS_COLS_LENGTH;
  uint8_t idxRow = CHARS_ROWS_LENGTH;

  if (size == X1) {
    while (idxCol--) {
      uint8_t letter = FONTS[(uint8_t)character - 32][idxCol];
      while (idxRow--) {
        if (letter & (1u << idxRow)) {
          ILI9341_DrawPixel(_ili9341_cache_index_col + idxCol, _ili9341_cache_index_row + idxRow, color);
        }
      }
      idxRow = CHARS_ROWS_LENGTH;
    }
    _ili9341_cache_index_col += CHARS_COLS_LENGTH + 1;
  } else if (size == X2) {
    while (idxCol--) {
      uint8_t letter = FONTS[(uint8_t)character - 32][idxCol];
      while (idxRow--) {
        if (letter & (1u << idxRow)) {
          ILI9341_DrawPixel(_ili9341_cache_index_col + idxCol, _ili9341_cache_index_row + (idxRow << 1),     color);
          ILI9341_DrawPixel(_ili9341_cache_index_col + idxCol, _ili9341_cache_index_row + (idxRow << 1) + 1, color);
        }
      }
      idxRow = CHARS_ROWS_LENGTH;
    }
    _ili9341_cache_index_col += CHARS_COLS_LENGTH + 2;
  } else { /* X3 */
    while (idxCol--) {
      uint8_t letter = FONTS[(uint8_t)character - 32][idxCol];
      while (idxRow--) {
        if (letter & (1u << idxRow)) {
          uint16_t x = _ili9341_cache_index_col + (idxCol << 1);
          uint16_t y = _ili9341_cache_index_row + (idxRow << 1);
          ILI9341_DrawPixel(x,   y,   color);
          ILI9341_DrawPixel(x,   y+1, color);
          ILI9341_DrawPixel(x+1, y,   color);
          ILI9341_DrawPixel(x+1, y+1, color);
        }
      }
      idxRow = CHARS_ROWS_LENGTH;
    }
    _ili9341_cache_index_col += (CHARS_COLS_LENGTH << 1) + 2;
  }
  return ILI9341_SUCCESS;
}

void ILI9341_DrawString (char *str, uint16_t color, ILI9341_Sizes size)
{
  uint16_t i = 0;
  uint16_t delta_y = CHARS_ROWS_LENGTH + (size >> 4);
  uint16_t max_y_pos = ILI9341_SIZE_Y - delta_y;

  while (str[i] != '\0') {
    uint16_t new_x = _ili9341_cache_index_col + CHARS_COLS_LENGTH + (size & 0x0F);
    uint16_t new_y = _ili9341_cache_index_row + delta_y;
    if (ILI9341_CheckPosition(new_x, new_y, max_y_pos, size) == ILI9341_SUCCESS) {
      ILI9341_DrawChar(str[i++], color, size);
    } else {
      break;
    }
  }
}

void ILI9341_PrintValue(char *val, uint16_t x, uint16_t y)
{
  // Efface uniquement la zone de la valeur
  uint16_t xe = x + 40;
  if (xe > ILI9341_SIZE_X) xe = ILI9341_SIZE_X;

  uint16_t ye = y + 20;
  if (ye > ILI9341_SIZE_Y) ye = ILI9341_SIZE_Y;

  ILI9341_SetWindow(x, y, xe, ye);
  ILI9341_SendColor565(ILI9341_BLACK,
                       (uint32_t)(xe - x + 1) * (uint32_t)(ye - y + 1));

  ILI9341_SetPosition(x, y);
  ILI9341_DrawString(val, ILI9341_RED, X2);
}
/*===========================  CERCLE  =============================*/

static inline void _putpx(int16_t x, int16_t y, uint16_t c) { ILI9341_DrawPixel(x, y, c); }

void ILI9341_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  ILI9341_FillCircle(x0, y0, r, ILI9341_BLACK);

  _putpx(x0,     y0 + r, color);
  _putpx(x0,     y0 - r, color);
  _putpx(x0 + r, y0,     color);
  _putpx(x0 - r, y0,     color);

  while (x < y) {
    if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
    x++; ddF_x += 2; f += ddF_x;

    _putpx(x0 + x, y0 + y, color);
    _putpx(x0 - x, y0 + y, color);
    _putpx(x0 + x, y0 - y, color);
    _putpx(x0 - x, y0 - y, color);
    _putpx(x0 + y, y0 + x, color);
    _putpx(x0 - y, y0 + x, color);
    _putpx(x0 + y, y0 - x, color);
    _putpx(x0 - y, y0 - x, color);
  }
}

void ILI9341_FillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;


  ILI9341_DrawLineVertical(x0, y0 - r, y0 + r, color);

  while (x < y) {
    if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
    x++; ddF_x += 2; f += ddF_x;


    ILI9341_DrawLineVertical(x0 + x, y0 - y, y0 + y, color);
    ILI9341_DrawLineVertical(x0 - x, y0 - y, y0 + y, color);
    ILI9341_DrawLineVertical(x0 + y, y0 - x, y0 + x, color);
    ILI9341_DrawLineVertical(x0 - y, y0 - x, y0 + x, color);
  }
}




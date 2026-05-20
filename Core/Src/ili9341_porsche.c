/*
 * ili9341_porsche.c
 *
 *  Created on: Feb 9, 2026
 *      Author: ben
 */
#include "ili9341_porsche.h"
/* --------- titre LCD --------- */
void LCD_DrawHeader(void)
{
  const char *title = " Microros - Porsche-V1 ";
  const char *menu11 = "Steer En: ";
  const char *menu12 = "Accel En: ";
  const char *menu21 = "Steer: ";
  const char *menu22 = "Vel: ";

  const char *menu31 = "Brk FrRi En: ";
  const char *menu32 = "Brk FrLe En: ";
  const char *menu41 = "Brk FrRi: ";
  const char *menu42 = "Brk FrLe: ";

  const char *menu51 = "Brk RrRi En: ";
  const char *menu52 = "Brk RrLe En: ";
  const char *menu61 = "Brk RrRi: ";
  const char *menu62 = "Brk RrLe: ";

  const char *menu71 = "Imu en: ";
  const char *menu72 = "Head en: ";
  const char *menu81 = "Imu a_x: ";
  const char *menu82 = "Head: ";

  const char *menu91 = "Odo FrRi En: ";
  const char *menu92 = "Odo FrLe En: ";
  const char *menu101 = "Odo FrRi: ";
  const char *menu102 = "Odo FrLe: ";

  const char *menu111 = "Odo RrRi En: ";
  const char *menu112 = "Odo RrLe En: ";
  const char *menu121 = "Odo RrRi: ";
  const char *menu122 = "Odo RrLe: ";

  uint16_t y_title        = 20;
  uint16_t y_line_top     = 10;
  uint16_t y_line_bottom1 = 40;
  uint16_t y_line_bottom2 = 90;
  uint16_t y_line_bottom4 = 180;
  uint16_t y_line_bottom5 = 230;



  uint16_t y_menu1 		= 50;
  uint16_t y_menu2 		= 70;
  uint16_t y_menu3 		= 100;
  uint16_t y_menu4 		= 120;
  uint16_t y_menu5 		= 140;
  uint16_t y_menu6 		= 160;
  uint16_t y_menu7 		= 190;
  uint16_t y_menu8 		= 210;
  uint16_t y_menu9 		= 240;
  uint16_t y_menu10 	= 260;
  uint16_t y_menu11 	= 280;
  uint16_t y_menu12 	= 300;

  uint16_t char_w = CHARS_COLS_LENGTH + 2;
  uint16_t text_w = (uint16_t)strlen(title) * char_w;

  uint16_t x0 = 0;
  if (text_w < ILI9341_SIZE_X)
  {
    x0 = (ILI9341_SIZE_X - text_w) / 2;
  }

  /* lignes horizontales  */
  ILI9341_DrawLineHorizontal(5, ILI9341_SIZE_X - 5, y_line_top,    ILI9341_WHITE);
  ILI9341_DrawLineHorizontal(5, ILI9341_SIZE_X - 5, y_line_bottom1, ILI9341_RED);
  ILI9341_DrawLineHorizontal(5, ILI9341_SIZE_X - 5, y_line_bottom2, ILI9341_RED);
  //ILI9341_DrawLineHorizontal(5, ILI9341_SIZE_X - 5, y_line_bottom3, ILI9341_RED);
  ILI9341_DrawLineHorizontal(5, ILI9341_SIZE_X - 5, y_line_bottom4, ILI9341_GREEN);
  ILI9341_DrawLineHorizontal(5, ILI9341_SIZE_X - 5, y_line_bottom5, ILI9341_GREEN);
  /* titre */
  ILI9341_SetPosition(x0, y_title);
  ILI9341_DrawString((char *)title, ILI9341_WHITE, X2);

  /* menu11 */
  ILI9341_SetPosition(10, y_menu1);
  ILI9341_DrawString((char *)menu11, ILI9341_RED, X2);
  /* menu12 */
  ILI9341_SetPosition(120, y_menu1);
  ILI9341_DrawString((char *)menu12, ILI9341_RED, X2);
  /* menu21 */
  ILI9341_SetPosition(10, y_menu2);
  ILI9341_DrawString((char *)menu21, ILI9341_RED, X2);
  /* menu22 */
  ILI9341_SetPosition(120, y_menu2);
  ILI9341_DrawString((char *)menu22, ILI9341_RED, X2);
  /* menu31 */
  ILI9341_SetPosition(10, y_menu3);
  ILI9341_DrawString((char *)menu31, ILI9341_RED, X2);
  /* menu32 */
  ILI9341_SetPosition(120, y_menu3);
  ILI9341_DrawString((char *)menu32, ILI9341_RED, X2);
  /* menu41 */
  ILI9341_SetPosition(10, y_menu4);
  ILI9341_DrawString((char *)menu41, ILI9341_RED, X2);
  /* menu42 */
  ILI9341_SetPosition(120, y_menu4);
  ILI9341_DrawString((char *)menu42, ILI9341_RED, X2);
  /* menu51 */
  ILI9341_SetPosition(10, y_menu5);
  ILI9341_DrawString((char *)menu51, ILI9341_RED, X2);
  /* menu52 */
  ILI9341_SetPosition(120, y_menu5);
  ILI9341_DrawString((char *)menu52, ILI9341_RED, X2);
  /* menu61 */
  ILI9341_SetPosition(10, y_menu6);
  ILI9341_DrawString((char *)menu61, ILI9341_RED, X2);
  /* menu62 */
  ILI9341_SetPosition(120, y_menu6);
  ILI9341_DrawString((char *)menu62, ILI9341_RED, X2);
  /* menu71 */
  ILI9341_SetPosition(10, y_menu7);
  ILI9341_DrawString((char *)menu71, ILI9341_GREEN, X2);
  /* menu72 */
  ILI9341_SetPosition(120, y_menu7);
  ILI9341_DrawString((char *)menu72, ILI9341_GREEN, X2);
  /* menu81 */
  ILI9341_SetPosition(10, y_menu8);
  ILI9341_DrawString((char *)menu81, ILI9341_GREEN, X2);
  /* menu82 */
  ILI9341_SetPosition(120, y_menu8);
  ILI9341_DrawString((char *)menu82, ILI9341_GREEN, X2);
  /* menu91 */
  ILI9341_SetPosition(10, y_menu9);
  ILI9341_DrawString((char *)menu91, ILI9341_GREEN, X2);
  /* menu92 */
  ILI9341_SetPosition(120, y_menu9);
  ILI9341_DrawString((char *)menu92, ILI9341_GREEN, X2);
  /* menu101 */
  ILI9341_SetPosition(10, y_menu10);
  ILI9341_DrawString((char *)menu101, ILI9341_GREEN, X2);
  /* menu102 */
  ILI9341_SetPosition(120, y_menu10);
  ILI9341_DrawString((char *)menu102, ILI9341_GREEN, X2);
  /* menu111 */
  ILI9341_SetPosition(10, y_menu11);
  ILI9341_DrawString((char *)menu111, ILI9341_GREEN, X2);
  /* menu112 */
  ILI9341_SetPosition(120, y_menu11);
  ILI9341_DrawString((char *)menu112, ILI9341_GREEN, X2);
  /* menu121 */
  ILI9341_SetPosition(10, y_menu12);
  ILI9341_DrawString((char *)menu121, ILI9341_GREEN, X2);
  /* menu122 */
  ILI9341_SetPosition(120, y_menu12);
  ILI9341_DrawString((char *)menu122, ILI9341_GREEN, X2);



}

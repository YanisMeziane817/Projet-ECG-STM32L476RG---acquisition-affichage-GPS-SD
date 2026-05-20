/** 
 * ---------------------------------------------------------------+ 
 * @file        ili9341_porsche.h
 * @brief       Interface graphique pour le projet Porsche-V1
 * ---------------------------------------------------------------+ 
 */

#ifndef __ILI9341_PORSCHE_H__
#define __ILI9341_PORSCHE_H__

#include <stdint.h>
#include <string.h>
#include "ili9341.h"
#include "font.h"

/**
 * @brief  Dessine le tableau de bord complet (titre, lignes et labels).
 * @retval None
 */
void LCD_DrawHeader(void);

#endif /* __ILI9341_PORSCHE_H__ */
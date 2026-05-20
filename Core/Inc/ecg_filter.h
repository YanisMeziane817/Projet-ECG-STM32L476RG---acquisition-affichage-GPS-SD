#ifndef ECG_FILTER_H
#define ECG_FILTER_H

#include "main.h"

// Taille totale du buffer DMA (256 pour la moitié A + 256 pour la moitié B)[cite: 2]
#define ADC_BUF_LEN 512

/* --- Fonctions Publiques --- */

// Initialise le filtre et les variables internes
void ECG_Filter_Init(void);

// Traite un bloc de données envoyé par le DMA (moitié du buffer)
void ECG_Process_Block(uint32_t *input_block, uint16_t size);

// Récupère la dernière valeur filtrée pour l'affichage TFT
uint32_t ECG_Get_Latest_Clean_Value(void);

#endif

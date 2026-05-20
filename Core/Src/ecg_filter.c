#include "ecg_filter.h"

/* --- Variables Privées --- */
static uint32_t last_filtered_val = 0; // Stocke le dernier point propre[cite: 1]

// Paramètres du filtre (Moyenne glissante sur 8 points)[cite: 1]
#define FILTER_WINDOW 8
static uint32_t filter_buffer[FILTER_WINDOW];
static uint8_t  filter_idx = 0;
static uint32_t filter_sum = 0;

/**
 * @brief Initialisation du module ECG[cite: 1]
 */
void ECG_Filter_Init(void) {
    filter_sum = 0;
    filter_idx = 0;
    last_filtered_val = 0;
    for (int i = 0; i < FILTER_WINDOW; i++) {
        filter_buffer[i] = 0;
    }
}

/**
 * @brief Algorithme interne de filtrage (Passe-bas)[cite: 1]
 * @param raw Valeur brute issue de l'ADC
 * @return Valeur lissée (moyenne des 8 derniers points)
 */
static uint32_t Apply_LowPass(uint32_t raw) {
    // Retirer la valeur la plus ancienne de la somme[cite: 1]
    filter_sum -= filter_buffer[filter_idx];

    // Ajouter la nouvelle valeur brute[cite: 1]
    filter_buffer[filter_idx] = raw;
    filter_sum += raw;

    // Avancer l'index de manière circulaire[cite: 1]
    filter_idx = (filter_idx + 1) % FILTER_WINDOW;

    // Retourner la moyenne via décalage de bits (div par 8)[cite: 1]
    return (filter_sum >> 3);
}

/**
 * @brief Traite un bloc de données (appelé par les callbacks DMA)[cite: 1]
 * @param input_block Pointeur vers la partie du buffer à traiter
 * @param size Nombre d'éléments (256)[cite: 2]
 */
void ECG_Process_Block(uint32_t *input_block, uint16_t size) {
    for (uint16_t i = 0; i < size; i++) {
        // Chaque point du bloc passe par le filtre[cite: 1]
        last_filtered_val = Apply_LowPass(input_block[i]);
    }
}

/**
 * @brief Interface pour récupérer la donnée filtrée[cite: 1]
 */
uint32_t ECG_Get_Latest_Clean_Value(void) {
    return last_filtered_val;
}

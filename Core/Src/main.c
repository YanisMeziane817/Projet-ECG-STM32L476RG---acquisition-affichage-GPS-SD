/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c  — VERSION v13
  *  Base : v12
  *  Ajouts :
  *   1. Parser GPRMC pour récupérer la DATE (DDMMYY) en plus de GPGGA
  *   2. Nom fichier : ECG_HH_DD_MM_YYYY.CSV  ex: ECG_18_22_05_2026.CSV
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "ili9341.h"
#include "fatfs.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* ─── Handles ─────────────────────────────────────────────────────────────── */
ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
TIM_HandleTypeDef  htim2;
SPI_HandleTypeDef  hspi1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* ─── Constantes ──────────────────────────────────────────────────────────── */
#define BUFFER_SIZE    20
#define ADC_BASELINE   2000
#define GAIN           5
#define TOTAL_SAMPLES  1000u
#define MA_SIZE        1
#define CSV_BUF_SIZE   512
#define GPS_LINE_SIZE  128

/* ─── Buffers ADC/DMA ─────────────────────────────────────────────────────── */
uint16_t         adc_buffer[BUFFER_SIZE];
volatile uint8_t dma_half_flag     = 0;
volatile uint8_t dma_complete_flag = 0;

/* ─── Filtre MA ───────────────────────────────────────────────────────────── */
static uint16_t ma_buf[MA_SIZE];
static uint8_t  ma_idx = 0;
static uint32_t ma_sum = 0;

/* ─── SD / FatFS ──────────────────────────────────────────────────────────── */
static FATFS    fs;
static FIL      csv_file;
static char     csv_buf[CSV_BUF_SIZE];
static uint16_t csv_pos = 0;

/* ─── GPS ─────────────────────────────────────────────────────────────────── */
static float   gps_latitude  = 0.0f;
static float   gps_longitude = 0.0f;
static float   gps_altitude  = 0.0f;
static char    gps_hh[3]     = "00";   /* heure   HH         */
static char    gps_dd[3]     = "00";   /* jour    DD         */
static char    gps_mm[3]     = "00";   /* mois    MM         */
static char    gps_yyyy[5]   = "2000"; /* année   YYYY       */
static uint8_t gps_valid     = 0;      /* fix GPGGA OK       */
static uint8_t gps_date_valid = 0;     /* date GPRMC OK      */

/* ─── Affichage ───────────────────────────────────────────────────────────── */
static uint16_t pos          = 319;
static uint16_t last_x       = 120;
static uint8_t  first_pt     = 1;
static uint32_t sample_count = 0;

/* ─── Macros ──────────────────────────────────────────────────────────────── */
#define AD8232_WAKE()   HAL_GPIO_WritePin(SDN_GPIO_Port,     SDN_Pin,    GPIO_PIN_SET)
#define AD8232_SLEEP()  HAL_GPIO_WritePin(SDN_GPIO_Port,     SDN_Pin,    GPIO_PIN_RESET)
#define BTN_PRESSED()   (HAL_GPIO_ReadPin(Pousoir_GPIO_Port, Pousoir_Pin) == GPIO_PIN_RESET)
#define SD_CS_HIGH()    HAL_GPIO_WritePin(SD_CS_GPIO_Port,   SD_CS_Pin,  GPIO_PIN_SET)
#define SD_CS_LOW()     HAL_GPIO_WritePin(SD_CS_GPIO_Port,   SD_CS_Pin,  GPIO_PIN_RESET)
#define LOG(msg)        HAL_UART_Transmit(&huart2, (uint8_t*)(msg), strlen(msg), 200)

/* ─── Prototypes ──────────────────────────────────────────────────────────── */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM2_Init(void);
static void MX_ADC1_Init(void);
static void gps_read_blocking(void);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Filtre moyenne mobile
 * ═══════════════════════════════════════════════════════════════════════════ */
static void ma_init(uint16_t baseline)
{
    for (int i = 0; i < MA_SIZE; i++) ma_buf[i] = baseline;
    ma_sum = (uint32_t)baseline * MA_SIZE;
    ma_idx = 0;
}

static uint16_t ma_filter(uint16_t raw)
{
    ma_sum        -= ma_buf[ma_idx];
    ma_buf[ma_idx] = raw;
    ma_sum        += raw;
    ma_idx         = (ma_idx + 1) % MA_SIZE;
    return (uint16_t)(ma_sum / MA_SIZE);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  CSV
 * ═══════════════════════════════════════════════════════════════════════════ */
static void csv_flush(void)
{
    if (csv_pos == 0) return;
    UINT bw;
    f_write(&csv_file, csv_buf, csv_pos, &bw);
    csv_pos = 0;
}

static void csv_write_sample(uint32_t idx, uint16_t raw, uint16_t filt)
{
    char line[32];
    int  len = snprintf(line, sizeof(line), "%lu;%u;%u\r\n",
                        (unsigned long)idx, raw, filt);
    if (csv_pos + (uint16_t)len >= CSV_BUF_SIZE) csv_flush();
    memcpy(&csv_buf[csv_pos], line, (size_t)len);
    csv_pos += (uint16_t)len;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  GPS — parser GPGGA
 *  Récupère : latitude, longitude, altitude, heure HH
 *  Champs : [1]=HHMMSS [2]=lat [3]=N/S [4]=lon [5]=E/W [6]=qualité [9]=alt
 * ═══════════════════════════════════════════════════════════════════════════ */
static void gps_parse_gpgga(const char *line)
{
    const char *start = strstr(line, "$GPGGA");
    if (start == NULL) start = strstr(line, "$GNGGA");
    if (start == NULL) return;

    char buf[GPS_LINE_SIZE];
    strncpy(buf, start, GPS_LINE_SIZE - 1);
    buf[GPS_LINE_SIZE - 1] = '\0';

    char   *fields[12];
    uint8_t nf = 0;
    char   *p  = buf;
    while (*p && nf < 12) {
        fields[nf++] = p;
        while (*p && *p != ',') p++;
        if (*p == ',') { *p = '\0'; p++; }
    }
    if (nf < 10) return;
    if (fields[6][0] == '0' || fields[6][0] == '\0') { gps_valid = 0; return; }

    /* Heure HH [1] : "HHMMSS.ss" → garder seulement HH */
    gps_hh[0] = fields[1][0];
    gps_hh[1] = fields[1][1];
    gps_hh[2] = '\0';

    /* Latitude */
    float lat_raw = atof(fields[2]);
    int   lat_deg = (int)(lat_raw / 100);
    gps_latitude  = lat_deg + (lat_raw - lat_deg * 100) / 60.0f;
    if (fields[3][0] == 'S') gps_latitude = -gps_latitude;

    /* Longitude */
    float lon_raw = atof(fields[4]);
    int   lon_deg = (int)(lon_raw / 100);
    gps_longitude = lon_deg + (lon_raw - lon_deg * 100) / 60.0f;
    if (fields[5][0] == 'W') gps_longitude = -gps_longitude;

    /* Altitude */
    gps_altitude = atof(fields[9]);

    gps_valid = 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  GPS — parser GPRMC
 *  Récupère : date DD MM YYYY
 *  Trame : $GPRMC,HHMMSS,A,lat,N,lon,E,vitesse,cap,DDMMYY,,,*xx
 *  Champs :   [1]       [2][3][4][5][6][7]     [8] [9]
 * ═══════════════════════════════════════════════════════════════════════════ */
static void gps_parse_gprmc(const char *line)
{
    const char *start = strstr(line, "$GPRMC");
    if (start == NULL) start = strstr(line, "$GNRMC");
    if (start == NULL) return;

    char buf[GPS_LINE_SIZE];
    strncpy(buf, start, GPS_LINE_SIZE - 1);
    buf[GPS_LINE_SIZE - 1] = '\0';

    char   *fields[12];
    uint8_t nf = 0;
    char   *p  = buf;
    while (*p && nf < 12) {
        fields[nf++] = p;
        while (*p && *p != ',') p++;
        if (*p == ',') { *p = '\0'; p++; }
    }
    if (nf < 10) return;

    /* Statut [2] : 'A' = valide, 'V' = invalide */
    if (fields[2][0] != 'A') return;

    /* Date [9] : "DDMMYY" → DD, MM, 20YY */
    if (strlen(fields[9]) < 6) return;

    gps_dd[0] = fields[9][0]; gps_dd[1] = fields[9][1]; gps_dd[2] = '\0';
    gps_mm[0] = fields[9][2]; gps_mm[1] = fields[9][3]; gps_mm[2] = '\0';
    /* Année : "YY" → "20YY" */
    gps_yyyy[0] = '2'; gps_yyyy[1] = '0';
    gps_yyyy[2] = fields[9][4]; gps_yyyy[3] = fields[9][5];
    gps_yyyy[4] = '\0';

    gps_date_valid = 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  GPS — lecture bloquante
 *  Attend GPGGA (position+altitude) ET GPRMC (date)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void gps_read_blocking(void)
{
    char     buf[GPS_LINE_SIZE];
    uint8_t  byte;
    uint16_t idx         = 0;
    uint8_t  last_was_cr = 0;
    uint32_t timeout     = HAL_GetTick();

    gps_valid      = 0;
    gps_date_valid = 0;
    LOG("GPS: attente trame...\r\n");

    while ((HAL_GetTick() - timeout) < 5000UL)
    {
        if (HAL_UART_Receive(&huart3, &byte, 1, 10) != HAL_OK) continue;

        if (byte == '\r')
        {
            last_was_cr = 1;
            if (idx > 0) {
                buf[idx] = '\0'; idx = 0;
                gps_parse_gpgga(buf);
                gps_parse_gprmc(buf);

                if (gps_valid && gps_date_valid) {
                    char msg[96];
                    int len = snprintf(msg, sizeof(msg),
                        "GPS OK: %sh%s %s/%s/%s LAT=%.6f LON=%.6f ALT=%.1fm\r\n",
                        gps_hh, "UTC", gps_dd, gps_mm, gps_yyyy,
                        (double)gps_latitude, (double)gps_longitude,
                        (double)gps_altitude);
                    HAL_UART_Transmit(&huart2, (uint8_t*)msg, (uint16_t)len, 200);
                    return;
                }
            }
        }
        else if (byte == '\n')
        {
            if (last_was_cr) { last_was_cr = 0; continue; }
            last_was_cr = 0;
            if (idx > 0) {
                buf[idx] = '\0'; idx = 0;
                gps_parse_gpgga(buf);
                gps_parse_gprmc(buf);

                if (gps_valid && gps_date_valid) {
                    char msg[96];
                    int len = snprintf(msg, sizeof(msg),
                        "GPS OK: %sh%s %s/%s/%s LAT=%.6f LON=%.6f ALT=%.1fm\r\n",
                        gps_hh, "UTC", gps_dd, gps_mm, gps_yyyy,
                        (double)gps_latitude, (double)gps_longitude,
                        (double)gps_altitude);
                    HAL_UART_Transmit(&huart2, (uint8_t*)msg, (uint16_t)len, 200);
                    return;
                }
            }
        }
        else
        {
            last_was_cr = 0;
            if (idx < GPS_LINE_SIZE - 1) buf[idx++] = (char)byte;
            else idx = 0;
        }
    }
    LOG("GPS timeout - pas de fix\r\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    SD_CS_HIGH();

    MX_USART2_UART_Init();
    HAL_Delay(10);
    LOG("\r\n\r\n=== BOOT OK ===\r\n");
    LOG("SYSCLK 80MHz | USART2 115200 8N1\r\n");

    MX_USART3_UART_Init();
    LOG("USART3 GPS OK (PB11=RX 9600)\r\n");

    MX_SPI1_Init();
    MX_FATFS_Init();
    MX_DMA_Init();
    MX_TIM2_Init();
    MX_ADC1_Init();

    SD_CS_HIGH();
    HAL_Delay(10);

    ILI9341_Init();
    ILI9341_ClearScreen(ILI9341_BLACK);

    SD_CS_HIGH();

    AD8232_SLEEP();
    LOG("En attente bouton...\r\n");

    /* ═══ Boucle principale ══════════════════════════════════════════════ */
    while (1)
    {
        while (!BTN_PRESSED()) {}
        HAL_Delay(50);
        while ( BTN_PRESSED()) {}
        HAL_Delay(50);

        LOG("\r\n--- Acquisition demarree ---\r\n");

        AD8232_WAKE();
        HAL_Delay(5);
        ILI9341_ClearScreen(ILI9341_BLACK);
        SD_CS_HIGH();
        HAL_Delay(5);

        /* ── 1. GPS ──────────────────────────────────────────────────── */
        gps_read_blocking();

        /* ── 2. SD ───────────────────────────────────────────────────── */
        uint8_t file_open = 0;

        f_mount(NULL, "", 0);
        HAL_Delay(20);
        FRESULT fm = f_mount(&fs, "", 1);

        if (fm == FR_NO_FILESYSTEM) {
            LOG("SD: formatage FAT32...\r\n");
            BYTE work[512];
            fm = f_mkfs("", FM_FAT32, 0, work, sizeof(work));
            char fmsg[32];
            snprintf(fmsg, sizeof(fmsg), "SD mkfs code=%d\r\n", (int)fm);
            LOG(fmsg);
            fm = f_mount(&fs, "", 1);
        }

        if (fm == FR_OK)
        {
            char fname[32];
            UINT bw;

            /* Nom fichier : ECG_HH_DD_MM_YYYY.CSV si GPS + date valides
               sinon fallback numéroté ECG_001.CSV                      */
            if (gps_valid && gps_date_valid) {
                snprintf(fname, sizeof(fname), "ECG_%s_%s_%s_%s.CSV",
                         gps_hh, gps_dd, gps_mm, gps_yyyy);
            } else {
                FILINFO fno;
                for (uint16_t n = 1; n <= 999; n++) {
                    snprintf(fname, sizeof(fname), "ECG_%03u.CSV", (unsigned)n);
                    if (f_stat(fname, &fno) == FR_NO_FILE) break;
                }
            }

            if (f_open(&csv_file, fname, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK)
            {
                char hdr[160];
                int  hlen;
                if (gps_valid)
                    hlen = snprintf(hdr, sizeof(hdr),
                                    "heure_utc;latitude;longitude;altitude_m\r\n"
                                    "%sh UTC %s/%s/%s;%.6f;%.6f;%.1f\r\n"
                                    "index;raw;filtered\r\n",
                                    gps_hh, gps_dd, gps_mm, gps_yyyy,
                                    (double)gps_latitude,
                                    (double)gps_longitude,
                                    (double)gps_altitude);
                else
                    hlen = snprintf(hdr, sizeof(hdr),
                                    "heure_utc;latitude;longitude;altitude_m\r\n"
                                    "N/A;0.000000;0.000000;0.0\r\n"
                                    "index;raw;filtered\r\n");

                if (f_write(&csv_file, hdr, (UINT)hlen, &bw) == FR_OK && bw == (UINT)hlen) {
                    file_open = 1;
                    LOG("SD OK: "); LOG(fname); LOG("\r\n");
                } else { f_close(&csv_file); LOG("SD ERR write hdr\r\n"); }
            } else { LOG("SD ERR open\r\n"); }
        }
        else {
            char errmsg[32];
            snprintf(errmsg, sizeof(errmsg), "SD ERR mount code=%d\r\n", (int)fm);
            LOG(errmsg);
        }

        /* ── 3. Init MA + reset ───────────────────────────────────────── */
        ma_init(ADC_BASELINE);
        sample_count      = 0;
        pos               = 319;
        last_x            = 120;
        first_pt          = 1;
        csv_pos           = 0;
        dma_complete_flag = 0;
        dma_half_flag     = 0;

        /* ── 4. Démarrage DMA → TIM2 ─────────────────────────────────── */
        HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, BUFFER_SIZE);
        HAL_TIM_Base_Start(&htim2);

        /* ─── Boucle d'acquisition ───────────────────────────────────── */
        while (sample_count < TOTAL_SAMPLES)
        {
            uint8_t process = 0;
            int     offset  = 0;

            if (dma_half_flag)          { dma_half_flag = 0;     process = 1; offset = 0; }
            else if (dma_complete_flag) { dma_complete_flag = 0; process = 1; offset = BUFFER_SIZE / 2; }

            if (!process) continue;

            uint8_t leads_off =
                (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6) == GPIO_PIN_SET) ||
                (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_SET);

            for (int i = offset; i < offset + (BUFFER_SIZE / 2); i++)
            {
                if (sample_count >= TOTAL_SAMPLES) break;

                uint16_t raw      = adc_buffer[i];
                uint16_t filtered = ma_filter(raw);
                if (leads_off) filtered = ADC_BASELINE;

                if (file_open) csv_write_sample(sample_count, raw, filtered);
                sample_count++;

                /* Oscilloscope */
                int32_t c     = (int32_t)filtered - ADC_BASELINE;
                int16_t cur_x = 120 - (int16_t)(c / GAIN);
                if (cur_x <   5) cur_x =   5;
                if (cur_x > 227) cur_x = 227;

                for (uint8_t k = 1; k <= 5; k++) {
                    uint16_t ey = (uint16_t)((pos - k + 320) % 320);
                    ILI9341_DrawLineHorizontal(0, 227, ey, ILI9341_BLACK);
                }

                if (!first_pt)
                    ILI9341_DrawLine(last_x, (uint16_t)cur_x, pos + 1, pos, ILI9341_GREEN);
                else {
                    ILI9341_DrawPixel((uint16_t)cur_x, pos, ILI9341_GREEN);
                    first_pt = 0;
                }

                last_x = (uint16_t)cur_x;
                if (pos == 0) { pos = 319; first_pt = 1; } else pos--;
            }
        }

        /* ── 5. Fin acquisition ──────────────────────────────────────── */
        HAL_TIM_Base_Stop(&htim2);
        HAL_ADC_Stop_DMA(&hadc1);
        dma_complete_flag = 0;
        dma_half_flag     = 0;

        if (file_open) {
            csv_flush();
            f_sync(&csv_file);
            f_close(&csv_file);
            LOG("Fichier ferme OK\r\n");
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "Acquisition OK: %lu echantillons\r\n",
                 (unsigned long)sample_count);
        LOG(msg);

        AD8232_SLEEP();
        SD_CS_HIGH();
        ILI9341_ClearScreen(ILI9341_BLACK);
        SD_CS_HIGH();
        LOG("En attente bouton...\r\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Callbacks DMA/ADC
 * ═══════════════════════════════════════════════════════════════════════════ */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) dma_half_flag = 1;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) dma_complete_flag = 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Initialisations périphériques
 * ═══════════════════════════════════════════════════════════════════════════ */
static void MX_SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 7;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

static void MX_TIM2_Init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 79;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 999;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) Error_Handler();
}

static void MX_ADC1_Init(void)
{
    ADC_MultiModeTypeDef   multimode = {0};
    ADC_ChannelConfTypeDef sConfig   = {0};

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV1;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIG_T2_TRGO;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.Overrun               = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.OversamplingMode      = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    multimode.Mode = ADC_MODE_INDEPENDENT;
    HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode);

    sConfig.Channel      = ADC_CHANNEL_4;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) Error_Handler();
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

static void MX_USART3_UART_Init(void)
{
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 9600;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

    HAL_GPIO_WritePin(GPIOC, LCD_RST_Pin | LCD_D1_Pin | SDN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, LCD_RD_Pin | LCD_WR_Pin | LCD_RS_Pin |
                             LCD_D7_Pin | LCD_D0_Pin | LCD_D2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin | LCD_D6_Pin | LCD_D3_Pin |
                             LCD_D5_Pin | LCD_D4_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = LCD_RST_Pin | LCD_D1_Pin | SDN_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = Analog_input_PC3_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(Analog_input_PC3_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = LCD_RD_Pin | LCD_WR_Pin | LCD_RS_Pin |
                            LCD_D7_Pin | LCD_D0_Pin | LCD_D2_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = LCD_CS_Pin | LCD_D6_Pin | LCD_D3_Pin |
                            LCD_D5_Pin | LCD_D4_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = Pousoir_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(Pousoir_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = SD_CS_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = 1;
    RCC_OscInitStruct.PLL.PLLN            = 10;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}

void Error_Handler(void) { __disable_irq(); while (1) {} }

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif

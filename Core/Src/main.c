/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   ECG AD8232 + GPS — USART2 9600 baud pour tout
  *
  *  GPS TX → PA3 (USART2 RX) 9600 baud
  *  PA2 (USART2 TX) → ST-Link USB → HTerm 9600 baud
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

ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
TIM_HandleTypeDef  htim2;
SPI_HandleTypeDef  hspi1;
UART_HandleTypeDef huart2;   /* GPS RX + debug TX — PA3=RX PA2=TX — 9600 baud */

#define BUFFER_SIZE    20
#define ADC_BASELINE   2000
#define GAIN           5
#define TOTAL_SAMPLES  500u
#define MA_SIZE        4
#define CSV_BUF_SIZE   256
#define GPS_LINE_SIZE  128

uint16_t         adc_buffer[BUFFER_SIZE];
volatile uint8_t dma_complete_flag = 0;

static uint16_t ma_buf[MA_SIZE];
static uint8_t  ma_idx = 0;
static uint32_t ma_sum = 0;

static FATFS    fs;
static FIL      csv_file;
static char     csv_buf[CSV_BUF_SIZE];
static uint16_t csv_pos = 0;

static uint8_t  gps_rx_byte  = 0;
static char     gps_line[GPS_LINE_SIZE];
static uint16_t gps_line_idx = 0;
static uint8_t  gps_line_rdy = 0;
static char     gps_nmea[GPS_LINE_SIZE];
static float    gps_latitude  = 0.0f;
static float    gps_longitude = 0.0f;
static uint8_t  gps_valid     = 0;

static uint16_t pos          = 319;
static uint16_t last_x       = 120;
static uint8_t  first_pt     = 1;
static uint32_t sample_count = 0;

#define AD8232_WAKE()  HAL_GPIO_WritePin(SDN_GPIO_Port, SDN_Pin, GPIO_PIN_SET)
#define AD8232_SLEEP() HAL_GPIO_WritePin(SDN_GPIO_Port, SDN_Pin, GPIO_PIN_RESET)
#define BTN_PRESSED()  (HAL_GPIO_ReadPin(Pousoir_GPIO_Port, Pousoir_Pin) == GPIO_PIN_RESET)
#define LOG(msg)       HAL_UART_Transmit(&huart2, (uint8_t*)(msg), strlen(msg), 100)

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM2_Init(void);
static void MX_ADC1_Init(void);

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

static void csv_flush(void)
{
    if (csv_pos == 0) return;
    UINT bw;
    f_write(&csv_file, csv_buf, csv_pos, &bw);
    csv_pos = 0;
}

static void csv_write_gps(uint32_t idx, uint16_t raw, uint16_t filt)
{
    char line[64];
    int  len;
    if (gps_valid)
        len = snprintf(line, sizeof(line), "%lu;%u;%u;%.6f;%.6f\r\n",
                       (unsigned long)idx, raw, filt,
                       (double)gps_latitude, (double)gps_longitude);
    else
        len = snprintf(line, sizeof(line), "%lu;%u;%u;0.000000;0.000000\r\n",
                       (unsigned long)idx, raw, filt);
    if (csv_pos + (uint16_t)len >= CSV_BUF_SIZE) csv_flush();
    memcpy(&csv_buf[csv_pos], line, (size_t)len);
    csv_pos += (uint16_t)len;
}

static void gps_parse_gprmc(const char *line)
{
    if (strncmp(line, "$GPRMC", 6) != 0 && strncmp(line, "$GNRMC", 6) != 0) return;
    char buf[GPS_LINE_SIZE];
    strncpy(buf, line, GPS_LINE_SIZE - 1);
    buf[GPS_LINE_SIZE - 1] = '\0';
    char *fields[12]; uint8_t nf = 0; char *p = buf;
    while (*p && nf < 12) {
        fields[nf++] = p;
        while (*p && *p != ',') p++;
        if (*p == ',') { *p = '\0'; p++; }
    }
    if (nf < 7) return;
    if (fields[2][0] != 'A') { gps_valid = 0; return; }
    float lat_raw = atof(fields[3]); int lat_deg = (int)(lat_raw / 100);
    gps_latitude  = lat_deg + (lat_raw - lat_deg * 100) / 60.0f;
    if (fields[4][0] == 'S') gps_latitude = -gps_latitude;
    float lon_raw = atof(fields[5]); int lon_deg = (int)(lon_raw / 100);
    gps_longitude = lon_deg + (lon_raw - lon_deg * 100) / 60.0f;
    if (fields[6][0] == 'W') gps_longitude = -gps_longitude;
    gps_valid = 1;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();
    LOG("BOOT OK\r\n");
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_FATFS_Init();
    MX_DMA_Init();
    MX_TIM2_Init();
    MX_ADC1_Init();
    ILI9341_Init();
    ILI9341_ClearScreen(ILI9341_BLACK);
    AD8232_SLEEP();
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
    HAL_UART_Receive_IT(&huart2, &gps_rx_byte, 1);
    LOG("GPS PRET\r\n");
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, BUFFER_SIZE);

    while (1)
    {
        while (!BTN_PRESSED()) {
            if (gps_line_rdy) { gps_line_rdy = 0; gps_parse_gprmc(gps_nmea); }
        }
        HAL_Delay(50);
        while (BTN_PRESSED()) {}
        HAL_Delay(50);
        AD8232_WAKE();
        HAL_Delay(5);
        ILI9341_ClearScreen(ILI9341_BLACK);
        ma_init(ADC_BASELINE);
        sample_count = 0; pos = 319; last_x = 120;
        first_pt = 1; dma_complete_flag = 0; csv_pos = 0;

        uint8_t file_open = 0;
        f_mount(NULL, "", 0); HAL_Delay(50);
        disk_initialize(0); HAL_Delay(20);
        FRESULT fm = f_mount(&fs, "", 1);
        if (fm == FR_NO_FILESYSTEM) {
            BYTE work[512];
            f_mkfs("", FM_FAT32, 0, work, sizeof(work));
            fm = f_mount(&fs, "", 1);
        }
        if (fm == FR_OK) {
            char fname[16]; FILINFO fno; UINT bw;
            for (uint16_t n = 1; n <= 999; n++) {
                snprintf(fname, sizeof(fname), "ECG_%03u.CSV", (unsigned)n);
                if (f_stat(fname, &fno) == FR_NO_FILE) break;
            }
            if (f_open(&csv_file, fname, FA_CREATE_NEW | FA_WRITE) == FR_OK) {
                if (f_write(&csv_file, "index;raw;filtered;latitude;longitude\r\n", 39, &bw) == FR_OK && bw == 39)
                    file_open = 1;
                else f_close(&csv_file);
            }
        }

        HAL_TIM_Base_Start(&htim2);
        while (sample_count < TOTAL_SAMPLES)
        {
            if (dma_complete_flag)
            {
                dma_complete_flag = 0;
                uint8_t leads_off =
                    (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6) == GPIO_PIN_SET) ||
                    (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_SET);
                for (int i = 0; i < BUFFER_SIZE; i++)
                {
                    if (sample_count >= TOTAL_SAMPLES) break;
                    uint16_t raw      = adc_buffer[i];
                    uint16_t filtered = ma_filter(raw);
                    if (leads_off) filtered = ADC_BASELINE;
                    if (file_open) csv_write_gps(sample_count, raw, filtered);
                    sample_count++;
                    int32_t  c     = (int32_t)filtered - ADC_BASELINE;
                    int16_t  cur_x = 120 - (int16_t)(c / GAIN);
                    if (cur_x < 5)   cur_x = 5;
                    if (cur_x > 227) cur_x = 227;
                    for (uint8_t k = 1; k <= 5; k++) {
                        uint16_t ey = (pos - k + 320) % 320;
                        ILI9341_DrawLineHorizontal(0, 227, ey, ILI9341_BLACK);
                    }
                    if (!first_pt)
                        ILI9341_DrawLine(last_x, (uint16_t)cur_x, pos + 1, pos, ILI9341_GREEN);
                    else { ILI9341_DrawPixel((uint16_t)cur_x, pos, ILI9341_GREEN); first_pt = 0; }
                    last_x = (uint16_t)cur_x;
                    if (pos == 0) { pos = 319; first_pt = 1; } else pos--;
                }
            }
        }
        HAL_TIM_Base_Stop(&htim2);
        HAL_ADC_Stop_DMA(&hadc1);
        dma_complete_flag = 0;
        if (file_open) { csv_flush(); f_sync(&csv_file); f_close(&csv_file); }
        AD8232_SLEEP();
        ILI9341_ClearScreen(ILI9341_BLACK);
        HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, BUFFER_SIZE);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) dma_complete_flag = 1;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;
    char c = (char)gps_rx_byte;
    if (c == '\n' || c == '\r') {
        if (gps_line_idx > 0) {
            gps_line[gps_line_idx] = '\0';
            memcpy(gps_nmea, gps_line, gps_line_idx + 1);
            gps_line_rdy = 1;
            gps_line_idx = 0;
        }
    } else if (gps_line_idx < GPS_LINE_SIZE - 1) {
        gps_line[gps_line_idx++] = c;
    } else {
        gps_line_idx = 0;
    }
    HAL_UART_Receive_IT(&huart2, &gps_rx_byte, 1);
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 7;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_6; g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP; g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);
}

static void MX_TIM2_Init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance = TIM2; htim2.Init.Prescaler = 79;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP; htim2.Init.Period = 999;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) Error_Handler();
}

static void MX_ADC1_Init(void)
{
    ADC_MultiModeTypeDef   multimode = {0};
    ADC_ChannelConfTypeDef sConfig   = {0};
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T2_TRGO;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
    multimode.Mode = ADC_MODE_INDEPENDENT;
    HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode);
    sConfig.Channel = ADC_CHANNEL_4; sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED; sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
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
    huart2.Init.BaudRate     = 9600;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
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
    HAL_GPIO_WritePin(GPIOA, LCD_RD_Pin | LCD_WR_Pin | LCD_RS_Pin | LCD_D7_Pin | LCD_D0_Pin | LCD_D2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin | LCD_D6_Pin | LCD_D3_Pin | LCD_D5_Pin | LCD_D4_Pin, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = LCD_RST_Pin | LCD_D1_Pin | SDN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = Analog_input_PC3_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL; GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(Analog_input_PC3_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = LCD_RD_Pin | LCD_WR_Pin | LCD_RS_Pin | LCD_D7_Pin | LCD_D0_Pin | LCD_D2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = LCD_CS_Pin | LCD_D6_Pin | LCD_D3_Pin | LCD_D5_Pin | LCD_D4_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = Pousoir_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(Pousoir_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = SD_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 1; RCC_OscInitStruct.PLL.PLLN = 10;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7; RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}

void Error_Handler(void) { __disable_irq(); while (1) {} }

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif

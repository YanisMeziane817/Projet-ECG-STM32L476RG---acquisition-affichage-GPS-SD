/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   Driver SD SPI pour FatFs — STM32L476
  *          SPI1 : PA5=SCK, PA6=MISO, PA7=MOSI, PB6=CS
 ******************************************************************************
  */
 /* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */
#include <string.h>
#include "ff_gen_drv.h"
#include "main.h"

extern SPI_HandleTypeDef hspi1;

#define SD_CS_LOW()   HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET)
#define SD_CS_HIGH()  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET)

#define CMD0   0x40
#define CMD8   0x48
#define CMD9   0x49
#define CMD12  0x4C
#define CMD16  0x50
#define CMD17  0x51
#define CMD18  0x52
#define CMD24  0x58
#define CMD25  0x59
#define CMD41  0x69
#define CMD55  0x77
#define CMD58  0x7A

static volatile DSTATUS Stat = STA_NOINIT;
static uint8_t CardType = 0;

static uint8_t spi_tx_rx(uint8_t d)
{
    uint8_t r;
    HAL_SPI_TransmitReceive(&hspi1, &d, &r, 1, 100);
    return r;
}

static void spi_skip(uint16_t n)
{
    while (n--) spi_tx_rx(0xFF);
}

static uint8_t wait_ready(void)
{
    uint8_t r;
    uint32_t t = HAL_GetTick();
    do { r = spi_tx_rx(0xFF); }
    while (r != 0xFF && (HAL_GetTick() - t) < 2000);
    return r;
}

static uint8_t send_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t res, n;
    if (cmd & 0x80) {
        cmd &= 0x7F;
        res = send_cmd(CMD55, 0);
        if (res > 1) return res;
    }
    SD_CS_HIGH(); spi_tx_rx(0xFF);
    SD_CS_LOW();  spi_tx_rx(0xFF);
    spi_tx_rx(cmd);
    spi_tx_rx((uint8_t)(arg >> 24));
    spi_tx_rx((uint8_t)(arg >> 16));
    spi_tx_rx((uint8_t)(arg >>  8));
    spi_tx_rx((uint8_t)(arg      ));
    n = 0x01;
    if (cmd == CMD0) n = 0x95;
    if (cmd == CMD8) n = 0x87;
    spi_tx_rx(n);
    n = 10;
    do { res = spi_tx_rx(0xFF); } while ((res & 0x80) && --n);
    return res;
}
/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
    uint8_t n, ty, ocr[4];
    uint32_t t;

    if (pdrv != 0) return STA_NOINIT;

    Stat = STA_NOINIT;
    CardType = 0;

    SD_CS_HIGH();
    HAL_Delay(1);
    for (uint8_t i = 0; i < 20; i++) spi_tx_rx(0xFF);
    HAL_Delay(1);

    uint8_t r0 = 0xFF;
    for (uint8_t retry = 0; retry < 10 && r0 != 1; retry++)
    {
        SD_CS_LOW();
        spi_tx_rx(0xFF);
        spi_tx_rx(0x40);
        spi_tx_rx(0x00);
        spi_tx_rx(0x00);
        spi_tx_rx(0x00);
        spi_tx_rx(0x00);
        spi_tx_rx(0x95);
        r0 = 0xFF;
        for (uint8_t j = 0; j < 8; j++) {
            r0 = spi_tx_rx(0xFF);
            if (!(r0 & 0x80)) break;
        }
        SD_CS_HIGH();
        spi_tx_rx(0xFF);
        HAL_Delay(20);
    }

    ty = 0;
    if (r0 == 1)
    {
        t = HAL_GetTick();
        if (send_cmd(CMD8, 0x1AA) == 1)
        {
            for (n = 0; n < 4; n++) ocr[n] = spi_tx_rx(0xFF);
            if (ocr[2] == 0x01 && ocr[3] == 0xAA)
            {
                while ((HAL_GetTick()-t) < 5000 &&
                       send_cmd((uint8_t)(CMD41|0x80), 0x40000000)) {
                    HAL_Delay(10);
                }
                if ((HAL_GetTick()-t) < 5000 && send_cmd(CMD58, 0) == 0) {
                    for (n = 0; n < 4; n++) ocr[n] = spi_tx_rx(0xFF);
                    ty = (ocr[0] & 0x40) ? 6 : 2;
                }
            }
        }
        else
        {
            uint8_t cmd2;
            if (send_cmd((uint8_t)(CMD41|0x80), 0) <= 1)
                { ty = 1; cmd2 = (uint8_t)(CMD41|0x80); }
            else
                { ty = 1; cmd2 = CMD16; }
            while ((HAL_GetTick()-t) < 1000 && send_cmd(cmd2, 0)) {}
            if ((HAL_GetTick()-t) >= 1000 || send_cmd(CMD16, 512) != 0) ty = 0;
        }
    }

    CardType = ty;
    SD_CS_HIGH();
    spi_tx_rx(0xFF);

    if (ty) {
        Stat &= ~STA_NOINIT;
    } else {
        Stat = STA_NOINIT;
    }
    return Stat;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
    (void)pdrv;
    return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
    if (pdrv || !count)    return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;
    if (!(CardType & 4)) sector *= 512;

    if (count == 1) {
        if (send_cmd(CMD17, sector) == 0) {
            uint32_t t = HAL_GetTick(); uint8_t tk;
            do { tk = spi_tx_rx(0xFF); } while (tk==0xFF && (HAL_GetTick()-t)<200);
            if (tk == 0xFE) {
                for (uint16_t b = 0; b < 512; b++) buff[b] = spi_tx_rx(0xFF);
                spi_skip(2);
                count = 0;
            }
        }
    } else {
        if (send_cmd(CMD18, sector) == 0) {
            do {
                uint32_t t = HAL_GetTick(); uint8_t tk;
                do { tk = spi_tx_rx(0xFF); } while (tk==0xFF && (HAL_GetTick()-t)<200);
                if (tk != 0xFE) break;
                for (uint16_t b = 0; b < 512; b++) buff[b] = spi_tx_rx(0xFF);
                buff += 512;
                spi_skip(2);
            } while (--count);
            send_cmd(CMD12, 0);
        }
    }
    SD_CS_HIGH(); spi_tx_rx(0xFF);
    return count ? RES_ERROR : RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
    if (pdrv || !count)    return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;
    if (!(CardType & 4)) sector *= 512;

    if (count == 1) {
        if (send_cmd(CMD24, sector) == 0) {
            wait_ready();
            HAL_Delay(1);
            spi_tx_rx(0xFE);
            for (uint16_t b = 0; b < 512; b++) spi_tx_rx(((uint8_t*)buff)[b]);
            spi_tx_rx(0xFF);
            spi_tx_rx(0xFF);
            uint8_t resp = spi_tx_rx(0xFF);
            if ((resp & 0x1F) == 0x05) count = 0;
            wait_ready();
        }
    } else {
        if (send_cmd(CMD25, sector) == 0) {
            do {
                wait_ready();
                spi_tx_rx(0xFC);
                for (uint16_t b = 0; b < 512; b++) spi_tx_rx(((uint8_t*)buff)[b]);
                buff += 512;
                spi_skip(2);
                if ((spi_tx_rx(0xFF) & 0x1F) != 0x05) break;
                wait_ready();
            } while (--count);
            wait_ready();
            spi_tx_rx(0xFD);
            wait_ready();
        }
    }
    SD_CS_HIGH(); spi_tx_rx(0xFF);
    return count ? RES_ERROR : RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
    DRESULT  res = RES_ERROR;
    uint8_t  n, csd[16];
    DWORD    csize;

    if (pdrv)              return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    switch (cmd) {
        case CTRL_SYNC:
            SD_CS_LOW();
            res = (wait_ready() == 0xFF) ? RES_OK : RES_ERROR;
            break;
        case GET_SECTOR_COUNT:
            if (send_cmd(CMD9, 0) == 0) {
                uint32_t t = HAL_GetTick(); uint8_t tk;
                do { tk = spi_tx_rx(0xFF); } while (tk==0xFF && (HAL_GetTick()-t)<200);
                if (tk == 0xFE) {
                    for (n=0; n<16; n++) csd[n] = spi_tx_rx(0xFF);
                    spi_skip(2);
                    if ((csd[0]>>6) == 1) {
                        csize = ((DWORD)(csd[7]&0x3F)<<16)|((DWORD)csd[8]<<8)|csd[9];
                        *(DWORD*)buff = (csize+1) << 10;
                    } else {
                        n = (csd[5]&15)+((csd[10]&128)>>7)+((csd[9]&3)<<1)+2;
                        csize = ((DWORD)(csd[8]>>6)|((DWORD)csd[7]<<2)|
                                 ((DWORD)(csd[6]&3)<<10))+1;
                        *(DWORD*)buff = csize << (n-9);
                    }
                    res = RES_OK;
                }
            }
            break;
        case GET_SECTOR_SIZE: *(WORD*)buff  = 512; res = RES_OK; break;
        case GET_BLOCK_SIZE:  *(DWORD*)buff = 128; res = RES_OK; break;
        default: res = RES_PARERR;
    }
    SD_CS_HIGH(); spi_tx_rx(0xFF);
    return res;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */


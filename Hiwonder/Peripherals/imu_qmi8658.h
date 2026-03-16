/**
 * @file imu_qmi8658.h
 * @brief QMI8658 6-DOF IMU driver (accel + gyro, I2C)
 *
 * Device address: 0x6A (SA0=0) or 0x6B (SA0=1)
 * WHO_AM_I reg 0x00 returns 0x05
 */

#ifndef __IMU_QMI8658_H_
#define __IMU_QMI8658_H_

#include "imu.h"
#include <stdint.h>

/* ── Device addresses ─────────────────────────────────────────────────────── */
#define QMI8658_DEV_ADDR_SA0_LOW  0x6A
#define QMI8658_DEV_ADDR_SA0_HIGH 0x6B

/* ── Register map ─────────────────────────────────────────────────────────── */
#define QMI8658_REG_WHO_AM_I   0x00   /* Read: 0x05 */
#define QMI8658_REG_REVISION   0x01
#define QMI8658_REG_CTRL1      0x02   /* Serial IF, INT config */
#define QMI8658_REG_CTRL2      0x03   /* Accel ODR + FS */
#define QMI8658_REG_CTRL3      0x04   /* Gyro  ODR + FS */
#define QMI8658_REG_CTRL5      0x06   /* Low-pass filter */
#define QMI8658_REG_CTRL6      0x07   /* AE divider (set 0x00 for raw sensor output) */
#define QMI8658_REG_CTRL7      0x08   /* Sensor enable */
#define QMI8658_REG_CTRL9      0x0A   /* Command register — write 0x00 to commit CTRL7 */
#define QMI8658_REG_STATUSINT  0x2E   /* Interrupt status (bit0=AvailStatus) */
#define QMI8658_REG_STATUS0    0x2F   /* Reserved/unused on this variant */
#define QMI8658_REG_STATUS1    0x30   /* Sensor data available (bit0=acc, bit1=gyro) */
#define QMI8658_REG_TEMP_L     0x33
#define QMI8658_REG_TEMP_H     0x34
#define QMI8658_REG_RESET      0x60   /* Soft reset: write 0xB0 */
#define QMI8658_REG_AX_L       0x35
#define QMI8658_REG_AY_L       0x37
#define QMI8658_REG_AZ_L       0x39
#define QMI8658_REG_GX_L       0x3B
#define QMI8658_REG_GY_L       0x3D
#define QMI8658_REG_GZ_L       0x3F

/* ── CTRL1 bits ───────────────────────────────────────────────────────────── */
/* bit 0 = AI (I2C addr auto-increment for burst reads), bit 2 = SDO (addr),  *
 * bit 3 = BE, bit 4 = INT1_EN, bit 5 = INT1_LEVEL                            */
#define QMI8658_CTRL1_AI         (1 << 0)   /* I2C address auto-increment (required for burst reads) */
#define QMI8658_CTRL1_INT1_EN    (1 << 4)   /* Enable INT1 output pin */
#define QMI8658_CTRL1_INT1_HIGH  (1 << 5)   /* INT1 active-high */

/* ── CTRL2: Accel ─────────────────────────────────────────────────────────── */
#define QMI8658_ACC_ODR_125Hz   0x06
#define QMI8658_ACC_FS_4G       (1 << 4)    /* ±4g → 8192 LSB/g */

/* ── CTRL3: Gyro ──────────────────────────────────────────────────────────── */
#define QMI8658_GYR_ODR_112Hz   0x06
#define QMI8658_GYR_FS_2048DPS  (7 << 4)   /* ±2048 dps → 16 LSB/dps */
#define QMI8658_GYR_FS_512DPS   (3 << 4)   /* ±512 dps  → 64 LSB/dps */

/* ── CTRL7: sensor enable ─────────────────────────────────────────────────── */
#define QMI8658_CTRL7_ACC_EN    (1 << 0)
#define QMI8658_CTRL7_GYR_EN    (1 << 1)

/* ── Scale factors ────────────────────────────────────────────────────────── */
#define QMI8658_ACC_SCALE    (1.0f / 8192.0f)   /* g per LSB  (±4g range) */
#define QMI8658_GYRO_SCALE   (1.0f / 64.0f)     /* dps per LSB (±512dps)  */

/* ── Object struct ────────────────────────────────────────────────────────── */
typedef struct QMI8658Object {
    IMU_ObjectTypeDef base;   /* MUST be first — cast-compatible with IMU_ObjectTypeDef */

    uint8_t dev_addr;

    float accel[3];      /* [g]     X Y Z */
    float gyro[3];       /* [deg/s] X Y Z */
    float temperature;   /* [°C] */

    /* I2C callbacks (set by porting layer) */
    int  (*i2c_write_byte)(struct QMI8658Object *self, uint8_t reg, uint8_t data);
    int  (*i2c_read_bytes) (struct QMI8658Object *self, uint8_t reg, uint32_t len, uint8_t *buf);
    void (*sleep_ms)       (uint32_t ms);
} QMI8658ObjectTypeDef;

void qmi8658_object_init(QMI8658ObjectTypeDef *self, uint8_t dev_addr);

#endif /* __IMU_QMI8658_H_ */

/**
 * @file imu_qmi8658.c
 * @brief QMI8658 driver implementation
 *
 * Initialises the chip, enables accel+gyro at ~125 Hz, arms the INT1
 * data-ready interrupt (active-high, matches STM32 PB12 rising-edge EXTI),
 * and provides a single burst-read update function.
 */

#include "imu_qmi8658.h"
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static inline int _wr(QMI8658ObjectTypeDef *s, uint8_t reg, uint8_t val)
{
    return s->i2c_write_byte(s, reg, val);
}

static inline int _rd(QMI8658ObjectTypeDef *s, uint8_t reg, uint32_t len, uint8_t *buf)
{
    return s->i2c_read_bytes(s, reg, len, buf);
}

/* Issue a CTRL9 command and poll STATUSINT.bit7 (CmdDone) for up to ~50 ms */
static void _ctrl9_cmd(QMI8658ObjectTypeDef *self, uint8_t cmd)
{
    _wr(self, QMI8658_REG_CTRL9, cmd);
    for (int i = 0; i < 50; i++) {
        uint8_t sint = 0;
        _rd(self, QMI8658_REG_STATUSINT, 1, &sint);
        if (sint & 0x80) break;   /* bit7 = CmdDone */
        self->sleep_ms(1);
    }
    _wr(self, QMI8658_REG_CTRL9, 0x00);   /* clear command */
}

/* ── IMU vtable implementations ──────────────────────────────────────────── */

static void qmi8658_reset(IMU_ObjectTypeDef *base)
{
    QMI8658ObjectTypeDef *self = (QMI8658ObjectTypeDef *)base;

    /* Disable sensors first (no soft reset — it locks up the I2C bus) */
    _wr(self, QMI8658_REG_CTRL7, 0x00);
    _ctrl9_cmd(self, 0x00);
    self->sleep_ms(10);

    /* CTRL1: no INT, leave AI default */
    _wr(self, QMI8658_REG_CTRL1, 0x00);

    /* CTRL2: Accel — ±4g, 125 Hz; then CTRL9 Cmd_Config_Acc to commit */
    _wr(self, QMI8658_REG_CTRL2, QMI8658_ACC_FS_4G | QMI8658_ACC_ODR_125Hz);
    _ctrl9_cmd(self, 0x01);   /* Ctrl_Cmd_Config_Acc */

    /* CTRL3: Gyro — ±512 dps, 112 Hz; then CTRL9 Cmd_Config_Gyro to commit */
    _wr(self, QMI8658_REG_CTRL3, QMI8658_GYR_FS_512DPS | QMI8658_GYR_ODR_112Hz);
    _ctrl9_cmd(self, 0x02);   /* Ctrl_Cmd_Config_Gyro */

    /* CTRL5: LPF disabled */
    _wr(self, QMI8658_REG_CTRL5, 0x00);

    /* CTRL6: AttitudeEngine divider — 0x00 = disabled, raw sensor output */
    _wr(self, QMI8658_REG_CTRL6, 0x00);

    /* CTRL7: enable accel + gyro; CTRL9 NoCMD to start the sampling pipeline */
    _wr(self, QMI8658_REG_CTRL7, QMI8658_CTRL7_ACC_EN | QMI8658_CTRL7_GYR_EN);
    _ctrl9_cmd(self, 0x00);   /* Ctrl_Cmd_NoCMD — commit CTRL7 */

    self->sleep_ms(50);   /* let sensors settle */
}

static int qmi8658_update(IMU_ObjectTypeDef *base)
{
    QMI8658ObjectTypeDef *self = (QMI8658ObjectTypeDef *)base;

    /* Poll STATUSINT.bit0 (AvailStatus) — set when new sample is ready AND INT1 is disabled.
     * Reading STATUSINT when AvailStatus=1 atomically latches output registers. */
    uint8_t sint = 0;
    for (int i = 0; i < 20; i++) {
        _rd(self, QMI8658_REG_STATUSINT, 1, &sint);
        if (sint & 0x01) break;
        self->sleep_ms(1);
    }
    /* Continue even if timed out */

    /* I2C auto-increment is broken on this chip — read each byte individually */
#define RD1(reg, var) do { uint8_t _b; if (_rd(self,(reg),1,&_b)!=0) return -1; (var)=_b; } while(0)
    uint8_t lo, hi;
    RD1(QMI8658_REG_AX_L,     lo); RD1(QMI8658_REG_AX_L + 1, hi);
    int16_t raw_ax = (int16_t)((hi << 8) | lo);
    RD1(QMI8658_REG_AY_L,     lo); RD1(QMI8658_REG_AY_L + 1, hi);
    int16_t raw_ay = (int16_t)((hi << 8) | lo);
    RD1(QMI8658_REG_AZ_L,     lo); RD1(QMI8658_REG_AZ_L + 1, hi);
    int16_t raw_az = (int16_t)((hi << 8) | lo);
    RD1(QMI8658_REG_GX_L,     lo); RD1(QMI8658_REG_GX_L + 1, hi);
    int16_t raw_gx = (int16_t)((hi << 8) | lo);
    RD1(QMI8658_REG_GY_L,     lo); RD1(QMI8658_REG_GY_L + 1, hi);
    int16_t raw_gy = (int16_t)((hi << 8) | lo);
    RD1(QMI8658_REG_GZ_L,     lo); RD1(QMI8658_REG_GZ_L + 1, hi);
    int16_t raw_gz = (int16_t)((hi << 8) | lo);
#undef RD1

    self->accel[0] = (float)raw_ax * QMI8658_ACC_SCALE;
    self->accel[1] = (float)raw_ay * QMI8658_ACC_SCALE;
    self->accel[2] = (float)raw_az * QMI8658_ACC_SCALE;

    self->gyro[0]  = (float)raw_gx * QMI8658_GYRO_SCALE;
    self->gyro[1]  = (float)raw_gy * QMI8658_GYRO_SCALE;
    self->gyro[2]  = (float)raw_gz * QMI8658_GYRO_SCALE;

    return 0;
}

static int qmi8658_get_accel(IMU_ObjectTypeDef *base, float *xyz)
{
    QMI8658ObjectTypeDef *self = (QMI8658ObjectTypeDef *)base;
    xyz[0] = self->accel[0];
    xyz[1] = self->accel[1];
    xyz[2] = self->accel[2];
    return 0;
}

static int qmi8658_get_gyro(IMU_ObjectTypeDef *base, float *xyz)
{
    QMI8658ObjectTypeDef *self = (QMI8658ObjectTypeDef *)base;
    xyz[0] = self->gyro[0];
    xyz[1] = self->gyro[1];
    xyz[2] = self->gyro[2];
    return 0;
}

/* Stubs for unused interface methods */
static void qmi8658_calibrat(IMU_ObjectTypeDef *base) { (void)base; }
static int  qmi8658_get_euler(IMU_ObjectTypeDef *base, float *rpy) { (void)base; (void)rpy; return -1; }
static int  qmi8658_get_quat (IMU_ObjectTypeDef *base, float *q)   { (void)base; (void)q;   return -1; }
static int  qmi8658_get_accel_axis(IMU_ObjectTypeDef *base, IMU_AxisEnum axis, float *v)
{
    QMI8658ObjectTypeDef *self = (QMI8658ObjectTypeDef *)base;
    if (axis >= IMU_AXIS_X && axis <= IMU_AXIS_Z) { *v = self->accel[axis]; return 0; }
    return -1;
}
static int  qmi8658_get_gyro_axis(IMU_ObjectTypeDef *base, IMU_AxisEnum axis, float *v)
{
    QMI8658ObjectTypeDef *self = (QMI8658ObjectTypeDef *)base;
    if (axis >= IMU_AXIS_X && axis <= IMU_AXIS_Z) { *v = self->gyro[axis]; return 0; }
    return -1;
}

/* ── Public init ─────────────────────────────────────────────────────────── */

void qmi8658_object_init(QMI8658ObjectTypeDef *self, uint8_t dev_addr)
{
    memset(self, 0, sizeof(*self));
    self->dev_addr          = dev_addr;

    /* vtable */
    self->base.reset        = qmi8658_reset;
    self->base.calibrat     = qmi8658_calibrat;
    self->base.update       = qmi8658_update;
    self->base.get_euler    = qmi8658_get_euler;
    self->base.get_quat     = qmi8658_get_quat;
    self->base.get_accel    = qmi8658_get_accel;
    self->base.get_gyro     = qmi8658_get_gyro;
    self->base.get_accel_axis = qmi8658_get_accel_axis;
    self->base.get_gyro_axis  = qmi8658_get_gyro_axis;
}

#include "global.h"
#include "lwmem_porting.h"
#include "packet_reports.h"
#include "global_conf.h"
#include "imu.h"
#include "imu_qmi8658.h"

IMU_ObjectTypeDef *imus[1];

static int  i2c_write_byte(QMI8658ObjectTypeDef *self, uint8_t reg, uint8_t data);
static int  i2c_read_bytes(QMI8658ObjectTypeDef *self, uint8_t reg, uint32_t length, uint8_t *buf);
static void DelayMs(uint32_t ms);

void imus_init(void)
{
    QMI8658ObjectTypeDef *qmi = LWMEM_CCM_MALLOC(sizeof(QMI8658ObjectTypeDef));
    qmi8658_object_init(qmi, QMI8658_DEV_ADDR_SA0_LOW);
    qmi->i2c_write_byte = i2c_write_byte;
    qmi->i2c_read_bytes = i2c_read_bytes;
    qmi->sleep_ms       = DelayMs;
    imus[0] = (IMU_ObjectTypeDef *)qmi;
}

#if ENABLE_IMU
/**
 * @brief IMU task entry — reads QMI8658 at ~100 Hz (polling), transmits accel+gyro over serial.
 */
void imu_task_entry(void *argument)
{
    imus_init();
    imus[0]->reset(imus[0]);

    QMI8658ObjectTypeDef *qmi = (QMI8658ObjectTypeDef *)imus[0];

    for (;;) {
        osDelay(10);   /* ~100 Hz polling */

        if (imus[0]->update(imus[0]) != 0) {
            continue;
        }

        float imu_data[6] = {
            qmi->accel[0], qmi->accel[1], qmi->accel[2],
            qmi->gyro[0],  qmi->gyro[1],  qmi->gyro[2],
        };
        packet_transmit(&packet_controller, PACKET_FUNC_IMU, imu_data, sizeof(imu_data));
    }
}
#endif

static int i2c_write_byte(QMI8658ObjectTypeDef *self, uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(&hi2c2, self->dev_addr << 1, reg,
                             I2C_MEMADD_SIZE_8BIT, &data, 1, 0xFF);
}

static int i2c_read_bytes(QMI8658ObjectTypeDef *self, uint8_t reg, uint32_t length, uint8_t *buf)
{
    return HAL_I2C_Mem_Read(&hi2c2, self->dev_addr << 1, reg,
                            I2C_MEMADD_SIZE_8BIT, buf, length, 0xFF);
}

static void DelayMs(uint32_t ms)
{
    osDelay(ms);
}

/**
 * @file app.c
 * @author Wu TongXing
 * @brief 主应用逻辑
 * @version 0.1
 * @date 2023-11-05
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "cmsis_os2.h"
#include "led.h"
#include "lwmem_porting.h"
#include "global.h"
#include "adc.h"
#include "pwm_servo.h"
#include "serial_servo.h"
#include "FlashSave_porting.h"
#include "motors_param.h"

#define PWM_SERVO_DEV_MAX  2200
#define PWM_SERVO_DEV_MIN  800

/* 蜂鸣器	的频率 */
#define BUZZER_FREQ				 1300

//底盘类型
uint32_t Chassis_run_type = CHASSIS_TYPE_JETAUTO;
uint8_t  save_flash = 0;
uint32_t buzzer_count = 0;

/* 硬件初始化声明 */
void buzzers_init(void);    //蜂鸣器
void buttons_init(void);	//按键
void motors_init(void);     //电机
void pwm_servos_init(void); //舵机
void chassis_init(void);    //小车参数初始化
void mecanum_control(void);




void send_type(ChassisTypeEnum chassis_type);


//小阿克曼车控制函数
void minacker_control(void);


/* 用户入口函数 */
void app_task_entry(void *argument)
{
    extern osTimerId_t buzzer_timerHandle;
    extern osTimerId_t battery_check_timerHandle;

    /* Diagnostic: FreeRTOS scheduler is running, app task started */
    {
        extern UART_HandleTypeDef huart3;
        uint8_t diag_task[] = "TASK\r\n";
        HAL_UART_Transmit(&huart3, diag_task, sizeof(diag_task) - 1, 1000);
    }

    /* packet_init must come first — it zeroes packet_controller, so any
       callback registered before this call would be wiped out */
    packet_init();

    motors_init();
    pwm_servos_init();
    buzzers_init();
    buttons_init();

    osTimerStart(buzzer_timerHandle, BUZZER_TASK_PERIOD);
    osTimerStart(battery_check_timerHandle, BATTERY_TASK_PERIOD);
    HAL_ADC_Start(&hadc1);

    chassis_init();
    set_chassis_type(CHASSIS_TYPE_JETAUTO);

    for (int i = 0; i < 4; i++) {
        set_motor_type(motors[i], MOTOR_TYPE_JGB37);
    }

    chassis->stop(chassis);

    packet_start_recv();

    /* Main loop: send encoder telemetry at 100 Hz + PID debug at 10 Hz */
    int dbg_div = 0;
    for (;;) {
        osDelay(10);
        int64_t enc[4] = {
            motors[0]->counter,
            motors[1]->counter,
            motors[2]->counter,
            motors[3]->counter,
        };
        packet_transmit(&packet_controller, PACKET_FUNC_ENCODER, enc, sizeof(enc));

        /* Every 10th cycle (100 ms): send set_point + actual rps for all 4 motors */
        if (++dbg_div >= 10) {
            dbg_div = 0;
            float dbg[8] = {
                motors[0]->pid_controller.set_point, motors[1]->pid_controller.set_point,
                motors[2]->pid_controller.set_point, motors[3]->pid_controller.set_point,
                motors[0]->rps, motors[1]->rps, motors[2]->rps, motors[3]->rps,
            };
            packet_transmit(&packet_controller, 0x0B, dbg, sizeof(dbg));
        }
    }
}

/* 
*  大阿克曼底盘控制函数
*/
void mecanum_control(void)
{
    //定义电机运动速度
    //建议范围为 [50 , 450]
    static float speed = 300.0f;  


	//以x轴线速度运动（即向前运动）
	chassis->set_velocity(chassis, -200.0, 0, 0);
	osDelay(2000); //延时2s

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s
	
	//以x轴线速度运动（即向后运动）
	chassis->set_velocity(chassis, 200.0, 0, 0);
	osDelay(2000); //延时2s	

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s
	

	//以y轴线速度运动（即向左运动）
	chassis->set_velocity(chassis, 0, -200.0, 0);
	osDelay(2000); //延时2s

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s

	chassis->set_velocity(chassis, 0, 200.0, 0);
	osDelay(2000); //延时2s	

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s


	//角速度运动 即原地向左运动
	chassis->set_velocity(chassis, 0, 0, 0.5);
	osDelay(2000); //延时2s

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s
	
	//角速度运动 即原地向右运动
	chassis->set_velocity(chassis, 0, 0, -0.5);
	osDelay(2000); //延时2s	

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s


	//斜向运动 斜向左前运动
	chassis->set_velocity(chassis, -100.0, -100.0, 0);
	osDelay(2000); //延时2s

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s

	//斜向运动 斜向右后运动
	chassis->set_velocity(chassis, 100.0, 100.0, 0);
	osDelay(2000); //延时2s	

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s


	//漂移运动 向左边漂移
	chassis->drift(chassis, true);
	osDelay(2000); //延时2s	

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s

	//漂移运动 向右边漂移
	chassis->drift(chassis, false);
	osDelay(2000); //延时2s

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s
	

	/*//向左前方线性运动
	chassis->set_velocity(chassis, 200.0, 0, -0.3);
	osDelay(2000); //延时2s

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s

	//向右后方线性运动
	chassis->set_velocity(chassis, -200.0, 0, 0.3);
	osDelay(2000); //延时2s	

	chassis->stop(chassis); //停止
	osDelay(1000); //延时1s*/


}





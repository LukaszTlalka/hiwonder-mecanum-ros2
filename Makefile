##########################################################################
# Makefile for RosRobotControllerM4 (mecanum 8V)
# Target: STM32F407VET6, GCC toolchain
##########################################################################

TARGET = RosRobotControllerM4

# Toolchain (use CubeIDE bundled GCC)
GCC_PATH = /home/lukas/st/stm32cubeide_2.0.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.linux64_1.0.100.202602081740/tools/bin
CC  = $(GCC_PATH)/arm-none-eabi-gcc
CXX = $(GCC_PATH)/arm-none-eabi-g++
AS  = $(GCC_PATH)/arm-none-eabi-gcc -x assembler-with-cpp
CP  = $(GCC_PATH)/arm-none-eabi-objcopy
SZ  = $(GCC_PATH)/arm-none-eabi-size

HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

BUILD_DIR = build

# Linker script
LDSCRIPT = STM32F407VETx_FLASH.ld

# CPU flags
CPU   = -mcpu=cortex-m4
FPU   = -mfpu=fpv4-sp-d16
FLOAT = -mfloat-abi=hard
MCU   = $(CPU) -mthumb $(FPU) $(FLOAT)

# Compile flags
ASFLAGS  = $(MCU) -Wall -fdata-sections -ffunction-sections
CFLAGS   = $(MCU) -Wall -Wextra -Wno-unused-parameter \
           -Wno-int-conversion -Wno-implicit-function-declaration \
           -fdata-sections -ffunction-sections \
           -Os -g3 \
           $(DEFS) $(INCS)

# Linker flags
LDFLAGS  = $(MCU) -specs=nano.specs -T$(LDSCRIPT) \
           -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
           -Wl,--gc-sections \
           -lc -lm -lnosys

# Preprocessor defines
DEFS = \
  -DUSE_HAL_DRIVER \
  -DSTM32F407xx \
  -DLV_CONF_INCLUDE_SIMPLE \
  -DLV_LVGL_H_INCLUDE_SIMPLE

# Include paths
INCS = \
  -ICore/Inc \
  -IDrivers/STM32F4xx_HAL_Driver/Inc \
  -IDrivers/STM32F4xx_HAL_Driver/Inc/Legacy \
  -IMiddlewares/Third_Party/FreeRTOS/Source/include \
  -IMiddlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
  -IMiddlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F \
  -IMiddlewares/ST/STM32_USB_Host_Library/Class/HID/Inc \
  -IMiddlewares/ST/STM32_USB_Host_Library/Core/Inc \
  -IDrivers/CMSIS/Device/ST/STM32F4xx/Include \
  -IDrivers/CMSIS/Include \
  -IUSB_HOST/Target \
  -IUSB_HOST/App \
  -IThird_Party/Fusion/Fusion \
  -IThird_Party/LVGL/porting \
  -IThird_Party/LVGL/src \
  -IThird_Party/LVGL \
  -IThird_Party/U8g2 \
  -IThird_Party/RTT \
  -IThird_Party/Lw \
  -IThird_Party \
  -IHiwonder/LVGL_UI/guider_customer_fonts \
  -IHiwonder/LVGL_UI/guider_fonts \
  -IHiwonder/LVGL_UI/images \
  -IHiwonder/LVGL_UI \
  -IHiwonder/USB_HOST \
  -IHiwonder/Peripherals \
  -IHiwonder/Portings \
  -IHiwonder/Chassis \
  -IHiwonder/System \
  -IHiwonder/Misc

# Assembly sources
ASM_SOURCES = \
  Core/Startup/startup_stm32f407vetx.s

# C sources
C_SOURCES = \
  Core/Src/main.c \
  Core/Src/gpio.c \
  Core/Src/freertos.c \
  Core/Src/adc.c \
  Core/Src/crc.c \
  Core/Src/dma.c \
  Core/Src/i2c.c \
  Core/Src/spi.c \
  Core/Src/tim.c \
  Core/Src/usart.c \
  Core/Src/stm32f4xx_it.c \
  Core/Src/stm32f4xx_hal_msp.c \
  Core/Src/stm32f4xx_hal_timebase_tim.c \
  Core/Src/system_stm32f4xx.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_hcd.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_adc.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_crc.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c \
  Middlewares/Third_Party/FreeRTOS/Source/croutine.c \
  Middlewares/Third_Party/FreeRTOS/Source/event_groups.c \
  Middlewares/Third_Party/FreeRTOS/Source/list.c \
  Middlewares/Third_Party/FreeRTOS/Source/queue.c \
  Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c \
  Middlewares/Third_Party/FreeRTOS/Source/tasks.c \
  Middlewares/Third_Party/FreeRTOS/Source/timers.c \
  Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c \
  Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c \
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c \
  Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_core.c \
  Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_ctlreq.c \
  Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_ioreq.c \
  Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_pipes.c \
  Middlewares/ST/STM32_USB_Host_Library/Class/HID/Src/usbh_hid.c \
  Middlewares/ST/STM32_USB_Host_Library/Class/HID/Src/usbh_hid_keybd.c \
  Middlewares/ST/STM32_USB_Host_Library/Class/HID/Src/usbh_hid_mouse.c \
  Middlewares/ST/STM32_USB_Host_Library/Class/HID/Src/usbh_hid_parser.c \
  USB_HOST/Target/usbh_conf.c \
  USB_HOST/App/usb_host.c \
  Third_Party/RTT/SEGGER_RTT.c \
  Third_Party/RTT/SEGGER_RTT_printf.c \
  Third_Party/Fusion/Fusion/FusionAhrs.c \
  Third_Party/Fusion/Fusion/FusionCompass.c \
  Third_Party/Fusion/Fusion/FusionOffset.c \
  Third_Party/Lw/lwmem.c \
  Third_Party/Lw/lwmem_sys_cmsis_os.c \
  Third_Party/Lw/lwpkt.c \
  Third_Party/Lw/lwrb.c \
  Third_Party/Lw/lwshell.c \
  Third_Party/U8g2/mui.c \
  Third_Party/U8g2/mui_u8g2.c \
  Third_Party/U8g2/u8g2_bitmap.c \
  Third_Party/U8g2/u8g2_box.c \
  Third_Party/U8g2/u8g2_buffer.c \
  Third_Party/U8g2/u8g2_button.c \
  Third_Party/U8g2/u8g2_circle.c \
  Third_Party/U8g2/u8g2_cleardisplay.c \
  Third_Party/U8g2/u8g2_d_memory.c \
  Third_Party/U8g2/u8g2_d_setup.c \
  Third_Party/U8g2/u8g2_font.c \
  Third_Party/U8g2/u8g2_fonts.c \
  Third_Party/U8g2/u8g2_hvline.c \
  Third_Party/U8g2/u8g2_input_value.c \
  Third_Party/U8g2/u8g2_intersection.c \
  Third_Party/U8g2/u8g2_kerning.c \
  Third_Party/U8g2/u8g2_line.c \
  Third_Party/U8g2/u8g2_ll_hvline.c \
  Third_Party/U8g2/u8g2_message.c \
  Third_Party/U8g2/u8g2_polygon.c \
  Third_Party/U8g2/u8g2_selection_list.c \
  Third_Party/U8g2/u8g2_setup.c \
  Third_Party/U8g2/u8log.c \
  Third_Party/U8g2/u8log_u8g2.c \
  Third_Party/U8g2/u8log_u8x8.c \
  Third_Party/U8g2/u8x8_8x8.c \
  Third_Party/U8g2/u8x8_byte.c \
  Third_Party/U8g2/u8x8_cad.c \
  Third_Party/U8g2/u8x8_capture.c \
  Third_Party/U8g2/u8x8_d_ssd1306_128x32.c \
  Third_Party/U8g2/u8x8_d_ssd1306_128x64_noname.c \
  Third_Party/U8g2/u8x8_d_stdio.c \
  Third_Party/U8g2/u8x8_debounce.c \
  Third_Party/U8g2/u8x8_display.c \
  Third_Party/U8g2/u8x8_fonts.c \
  Third_Party/U8g2/u8x8_gpio.c \
  Third_Party/U8g2/u8x8_input_value.c \
  Third_Party/U8g2/u8x8_message.c \
  Third_Party/U8g2/u8x8_selection_list.c \
  Third_Party/U8g2/u8x8_setup.c \
  Third_Party/U8g2/u8x8_string.c \
  Third_Party/U8g2/u8x8_u8toa.c \
  Third_Party/U8g2/u8x8_u16toa.c \
  Third_Party/LVGL/src/lv_core/lv_disp.c \
  Third_Party/LVGL/src/lv_core/lv_group.c \
  Third_Party/LVGL/src/lv_core/lv_indev.c \
  Third_Party/LVGL/src/lv_core/lv_obj.c \
  Third_Party/LVGL/src/lv_core/lv_refr.c \
  Third_Party/LVGL/src/lv_core/lv_style.c \
  Third_Party/LVGL/src/lv_draw/lv_draw_arc.c \
  Third_Party/LVGL/src/lv_draw/lv_draw_blend.c \
  Third_Party/LVGL/src/lv_draw/lv_draw_img.c \
  Third_Party/LVGL/src/lv_draw/lv_draw_label.c \
  Third_Party/LVGL/src/lv_draw/lv_draw_line.c \
  Third_Party/LVGL/src/lv_draw/lv_draw_mask.c \
  Third_Party/LVGL/src/lv_draw/lv_draw_rect.c \
  Third_Party/LVGL/src/lv_draw/lv_draw_triangle.c \
  Third_Party/LVGL/src/lv_draw/lv_img_buf.c \
  Third_Party/LVGL/src/lv_draw/lv_img_cache.c \
  Third_Party/LVGL/src/lv_draw/lv_img_decoder.c \
  Third_Party/LVGL/src/lv_font/lv_font.c \
  Third_Party/LVGL/src/lv_font/lv_font_dejavu_16_persian_hebrew.c \
  Third_Party/LVGL/src/lv_font/lv_font_fmt_txt.c \
  Third_Party/LVGL/src/lv_font/lv_font_loader.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_8.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_10.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_12.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_12_subpx.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_14.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_16.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_18.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_20.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_22.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_24.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_26.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_28.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_28_compressed.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_30.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_32.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_34.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_36.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_38.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_40.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_42.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_44.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_46.c \
  Third_Party/LVGL/src/lv_font/lv_font_montserrat_48.c \
  Third_Party/LVGL/src/lv_font/lv_font_simsun_16_cjk.c \
  Third_Party/LVGL/src/lv_font/lv_font_unscii_8.c \
  Third_Party/LVGL/src/lv_font/lv_font_unscii_16.c \
  Third_Party/LVGL/src/lv_hal/lv_hal_disp.c \
  Third_Party/LVGL/src/lv_hal/lv_hal_indev.c \
  Third_Party/LVGL/src/lv_hal/lv_hal_tick.c \
  Third_Party/LVGL/src/lv_misc/lv_anim.c \
  Third_Party/LVGL/src/lv_misc/lv_area.c \
  Third_Party/LVGL/src/lv_misc/lv_async.c \
  Third_Party/LVGL/src/lv_misc/lv_bidi.c \
  Third_Party/LVGL/src/lv_misc/lv_color.c \
  Third_Party/LVGL/src/lv_misc/lv_debug.c \
  Third_Party/LVGL/src/lv_misc/lv_fs.c \
  Third_Party/LVGL/src/lv_misc/lv_gc.c \
  Third_Party/LVGL/src/lv_misc/lv_ll.c \
  Third_Party/LVGL/src/lv_misc/lv_log.c \
  Third_Party/LVGL/src/lv_misc/lv_math.c \
  Third_Party/LVGL/src/lv_misc/lv_mem.c \
  Third_Party/LVGL/src/lv_misc/lv_printf.c \
  Third_Party/LVGL/src/lv_misc/lv_task.c \
  Third_Party/LVGL/src/lv_misc/lv_templ.c \
  Third_Party/LVGL/src/lv_misc/lv_txt.c \
  Third_Party/LVGL/src/lv_misc/lv_txt_ap.c \
  Third_Party/LVGL/src/lv_misc/lv_utils.c \
  Third_Party/LVGL/src/lv_themes/lv_theme.c \
  Third_Party/LVGL/src/lv_themes/lv_theme_empty.c \
  Third_Party/LVGL/src/lv_themes/lv_theme_material.c \
  Third_Party/LVGL/src/lv_themes/lv_theme_mono.c \
  Third_Party/LVGL/src/lv_themes/lv_theme_template.c \
  Third_Party/LVGL/src/lv_widgets/lv_arc.c \
  Third_Party/LVGL/src/lv_widgets/lv_bar.c \
  Third_Party/LVGL/src/lv_widgets/lv_btn.c \
  Third_Party/LVGL/src/lv_widgets/lv_btnmatrix.c \
  Third_Party/LVGL/src/lv_widgets/lv_calendar.c \
  Third_Party/LVGL/src/lv_widgets/lv_canvas.c \
  Third_Party/LVGL/src/lv_widgets/lv_chart.c \
  Third_Party/LVGL/src/lv_widgets/lv_checkbox.c \
  Third_Party/LVGL/src/lv_widgets/lv_cont.c \
  Third_Party/LVGL/src/lv_widgets/lv_cpicker.c \
  Third_Party/LVGL/src/lv_widgets/lv_dropdown.c \
  Third_Party/LVGL/src/lv_widgets/lv_gauge.c \
  Third_Party/LVGL/src/lv_widgets/lv_img.c \
  Third_Party/LVGL/src/lv_widgets/lv_imgbtn.c \
  Third_Party/LVGL/src/lv_widgets/lv_keyboard.c \
  Third_Party/LVGL/src/lv_widgets/lv_label.c \
  Third_Party/LVGL/src/lv_widgets/lv_led.c \
  Third_Party/LVGL/src/lv_widgets/lv_line.c \
  Third_Party/LVGL/src/lv_widgets/lv_linemeter.c \
  Third_Party/LVGL/src/lv_widgets/lv_list.c \
  Third_Party/LVGL/src/lv_widgets/lv_msgbox.c \
  Third_Party/LVGL/src/lv_widgets/lv_objmask.c \
  Third_Party/LVGL/src/lv_widgets/lv_objx_templ.c \
  Third_Party/LVGL/src/lv_widgets/lv_page.c \
  Third_Party/LVGL/src/lv_widgets/lv_roller.c \
  Third_Party/LVGL/src/lv_widgets/lv_slider.c \
  Third_Party/LVGL/src/lv_widgets/lv_spinbox.c \
  Third_Party/LVGL/src/lv_widgets/lv_spinner.c \
  Third_Party/LVGL/src/lv_widgets/lv_switch.c \
  Third_Party/LVGL/src/lv_widgets/lv_table.c \
  Third_Party/LVGL/src/lv_widgets/lv_tabview.c \
  Third_Party/LVGL/src/lv_widgets/lv_textarea.c \
  Third_Party/LVGL/src/lv_widgets/lv_tileview.c \
  Third_Party/LVGL/src/lv_widgets/lv_win.c \
  Third_Party/LVGL/porting/lv_port_disp.c \
  Third_Party/LVGL/porting/lv_port_fs.c \
  Third_Party/LVGL/porting/lv_port_indev.c \
  Hiwonder/USB_HOST/usbh_hid_gamepad.c \
  Hiwonder/USB_HOST/usbh_hiwonder_hid.c \
  Hiwonder/LVGL_UI/events_init.c \
  Hiwonder/LVGL_UI/gui_guider.c \
  Hiwonder/LVGL_UI/setup_scr_screen_empty.c \
  Hiwonder/LVGL_UI/setup_scr_screen_imu.c \
  Hiwonder/LVGL_UI/setup_scr_screen_ps2.c \
  Hiwonder/LVGL_UI/setup_scr_screen_sbus.c \
  Hiwonder/LVGL_UI/setup_scr_screen_startup.c \
  Hiwonder/LVGL_UI/setup_scr_screen_sys.c \
  Hiwonder/LVGL_UI/images/_logo18_148x18.c \
  Hiwonder/LVGL_UI/custom.c \
  Hiwonder/LVGL_UI/guider_fonts/lv_font_SourceHanSansSC_Normal_16.c \
  Hiwonder/LVGL_UI/guider_fonts/lv_font_ps2_icons_14.c \
  Hiwonder/LVGL_UI/guider_fonts/lv_font_SourceHanSansSC_Normal_12.c \
  Hiwonder/LVGL_UI/guider_fonts/lv_font_SourceHanSansSC_Light_12.c \
  Hiwonder/LVGL_UI/guider_fonts/lv_font_SourceHanSansSC_Normal_14.c \
  Hiwonder/Peripherals/button.c \
  Hiwonder/Peripherals/buzzer.c \
  Hiwonder/Peripherals/led.c \
  Hiwonder/Peripherals/display_st7735.c \
  Hiwonder/Peripherals/imu_mpu6050.c \
  Hiwonder/Peripherals/imu_qmi8658.c \
  Hiwonder/Peripherals/pwm_servo.c \
  Hiwonder/Peripherals/serial_servo.c \
  Hiwonder/Peripherals/encoder_motor.c \
  Hiwonder/Misc/checksum.c \
  Hiwonder/Misc/log.c \
  Hiwonder/Misc/packet.c \
  Hiwonder/Misc/pid.c \
  Hiwonder/Misc/sbus.c \
  Hiwonder/Misc/syscall.c \
  Hiwonder/Chassis/differential_chassis.c \
  Hiwonder/Chassis/mecanum_chassis.c \
  Hiwonder/Chassis/ackermann_chassis.c \
  Hiwonder/Portings/led_porting.c \
  Hiwonder/Portings/button_porting.c \
  Hiwonder/Portings/buzzer_porting.c \
  Hiwonder/Portings/imu_porting.c \
  Hiwonder/Portings/lcd_porting.c \
  Hiwonder/Portings/motor_porting.c \
  Hiwonder/Portings/packet_porting.c \
  Hiwonder/Portings/pwm_servo_porting.c \
  Hiwonder/Portings/sbus_porting.c \
  Hiwonder/Portings/bluetooth_porting.c \
  Hiwonder/Portings/lwmem_porting.c \
  Hiwonder/Portings/chassis_porting.c \
  Hiwonder/Portings/u8g2_porting.c \
  Hiwonder/Portings/FlashSave_porting.c \
  Hiwonder/System/battery_handle.c \
  Hiwonder/System/gampad_handle.c \
  Hiwonder/System/gui_handle.c \
  Hiwonder/System/app.c

##########################################################################

OBJECTS  = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

ASM_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o $@ $<

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(ASM_OBJECTS) Makefile
	$(CC) $(OBJECTS) $(ASM_OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	-rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean

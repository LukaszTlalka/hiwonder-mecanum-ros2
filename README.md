# hiwonder-mecanum-ros2

STM32F4 firmware and Python web dashboard for the Hiwonder mecanum wheel robot controller.

**Hardware:** STM32F407VETx (Cortex-M4) · QMI8658 IMU · 4× DC motors with encoders · FreeRTOS

---

## Repository structure

```
Core/           STM32CubeMX-generated HAL init and FreeRTOS config
Hiwonder/       Application code — motors, IMU, chassis, packet protocol
Drivers/        STM32 HAL + CMSIS
Middlewares/    FreeRTOS, USB
Third_Party/    LVGL, LwMEM, Fusion, RTT, U8g2
web_dashboard.py  Browser-based dashboard (Python/Flask)
```

---

## Build

Requires the [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) GCC toolchain. Update `GCC_PATH` in the `Makefile` to match your installation, then:

```bash
make -j$(nproc)
```

Output: `build/RosRobotControllerM4.bin`

---

## Flash

Requires `stm32flash`:

```bash
sudo apt install stm32flash
```

### 1. Enter bootloader mode

1. Hold **BOOT0**
2. Press and release **RESET**
3. Keep holding **BOOT0** until flash starts

### 2. Flash

```bash
stm32flash -w build/RosRobotControllerM4.bin -v -g 0x08000000 /dev/ttyACM1
```

Adjust `/dev/ttyACM1` to match your serial port. Default baud rate: 57600.

---

## Web dashboard

Requires Python 3 and Flask:

```bash
pip install flask pyserial
python web_dashboard.py
```

Open `http://localhost:5000` in a browser. The dashboard displays live IMU data, motor encoder counts, battery voltage, and allows sending drive commands.

---

## Serial packet protocol

All communication uses a simple framed protocol over UART at 1 Mbaud:

```
AA 55 FUNC LEN [DATA...] CRC8
```

| FUNC | Description |
|------|-------------|
| 0x00 | SYS (battery, diagnostics) |
| 0x03 | Motor setpoints |
| 0x07 | IMU (6× float: ax ay az gx gy gz) |
| 0x0A | Encoder counts (4× int64) |
| 0x0B | PID debug (setpoints + RPS) |

---

## IMU

The QMI8658 6-DOF IMU (I2C address `0x6A`) is configured for:
- Accelerometer: ±4g, 125 Hz
- Gyroscope: ±512 dps, 112 Hz

> **Note:** The QMI8658 soft reset (`0xB0` to reg `0x60`) locks up the I2C bus on this board. The driver disables sensors via CTRL7 instead of issuing a soft reset.

# RosRobotControllerM4
##作者：CuZn

## Build

Requires the STM32CubeIDE GCC toolchain installed at the path in the Makefile (`GCC_PATH`).

```bash
cd /home/lukas/Downloads/RosRobotControllerM4_mecanum_8V
make -j$(nproc)
```

Output: `build/RosRobotControllerM4.hex`

## Flash to Board

Requires `stm32flash` installed (`sudo apt install stm32flash`).

### 1. Enter bootloader mode

1. Hold the **BOOT0** button
2. Press and release **RESET**
3. Release **BOOT0**

### 2. Flash

```bash
stm32flash -b 115200 -w build/RosRobotControllerM4.hex -v -R /dev/ttyACM1
```

- `-v` verifies the write
- `-R` resets the board into user code after flashing
- Device: `STM32F407VET6` (ID `0x0413`), port `/dev/ttyACM1`

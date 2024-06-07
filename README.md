# mspm0flash

Linux tool to program a TI MSPM0 microcontroller. The tool programs and
verifies the device memory through the I2C or UART interface.
It communicates with the controller through the BSL (Bootstrap Loader).


https://www.ti.com/lit/pdf/slau887

https://www.ti.com/lit/pdf/slaae88


## Usage

    Usage: ./mspm0flash [options] <CMD> <fw-bin-file>

      Flash and verify firmware binary to a TI MSPM0L microcontroller.

      Options:
      -a, --address ADDR      Using given I2C_ADDRESS for communication
                              (default 0x48)

      -b, --baud RATE         Using given baudrate for communication
                              (default 9600)

      -I, --i2c  DEVICE       Using given I2C DEVICE for communication.

      -S, --serail  DEVICE    Using given serial DEVICE for communication.

      -n, --no-script         Do not execute init/exit script.

      -s, --dont-start        Do not start the application after programming.

      -v, --verbose           Increase verbosity, can be set multiple times.

      -V, --version           Display program version and exit.

      -h, --help              Display this help and exit.

      CMD:
        prog <fw-bin-file>   Program the firmware data.
        info                 Display the device info.
        erase                Erase the full flash.

### Program

The controller can be programmed though an I2C or UART interface.

    mspm0flash -S /dev/ttyUSB0 -b 115200 -s -n prog <fw-bin-file>

    mspm0flash -I /dev/i2c-8 -s -n prog <fw-bin-file>


## Script

The script interface is used to prepare and exit the BSL.

The script has to be located under `/etc/mspm0flash/ctrl`.
Alternative the script location can be override by setting the environment
variable `MSPM0FLASH_CTRL`.

### Example

    #!/bin/sh

    programming_mode() {
        # reset
        # set invoke pin
        # reset
    }

    normal_mode() {
        # release invoke pin
        # reset
    }

    case "$1" in
    	init)
    		programming_mode
    		;;
    	exit)
    		normal_mode
    		;;
    esac

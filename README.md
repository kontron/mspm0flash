# mspm0flash

Linux tool to program a TI MSPM0 microcontroller. The tool programs and verifies the
device memory through the I2C interface. It communicates with the controller
through the BSL.

## Script

The script interface is used to prepare and exit the BSL.

### Example

    #!/bin/sh

    programming_mode() {
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

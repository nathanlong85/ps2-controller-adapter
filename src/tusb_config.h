#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#define CFG_TUSB_MCU OPT_MCU_RP2040
#define CFG_TUSB_OS OPT_OS_PICO
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_HOST

#define BOARD_TUH_RHPORT 0

#define CFG_TUH_HID 4
// This hub product enumerates as TWO hub-class devices (a second internal
// hub chip alongside the main 4-port hub) - 1 slot isn't enough even with
// nothing else plugged in. See enum_request_set_addr assert investigation.
#define CFG_TUH_HUB 2
#define CFG_TUH_DEVICE_MAX 4

#endif

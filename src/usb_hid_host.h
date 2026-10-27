#ifndef USB_HID_HOST_H
#define USB_HID_HOST_H

#include "guitar_input.h"

// Returns the latest normalized guitar input state, decoded from the most
// recently received HID report. Idle if the controller has never mounted,
// is currently disconnected, or its last report was malformed.
void usb_hid_host_get_guitar_input(guitar_input_t *dst);

#endif

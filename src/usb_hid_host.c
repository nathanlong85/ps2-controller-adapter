#include "tusb.h"
#include <stdio.h>

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
    printf("HID mounted: dev_addr=%u instance=%u desc_len=%u\n", dev_addr, instance, desc_len);
    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    printf("HID unmounted: dev_addr=%u instance=%u\n", dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    printf("HID report: dev_addr=%u instance=%u len=%u bytes=", dev_addr, instance, len);
    for (uint16_t i = 0; i < len; i++) {
        printf("%02X ", report[i]);
    }
    printf("\n");
    tuh_hid_receive_report(dev_addr, instance);
}

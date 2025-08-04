/*
 * pssdunlock.c
 *
 *  Created on: 2023-06-23 09:15
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libusb-1.0/libusb.h>

#define EP_DATA_IN  0x02
#define EP_DATA_OUT 0x81

#define VID_SAMSUNG   0x04e8

#define PID_T1_NORMAL 0x61f1
#define PID_T1_LOCKED 0x61f2

#define PID_T3_NORMAL 0x61f3
#define PID_T3_LOCKED 0x61f4

#define PID_T5_NORMAL 0x61f5
#define PID_T5_LOCKED 0x61f6

#define PID_T5_NORMAL 0x61f5
#define PID_T5_LOCKED 0x61f6

/*
 * These are appear to be correct for the T7 but after relink the device is not accessible
 * #define PID_T7_NORMAL 0x4001
 * #define PID_T7_LOCKED 0x4002
 */

uint8_t payload_unlock[31] = {
    0x55, 0x53, 0x42, 0x43,
    0x0a, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x10,
    0x85, 0x0a, 0x26, 0x00, 0xd6, 0x00, 0x01, 0x00,
    0xc6, 0x00, 0x4f, 0x00, 0xc2, 0x00, 0xb0, 0x00
};

uint8_t payload_relink[31] = {
    0x55, 0x53, 0x42, 0x43,
    0x0b, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x06,
    0xe8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

uint8_t payload_passwd[512] = {0};
uint8_t payload_return[512] = {0};

int main(int argc, char **argv)
{
    int transferred = 0;
    uint16_t pid_normal = 0;
    uint16_t pid_locked = 0;
    struct libusb_device_handle *devh = NULL;

    // Check arguments
    if (argc != 2 && argc != 3) {
        printf("Usage: %s <device> [password]\n", argv[0]);
        printf("\n");
        printf("Options:\n");
        printf("  <device>      Select device model: t1, t3, t5\n");
        printf("  [password]    Unlock password (Optional)\n");
        exit(1);
    }

    // Select device
    if (!strcmp(argv[1], "t1")) {
        pid_normal = PID_T1_NORMAL;
        pid_locked = PID_T1_LOCKED;
    } else if (!strcmp(argv[1], "t3")) {
        pid_normal = PID_T3_NORMAL;
        pid_locked = PID_T3_LOCKED;
    } else if (!strcmp(argv[1], "t5")) {
        pid_normal = PID_T5_NORMAL;
        pid_locked = PID_T5_LOCKED;
//   } else if (!strcmp(argv[1], "t7")) {
//        pid_normal = PID_T7_NORMAL;
//        pid_locked = PID_T7_LOCKED;
    } else {
        fprintf(stderr, "Unknown device: %s\n", argv[1]);
        exit(1);
    }

    // Initialize libusb
    int rc = libusb_init(NULL);
    if (rc < 0) {
        fprintf(stderr, "Error initializing libusb: %s\n", libusb_error_name(rc));
        exit(1);
    }

    // Find device
    devh = libusb_open_device_with_vid_pid(NULL, VID_SAMSUNG, pid_locked);
    if (!devh) {
        devh = libusb_open_device_with_vid_pid(NULL, VID_SAMSUNG, pid_normal);
        if (!devh) {
            fprintf(stderr, "No %s device found\n", argv[1]);
        } else {
            fprintf(stderr, "Device is unlocked\n", argv[1]);
        }
        goto out;
    }

    // Get password
    if (argc == 2) {
        printf("Password: ");
        fgets(payload_passwd, sizeof(payload_passwd) - 1, stdin);
        payload_passwd[strlen(payload_passwd) - 1] = '\0';
    } else {
        strncpy((char *)payload_passwd, argv[2], sizeof(payload_passwd) - 1);
    }

    // Reset device
    rc = libusb_reset_device(devh);
    if (rc < 0) {
        fprintf(stderr, "Error resetting device: %s\n", libusb_error_name(rc));
        goto out;
    }

    // Detach kernel driver
    rc = libusb_detach_kernel_driver(devh, 0);
    if (rc < 0) {
        fprintf(stderr, "Error detaching kernel driver: %s\n", libusb_error_name(rc));
        goto out;
    }

    // Claim interface
    rc = libusb_claim_interface(devh, 0);
    if (rc < 0) {
        fprintf(stderr, "Error claiming interface: %s\n", libusb_error_name(rc));
        goto out;
    }

    // Send unlock command
    libusb_bulk_transfer(devh, EP_DATA_IN,  payload_unlock, sizeof(payload_unlock), &transferred, 3000);
    libusb_bulk_transfer(devh, EP_DATA_IN,  payload_passwd, sizeof(payload_passwd), &transferred, 3000);
    libusb_bulk_transfer(devh, EP_DATA_OUT, payload_return, sizeof(payload_return), &transferred, 3000);

    if (payload_return[9] == 0x02) {
        printf("Unlock failed\n");
    } else {
        printf("Unlock successfuly\n");
    }

    // Send relink command
    libusb_bulk_transfer(devh, EP_DATA_IN,  payload_relink, sizeof(payload_relink), &transferred, 3000);
    libusb_bulk_transfer(devh, EP_DATA_OUT, payload_return, sizeof(payload_return), &transferred, 3000);

    if (payload_return[9] == 0x02) {
        printf("Relink failed\n");
    } else {
        printf("Relink successfuly\n");
    }

    // Release interface
    libusb_release_interface(devh, 0);
out:
    // Close device
    if (devh) {
        libusb_close(devh);
    }

    // Exit libusb
    libusb_exit(NULL);

    return rc;
}

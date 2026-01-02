/**
 * @file list_devices.c
 * @brief Example: List available network devices using C API
 *
 * 𓆓 Wadjet-Link — Restoring the complete picture of the automotive stream.
 *
 * This example demonstrates how to enumerate network interfaces that can
 * be used for packet capture with Wadjet.
 *
 * Build:
 *   gcc -o list_devices list_devices.c -lwadjet_c -L../../build
 *
 * Usage:
 *   ./list_devices
 */

#include <stdio.h>
#include <stdlib.h>
#include "wadjet_c.h"

int main(void) {
    int major, minor, patch;
    wadjet_version_components(&major, &minor, &patch);
    printf("Wadjet-Link v%d.%d.%d - Network Device Enumerator\n", major, minor, patch);
    printf("================================================\n\n");

    // Get list of devices
    wadjet_device_list_t list;
    wadjet_error_t err = wadjet_device_enumerate(&list);
    if (err != WADJET_OK) {
        fprintf(stderr, "Error enumerating devices: %s\n", wadjet_last_error());
        return 1;
    }

    size_t count = wadjet_device_list_count(list);
    printf("Found %zu network device(s):\n\n", count);

    for (size_t i = 0; i < count; i++) {
        wadjet_device_info_t info;
        if (wadjet_device_list_get(list, i, &info) == WADJET_OK) {
            printf("[%zu] %s\n", i + 1, info.name);
            if (info.description && info.description[0] != '\0') {
                printf("    Description: %s\n", info.description);
            }
            printf("    Flags:");
            if (info.is_up) printf(" UP");
            if (info.is_loopback) printf(" LOOPBACK");
            printf("\n\n");
        }
    }

    // Cleanup
    wadjet_device_list_destroy(list);
    
    printf("Tip: Use device name with wadjet_capture_create()\n");
    return 0;
}

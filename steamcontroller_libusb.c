//
// Created by deniska on 12/22/2018.
//

#include "include/sc.h"
#include "common.h"
#include "libusb.h"

// HID Class-Specific Requests values. See section 7.2 of the HID specifications
#define HID_GET_REPORT                0x01
#define HID_GET_IDLE                  0x02
#define HID_GET_PROTOCOL              0x03
#define HID_SET_REPORT                0x09
#define HID_SET_IDLE                  0x0A
#define HID_SET_PROTOCOL              0x0B
#define HID_REPORT_TYPE_INPUT         0x01
#define HID_REPORT_TYPE_OUTPUT        0x02
#define HID_REPORT_TYPE_FEATURE       0x03


struct SteamControllerDevice {
    char *deviceId;
    bool isWireless;
    libusb_device_handle *dev_handle;
};

struct SteamControllerDeviceEnum {
    char *deviceId;
    struct libusb_device_descriptor desc;
    struct SteamControllerDeviceEnum *next;
    libusb_device_handle *dev_handle;
};

SCAPI SteamControllerDeviceEnum *SteamController_EnumControllerDevices() {
    SteamControllerDeviceEnum *pEnum = NULL;
    libusb_device **devs;

    int cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0) {
        return pEnum;
    }
    int i = 0;
    libusb_device *dev;
    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            continue;
        }

        if (desc.idVendor != USB_VID_VALVE) {
            continue;
        }

        if (desc.idProduct != USB_PID_STEAMCONTROLLER_WIRED && desc.idProduct != USB_PID_STEAMCONTROLLER_WIRELESS) {
            continue;
        }

        libusb_device_handle* dev_handle = NULL;
        libusb_open(dev, &dev_handle);

        SteamControllerDeviceEnum *pNewEnum = malloc(sizeof(SteamControllerDeviceEnum));
        pNewEnum->dev_handle = dev_handle;
        pNewEnum->next = pEnum;

        char deviceId[255];
        memset(deviceId, 0, 255);
        printf("%04x:%04x", desc.idVendor, desc.idProduct);
        sprintf(deviceId, "%04x:%04x (bus %d, device %d)",
               desc.idVendor, desc.idProduct,
               libusb_get_bus_number(dev), libusb_get_device_address(dev));
        pNewEnum->deviceId = strdup(deviceId);

        pNewEnum->desc = desc;

        pEnum = pNewEnum;

    }
    // libusb_()
    libusb_free_device_list(devs, 1);

    return pEnum;
}

int print_configuration(struct libusb_device_handle *hDevice, struct libusb_config_descriptor *config)
{
    char *data;
    int index;

    data = (char *)malloc(512);
    memset(data, 0, 512);

    index = config->iConfiguration;

    libusb_get_string_descriptor_ascii(hDevice, index, data, 512);

    printf("\nInterface Descriptors: ");
    printf("\n\tNumber of Interfaces: %d", config->bNumInterfaces);
    printf("\n\tLength: %d", config->bLength);
    printf("\n\tDesc_Type: %d", config->bDescriptorType);
    printf("\n\tConfig_index: %d", config->iConfiguration);
    printf("\n\tTotal length: %lu", config->wTotalLength);
    printf("\n\tConfiguration Value: %d", config->bConfigurationValue);
    printf("\n\tConfiguration Attributes: %d", config->bmAttributes);
    printf("\n\tMaxPower(mA): %d\n", config->MaxPower);

    free(data);
    data = NULL;
    return 0;
}

SCAPI SteamControllerDevice * SteamController_Open(const SteamControllerDeviceEnum *pEnum) {
    if (!pEnum) {
        return NULL;
    }

    SteamControllerDevice *pDevice = malloc(sizeof(SteamControllerDevice));
    pDevice->isWireless = pEnum->desc.idProduct == USB_PID_STEAMCONTROLLER_WIRELESS;
    libusb_device *dev = libusb_get_device(pEnum->dev_handle);
    int r = libusb_open(dev, &pDevice->dev_handle);
    if (r < 0) {
        free(pDevice);
        return NULL;
    }
    struct libusb_config_descriptor *config;
    int ret_active = libusb_get_active_config_descriptor(dev, &config);

    print_configuration(pDevice->dev_handle, config);

    pDevice->deviceId = strdup(pEnum->deviceId);

    SteamController_Initialize(pDevice);
    return pDevice;
}

SCAPI void SteamController_Close(SteamControllerDevice *pDevice) {
    if (!pDevice)
        return;
    libusb_close(pDevice->dev_handle);

    free(pDevice->deviceId);
    free(pDevice);
}

bool SteamController_HIDSetFeatureReport(const SteamControllerDevice *pDevice, SteamController_HIDFeatureReport *pReport) {
    if (!pDevice || !pReport || !pDevice->dev_handle) {
        return false;
    }

    return true;
}

void libusb_transfer_cb_fn1(struct libusb_transfer *transfer) {
    int h = 0;
}

bool SteamController_HIDGetFeatureReport(const SteamControllerDevice *pDevice, SteamController_HIDFeatureReport *pReport) {
    if (!pDevice || !pReport || !pDevice->dev_handle) {
        return false;
    }

    return true;
}


SCAPI SteamControllerDeviceEnum *SteamController_NextControllerDevice(SteamControllerDeviceEnum *pCurrent) {
    if (!pCurrent)
        return NULL;

    SteamControllerDeviceEnum *pNext = pCurrent->next;
    if (pCurrent->dev_handle != NULL) {
        libusb_close(pCurrent->dev_handle);
    }
    free(pCurrent->deviceId);
    free(pCurrent);

    return pNext;
}

SCAPI int SteamController_Init() {
    int r = libusb_init(NULL);
    if (r < 0) {
        return r;
    }

    return 0;
}

SCAPI void SteamController_Exit() {
    libusb_exit(NULL);
}

bool SCAPI SteamController_IsWirelessDongle(const SteamControllerDevice *pDevice) {
    if (!pDevice)
        return false;
    return pDevice->isWireless;
}

const char *SteamController_GetId(const struct SteamControllerDevice *pDevice) {
    return pDevice->deviceId;
}


#if _WIN32

#include "include/sc.h"
#include "common.h"

#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <guiddef.h>

#include <stdio.h>

struct SteamControllerDevice {
	HANDLE      devHandle;
	char*		  guidStr;
	bool		  isWireless;
};

struct SteamControllerDeviceEnum {
	struct SteamControllerDeviceEnum *next;
	GUID  guid;
	SP_DEVICE_INTERFACE_DETAIL_DATA *pDevIntfDetailData;
	HIDD_ATTRIBUTES hidAttribs;
};

SCAPI SteamControllerDeviceEnum * SteamController_EnumControllerDevices() {
	GUID hidGuid;
	HidD_GetHidGuid(&hidGuid);

	HDEVINFO  devInfo;
	devInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_INTERFACEDEVICE | DIGCF_PRESENT);

	SP_DEVICE_INTERFACE_DATA devIntfData = { .cbSize = sizeof(SP_DEVICE_INTERFACE_DATA) };
	DWORD                    devIndex = 0;

	SteamControllerDeviceEnum *pEnum = NULL;

	while (SetupDiEnumDeviceInterfaces(devInfo, NULL, &hidGuid, devIndex++, &devIntfData)) {
		DWORD reqSize;

		SetupDiGetDeviceInterfaceDetail(devInfo, &devIntfData, NULL, 0, &reqSize, NULL);

		SP_DEVICE_INTERFACE_DETAIL_DATA *pDevIntfDetailData;
		pDevIntfDetailData = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(reqSize);
		pDevIntfDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if (!SetupDiGetDeviceInterfaceDetail(devInfo, &devIntfData, pDevIntfDetailData, reqSize, &reqSize, NULL)) {
			fprintf(stderr, "SetupDiGetDeviceInterfaceDetail failed. Last error: %08lx\n", GetLastError());
			free(pDevIntfDetailData);
			continue;
		}

		HANDLE devFile;
		devFile = CreateFile(
			pDevIntfDetailData->DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			0,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			0
		);
		if (devFile == INVALID_HANDLE_VALUE) {
			free(pDevIntfDetailData);
			continue;
		}

		HIDD_ATTRIBUTES hidAttribs = { .Size = sizeof(HIDD_ATTRIBUTES) };
		HidD_GetAttributes(devFile, &hidAttribs);
		CloseHandle(devFile);

		if (hidAttribs.VendorID != USB_VID_VALVE) {
			free(pDevIntfDetailData);
			continue;
		}

		if (hidAttribs.ProductID != USB_PID_STEAMCONTROLLER_WIRED && hidAttribs.ProductID != USB_PID_STEAMCONTROLLER_WIRELESS) {
			free(pDevIntfDetailData);
			continue;
		}

		SteamControllerDeviceEnum *pNewEnum = malloc(sizeof(SteamControllerDeviceEnum));
		pNewEnum->next = pEnum;
		pNewEnum->pDevIntfDetailData = pDevIntfDetailData;
		pNewEnum->guid = devIntfData.InterfaceClassGuid;
		pNewEnum->hidAttribs = hidAttribs;
		pEnum = pNewEnum;
	}

	SetupDiDestroyDeviceInfoList(devInfo);

	return pEnum;
}

SCAPI SteamControllerDeviceEnum * SteamController_NextControllerDevice(SteamControllerDeviceEnum *pCurrent) {
	if (!pCurrent)
		return NULL;

	SteamControllerDeviceEnum *pNext = pCurrent->next;

	free(pCurrent->pDevIntfDetailData);
	free(pCurrent);

	return pNext;
}

SCAPI SteamControllerDevice * SteamController_Open(const SteamControllerDeviceEnum *pEnum) {
	if (!pEnum)
		return NULL;

	SteamControllerDevice *pDevice = malloc(sizeof(SteamControllerDevice));
	pDevice->isWireless = pEnum->hidAttribs.ProductID == USB_PID_STEAMCONTROLLER_WIRELESS;
	pDevice->devHandle = CreateFile(
		pEnum->pDevIntfDetailData->DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		0,
		OPEN_EXISTING,
		0,
		NULL
	);

	char guidStr[256];
	GUID* guid = &pEnum->guid;
	sprintf(guidStr, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		(unsigned int)guid->Data1, guid->Data2, guid->Data3,
		guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
		guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
	pDevice->guidStr = strdup(guidStr);
	SteamController_Initialize(pDevice);

	return pDevice;
}

SCAPI void SteamController_Close(SteamControllerDevice *pDevice) {
	if (!pDevice) {
        return;
    }
	HANDLE handle = pDevice->devHandle;
	free(pDevice->guidStr);
	pDevice->guidStr = NULL;
	pDevice->devHandle = NULL;
	free(pDevice);
	CloseHandle(handle);
}

bool SteamController_HIDSetFeatureReport(const SteamControllerDevice *pDevice, SteamController_HIDFeatureReport *pReport) {
	if (!pDevice || !pReport || !pDevice->devHandle)
		return false;

	fprintf(stderr, "SteamController_HIDSetFeatureReport %02x\n", pReport->featureId);

	for (int i = 0; i < 50; i++) {
		bool ok = HidD_SetFeature(pDevice->devHandle, pReport, sizeof(SteamController_HIDFeatureReport));
		if (ok)
			return true;

		fprintf(stderr, "HidD_SetFeature failed. Last error: %08lx\n", GetLastError());
		Sleep(1);
	}

	return false;
}

bool SteamController_HIDGetFeatureReport(const SteamControllerDevice *pDevice, SteamController_HIDFeatureReport *pReport) {
	if (!pDevice || !pReport || !pDevice->devHandle)
		return false;

	uint8_t featureId = pReport->featureId;

	SteamController_HIDSetFeatureReport(pDevice, pReport);

	fprintf(stderr, "SteamController_HIDGetFeatureReport %02x\n", pReport->featureId);

	for (int i = 0; i < 50; i++) {
		bool ok = HidD_GetFeature(pDevice->devHandle, pReport, sizeof(SteamController_HIDFeatureReport));
		if (ok) {
			if (featureId == pReport->featureId)
				return true;
			continue;
		}

		fprintf(stderr, "HidD_SetFeature failed. Last error: %08lx\n", GetLastError());
		Sleep(1);
	}

	return false;
}

bool SCAPI SteamController_IsWirelessDongle(const SteamControllerDevice *pDevice) {
	if (!pDevice)
		return false;
	return pDevice->isWireless;
}

/**
 * Read the range of eventnts from the device.
 *
 * @param pController   Device to use.
 * @param pEvent        Where to store event data.
 *
 * @return The total number of successfully read events
 */
int SCAPI SteamController_ReadEvents(const SteamControllerDevice *pDevice, SteamControllerEvent **ppEvents, int total) {
	if (!pDevice) {
		return 0;
	}

	if (!ppEvents) {
		return 0;
	}

	size_t size = 65 * total;
	uint8_t* eventDataBuf = malloc(size);
	size_t len = SteamController_ReadRaw(pDevice, eventDataBuf, size);

	if (!len) {
		free(eventDataBuf);
		return 0;
	}

	uint8_t *eventData = eventDataBuf;

	int totalRead = 0;
	int i = 0;
	while (len > 0) {
		if (!*eventData) {
			eventData++;
		}
		else
		{
			break;
		}
		SteamControllerEvent event;
		uint8_t eventType = processEvent(&event, eventData, 64);
		if (eventType != 0) {
			*ppEvents[i] = event;
			i++;
			totalRead = i;
		}
		eventData += 64;
		len -= 65;
	}

	free(eventDataBuf);

	return totalRead;
}

SCAPI size_t SteamController_ReadRaw(const SteamControllerDevice *pDevice, uint8_t *buffer, size_t maxLen) {
	if (!pDevice)
		return 0;

	DWORD bytesRead = 0;
	BOOL isOk = ReadFile(pDevice->devHandle, buffer, maxLen, &bytesRead, NULL);
	if (!isOk) {
		DWORD dwLastError = GetLastError();
		if (ERROR_IO_PENDING != dwLastError) {
			return 0;
		}
	}

	return bytesRead & 0xff;
}

SCAPI const char* SteamController_GetId(const struct SteamControllerDevice *pDevice) {
	return pDevice->guidStr;
}

SCAPI int SteamController_Init() {
	return 0;
}

SCAPI void SteamController_Exit() {

}

#endif
//
// Created by deniska on 12/22/2018.
//
#include <stdio.h>
#include <string.h>


#include "include/sc.h"


int main() {
    int r = SteamController_Init();
    if (r < 0)
        return r;

    SteamControllerDeviceEnum *pEnum = SteamController_EnumControllerDevices();
    while (pEnum) {
        SteamControllerDevice *pDevice = SteamController_Open(pEnum);
        if (pDevice) {
            SteamController_Close(pDevice);
        }
        pEnum = SteamController_NextControllerDevice(pEnum);
    }
    SteamController_Exit();

    return 0;
}



// This file common for both projects

#pragma once

#include <stdint.h>
#include <string>

#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)

#define FILE_DEVICE_UNKNOWN 0x00000022
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0

#define IOCTL_IDD_ADD_MONITOR     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IDD_REMOVE_MONITOR  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)  
#define IOCTL_IDD_CHANGE_RES      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IDD_GET_INFO        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct MonitorConfig {
    uint16_t byteDepth;
    uint16_t width;
    uint16_t height;
    uint16_t refreshRate;
    uint16_t monitorId;
    bool enabled;
};

struct DriverInfo {
    uint32_t monitorCount;
    uint32_t maxMonitors;
    uint32_t driverVersion;
};

struct CreateMonitorRequest {
    MonitorConfig config;
    WCHAR frameReadyName[128] = {}; // Драйвер -> Приложение: "новый кадр готов"
    WCHAR frameProcessedName[128] = {}; // Приложение -> Драйвер: "кадр обработан"
    WCHAR sharedMemoryName[128] = {};
    WCHAR sharedTextureName1[128] = {};
    WCHAR sharedTextureName2[128] = {};
};
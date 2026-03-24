#pragma once

#include <stdint.h>
#include <string>
#include <windows.h>

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

enum class ConnectionType {
    Null, Usb, Wireless
};

enum class CoderType {
    Null, FFmpeg, Jpeg
};

struct MonitorConfig {
    uint16_t monitorId;
    uint16_t byteDepth;
    uint16_t width;
    uint16_t height;
    uint16_t refreshRate;
    uint16_t quality;
    ConnectionType connectionType;
    CoderType coderType;
    bool enabled;
};

struct DriverInfo {
    uint32_t monitorCount;
    uint32_t maxMonitors;
    uint32_t driverVersion;
};

struct CreateMonitorRequest {
    MonitorConfig config;
    WCHAR frameReadyName[128] = {};
    WCHAR frameProcessedName[128] = {};
    WCHAR sharedMemoryName[128] = {};
    WCHAR sharedTextureName1[128] = {};
    WCHAR sharedTextureName2[128] = {};
};
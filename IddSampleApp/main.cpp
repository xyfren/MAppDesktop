#include <iostream>
#include <vector>
#include <windows.h>
#include <swdevice.h>
#include <conio.h>
#include <wrl.h>

#include "MonitorManager.h"


VOID WINAPI
CreationCallback(
    _In_ HSWDEVICE hSwDevice,
    _In_ HRESULT hrCreateResult,
    _In_opt_ PVOID pContext,
    _In_opt_ PCWSTR pszDeviceInstanceId
    )
{
    HANDLE hEvent = *(HANDLE*) pContext;
    SetEvent(hEvent);
    UNREFERENCED_PARAMETER(hSwDevice);
    UNREFERENCED_PARAMETER(hrCreateResult);
    UNREFERENCED_PARAMETER(pszDeviceInstanceId);
}

int __cdecl wmain(int argc, wchar_t *argv[])
{
    setlocale(0, "");

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    HSWDEVICE hSwDevice;
    SW_DEVICE_CREATE_INFO createInfo = { 0 };

    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszzCompatibleIds = L"IddSampleDriver\0\0";
    createInfo.pszInstanceId = L"IddSampleDriver";
    createInfo.pszzHardwareIds = L"IddSampleDriver\0\0";
    createInfo.pszDeviceDescription = L"Idd Sample Driver";
    createInfo.CapabilityFlags = SWDeviceCapabilitiesRemovable |
                                 SWDeviceCapabilitiesSilentInstall |
                                 SWDeviceCapabilitiesDriverRequired;

    // Create the device
    HRESULT hr = SwDeviceCreate(L"IddSampleDriver",
                                L"HTREE\\ROOT\\0",
                                &createInfo,
                                0,
                                nullptr,
                                CreationCallback,
                                &hEvent,
                                &hSwDevice);
    if (FAILED(hr))
    {
        printf("SwDeviceCreate failed with 0x%lx\n", hr);
        return 1;
    }

    // Wait for callback to signal that the device has been created
    printf("Waiting for device to be created....\n");
    DWORD waitResult = WaitForSingleObject(hEvent, 10000);
    if (waitResult != WAIT_OBJECT_0)
    {
        printf("Wait for device creation failed\n");
        return 1;
    }
    
    MonitorManager* monManager = new MonitorManager();

    if (!monManager->Initialize()) {
        printf("MonitorManager Init Error\n");
        return 1;
    }

    if (!monManager->ConnectToDriver()) {
        printf("Open driver error!!!");
        DWORD err = GetLastError();
        printf("CreateFile failed, error = %u (0x%08X)\n", err, err);
        SwDeviceClose(hSwDevice);
        return 1;
    }
    // MAIN PROGRAMM LOOP
    bool bExit = false;
    char ch;

    printf("Choose action:\n");
    printf("1. Create new monitor.\n");
    printf("2. Remove monitor.\n");
    printf("3. Get Driver Info.\n");
    printf("Press 'x' to exit.\n");
    printf("\nEnter your choice (1-4): ");
    fflush(stdout);

    while (!bExit) {
        if (_kbhit()) {
            ch = _getch();

            switch (ch) {
            case '1':
                printf("\n\nCreating new monitor...\n");
                monManager->AddMonitor(0U, 1600U, 900U, 4U, 120U);
                printf("\nEnter your choice (1-4): ");
                fflush(stdout);
                break;

            case '2':
                printf("\n\nRemoving monitor...\n");
                monManager->RemoveMonitor(0);
                printf("\nEnter your choice (1-4): ");
                fflush(stdout);
                break;

            case '3':
            {
                printf("\n\n");
                DriverInfo driverInfo = monManager->GetDriverInfo();
                printf("Driver version: %d\n", driverInfo.driverVersion);
                printf("Monitor count: %d\n", driverInfo.monitorCount);
                printf("Max monitors: %d\n", driverInfo.maxMonitors);
                printf("\nEnter your choice (1-4): ");
                fflush(stdout);
                break;
            }
            case 'x':
                bExit = true;
                printf("\nExiting...\n");
                break;
            }
        }

        // Небольшая задержка чтобы не нагружать процессор
        Sleep(50);
    }
    // Stop the device, this will cause the sample to be unloaded
    SwDeviceClose(hSwDevice);

    return 0;
}
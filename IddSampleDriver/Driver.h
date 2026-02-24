#pragma once

#define NOMINMAX
#include <windows.h>
#include <bugcodes.h>
#include <wudfwdm.h>
#include <wdf.h>
#include <iddcx.h>
#include <string.h>

#include <dxgi1_5.h>
#include <d3d11_2.h>
#include <d3d11_1.h>
#include <avrt.h>
#include <wrl.h>
#include <time.h>

#include <memory>
#include <vector>

#include "Trace.h"
#include "../Common.h"
#include "Tools.h"
#include "../DoubleBuffer.h"
#include "../VideoBuffer.h"


namespace Microsoft
{
    namespace WRL
    {
        namespace Wrappers
        {
            // Adds a wrapper for thread handles to the existing set of WRL handle wrapper classes
            typedef HandleT<HandleTraits::HANDLENullTraits> Thread;
        }
    }
}

namespace Microsoft
{
    namespace IndirectDisp
    {
        /// <summary>
        /// Manages the creation and lifetime of a Direct3D render device.
        /// </summary>
        struct IndirectSampleMonitor
        {
            static constexpr size_t szEdidBlock = 128;
            static constexpr size_t szModeList = 3;

            const BYTE pEdidBlock[szEdidBlock];
            const struct SampleMonitorMode {
                DWORD Width;
                DWORD Height;
                DWORD VSync;
            } pModeList[szModeList];
            const DWORD ulPreferredModeIdx;
        };

        /// <summary>
        /// Manages the creation and lifetime of a Direct3D render device.
        /// </summary>
        struct Direct3DDevice
        {
            Direct3DDevice(LUID AdapterLuid);
            Direct3DDevice();
            HRESULT Init();

            LUID AdapterLuid;
            Microsoft::WRL::ComPtr<IDXGIFactory5> DxgiFactory;
            Microsoft::WRL::ComPtr<IDXGIAdapter1> Adapter;
            Microsoft::WRL::ComPtr<ID3D11Device> Device;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
        };

        /// <summary>
        /// Manages a thread that consumes buffers from an indirect display swap-chain object.
        /// </summary>
        class SwapChainProcessor
        {
        public:
            SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, std::shared_ptr<Direct3DDevice> Device, HANDLE NewFrameEvent, DoubleBuffer* pBuffer);
            ~SwapChainProcessor();

            Direct3DDevice* GetD3DDevice();

        private:
            static DWORD CALLBACK RunThread(LPVOID Argument);

            void Run();
            void RunCore();

            IDDCX_SWAPCHAIN m_hSwapChain;
            std::shared_ptr<Direct3DDevice> m_Device;
            HANDLE m_hAvailableBufferEvent;
            Microsoft::WRL::Wrappers::Thread m_hThread;
            Microsoft::WRL::Wrappers::Event m_hTerminateEvent;

            DoubleBuffer* m_pBuffer;
            uint64_t m_frameCounter;
        };

        /// <summary>
        /// Provides a sample implementation of an indirect display driver.
        /// </summary>
        class IndirectMonitorContext
        {
        public:
            IndirectMonitorContext(_In_ IDDCX_MONITOR Monitor,_In_ CreateMonitorRequest* pRequest);
            virtual ~IndirectMonitorContext();

            bool OpenSharedBuffer();

            bool OpenSharedTextures(Direct3DDevice* pD3DDevice);

            IDDCX_MONITOR GetMonitorHandle() const;
            MonitorConfig GetMonitorConfig() const;

            void AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent);
            void UnassignSwapChain();

        private:
            IDDCX_MONITOR m_Monitor;
            MonitorConfig m_Config;

            std::wstring m_frameReadyName;
            std::wstring m_frameProcessedName;
            std::wstring m_sharedMemoryName;
            std::wstring m_sharedTextureName1;
            std::wstring m_sharedTextureName2;
            
            std::unique_ptr<SwapChainProcessor> m_ProcessingThread;
            DoubleBuffer* m_pBuffer;
            VideoBuffer* m_pVideo;
        };

        class IndirectDeviceContext
        {
        public:
            IndirectDeviceContext(_In_ WDFDEVICE WdfDevice);
            virtual ~IndirectDeviceContext();

            void InitAdapter();

            NTSTATUS HandleIoctl(_In_ WDFREQUEST Request, _In_ size_t OutputLength,
                _In_ size_t InputLength, _In_ ULONG IoControlCode);

        protected:
            WDFDEVICE m_WdfDevice;
            IDDCX_ADAPTER m_Adapter;
        
        private:
            NTSTATUS AddMonitor(CreateMonitorRequest* pRequest);
            NTSTATUS RemoveMonitor(uint16_t MonitorId);

            std::vector<IndirectMonitorContext*> m_Monitors;
            WDFWAITLOCK m_MonitorLock;  // Äëÿ 
        };
    }
}

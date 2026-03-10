#pragma once

#include "FFmpegCoder.h"
#include "../Common.h"
#include <iostream>

class FrameManager
{

public:
	FrameManager(MonitorConfig& m_config);
	~FrameManager();

	int createFrameBuffer(uint32_t frameId, uint32_t rowPitch, std::span<uint8_t>& inputBuffer,std::mutex** ppOutputMutex, std::span<uint8_t>& outputBuffer);

private:

	MonitorConfig m_config = {};
	FFmpegCoder* m_pEncoder;

	std::vector<std::pair<std::mutex*, std::span<uint8_t>>> frameBuffers;
};


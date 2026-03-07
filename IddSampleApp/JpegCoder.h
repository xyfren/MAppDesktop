#pragma once

#include <span>
#include <vector>
#include <mutex>
#include <turbojpeg.h>
#include "../Common.h"

class JpegCoder
{
public:
	JpegCoder(MonitorConfig& config);
	~JpegCoder();

	int encodeToJpeg(const uint8_t* inputBuffer, uint8_t** outputBuffer, unsigned long* outputSize);

private:
	tjhandle m_compressor;

	MonitorConfig m_config;
};


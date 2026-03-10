#include "FrameManager.h"

FrameManager::FrameManager(MonitorConfig& m_config) {
	m_config = m_config;
	m_pEncoder = new FFmpegCoder(m_config);
	auto mtx1 = new std::mutex();
	auto mtx2 = new std::mutex();

	// Allocate with malloc so the buffer is compatible with realloc() which
	// FFmpegCoder may call internally if encoded data grows beyond this size.
	size_t bufSize = static_cast<size_t>(m_config.width) * m_config.height * m_config.byteDepth;
	uint8_t* buf1 = static_cast<uint8_t*>(malloc(bufSize));
	uint8_t* buf2 = static_cast<uint8_t*>(malloc(bufSize));
	std::span<uint8_t> buffer1Span(buf1, bufSize);
	std::span<uint8_t> buffer2Span(buf2, bufSize);

	frameBuffers.push_back({ mtx1, buffer1Span });
	frameBuffers.push_back({ mtx2, buffer2Span });
}

FrameManager::~FrameManager() {
	for (auto& [mtx, span] : frameBuffers) {
		if (span.data()) {
			free(span.data());
		}
		delete mtx;
	}
	delete m_pEncoder;
}

int FrameManager::createFrameBuffer(uint32_t frameId, uint32_t rowPitch, std::span<uint8_t>& inputBuffer, std::mutex** ppOutputMutex, std::span<uint8_t>& outputBuffer) {
	int bufferIdx = frameId % 2;

	frameBuffers[bufferIdx].first->lock();
	uint8_t* pOutputBuffer = frameBuffers[bufferIdx].second.data();
	size_t outputSize = frameBuffers[bufferIdx].second.size();

	int r = m_pEncoder->encodeFrame(inputBuffer.data(), rowPitch, &pOutputBuffer, &outputSize);
	if (r != 0) {
		std::cerr << "Ошибка кодирования H.264: " << r << std::endl;
		frameBuffers[bufferIdx].first->unlock();
		return r;
	}
	// Update the span in case the encoder reallocated the buffer.
	frameBuffers[bufferIdx].second = std::span<uint8_t>(pOutputBuffer, outputSize);
	frameBuffers[bufferIdx].first->unlock();

	*ppOutputMutex = frameBuffers[bufferIdx].first;
	outputBuffer = frameBuffers[bufferIdx].second;
	return r;
}
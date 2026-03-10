#include "FrameManager.h"

FrameManager::FrameManager(MonitorConfig& m_config) {
	m_config = m_config;
	m_pJpegCoder = new JpegCoder(m_config);
	auto mtx1 = new std::mutex();
	auto mtx2 = new std::mutex();

	// Allocate with tjAlloc so that TurboJPEG can safely realloc/free these
	// buffers if the encoded JPEG ever exceeds the pre-allocated size.
	unsigned long bufSize = m_config.width * m_config.height * m_config.byteDepth;
	uint8_t* buf1 = tjAlloc(bufSize);
	uint8_t* buf2 = tjAlloc(bufSize);
	std::span<uint8_t> buffer1Span(buf1, bufSize);
	std::span<uint8_t> buffer2Span(buf2, bufSize);

	frameBuffers.push_back({ mtx1,buffer1Span});
	frameBuffers.push_back({ mtx2,buffer2Span });
}

FrameManager::~FrameManager() {
	for (auto& [mtx, span] : frameBuffers) {
		if (span.data()) {
			tjFree(span.data());
		}
		delete mtx;
	}
	delete m_pJpegCoder;
}

int FrameManager::createFrameBuffer(uint32_t frameId, uint32_t rowPitch, std::span<uint8_t>& inputBuffer, std::mutex** ppOutputMutex, std::span<uint8_t>& outputBuffer) {
	int bufferIdx = frameId % 2;

	frameBuffers[bufferIdx].first->lock();
	uint8_t* pOutputBuffer = frameBuffers[bufferIdx].second.data();
	unsigned long outputSize = static_cast<unsigned long>(frameBuffers[bufferIdx].second.size());

	int r = m_pJpegCoder->encodeToJpeg(inputBuffer.data(), rowPitch,&pOutputBuffer, &outputSize);
	if (r != 0) {
		std::cerr << "Ошибка кодирования JPEG: " << tjGetErrorStr() << std::endl;
		frameBuffers[bufferIdx].first->unlock();
		return r;
	}
	// Update the span in case TurboJPEG reallocated the buffer.
	frameBuffers[bufferIdx].second = std::span<uint8_t>(pOutputBuffer, outputSize);
	frameBuffers[bufferIdx].first->unlock();

	*ppOutputMutex = frameBuffers[bufferIdx].first;
	outputBuffer = frameBuffers[bufferIdx].second;
	return r;
}
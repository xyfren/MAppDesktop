#include "FrameManager.h"

FrameManager::FrameManager(MonitorConfig& m_config) {
	m_config = m_config;
	m_pJpegCoder = new JpegCoder(m_config);
	auto mtx1 = new std::mutex();
	auto mtx2 = new std::mutex();

	uint8_t* buf1 = new uint8_t[m_config.width * m_config.height * m_config.byteDepth];
	uint8_t* buf2 = new uint8_t[m_config.width * m_config.height * m_config.byteDepth];
	std::span<uint8_t> buffer1Span(buf1, m_config.width * m_config.height * m_config.byteDepth);
	std::span<uint8_t> buffer2Span(buf2, m_config.width * m_config.height * m_config.byteDepth);

	frameBuffers.push_back({ mtx1,buffer1Span});
	frameBuffers.push_back({ mtx2,buffer2Span });
}

FrameManager::~FrameManager() {

}

int FrameManager::createFrameBuffer(uint32_t frameId, uint32_t rowPitch, std::span<uint8_t>& inputBuffer, std::mutex** ppOutputMutex, std::span<uint8_t>& outputBuffer) {
	int bufferIdx = frameId % 2;

	frameBuffers[bufferIdx].first->lock();
	uint8_t* pOutputBuffer = frameBuffers[bufferIdx].second.data();
	unsigned long outputSize = m_config.width * m_config.height * m_config.byteDepth; // Размер буфера веделенного через оператор new в конструкторе FrameManager. TurboJPEG может перевыделить его, если не хватит, но не может выделить новый, если указать nullptr.

	int r = m_pJpegCoder->encodeToJpeg(inputBuffer.data(), rowPitch,&pOutputBuffer, &outputSize);
	if (r != 0) {
		std::cerr << "Ошибка кодирования JPEG: " << tjGetErrorStr() << std::endl;
		frameBuffers[bufferIdx].first->unlock();
		return r;
	}
	frameBuffers[bufferIdx].second = std::span<uint8_t>(pOutputBuffer, outputSize);
	frameBuffers[bufferIdx].first->unlock();

	*ppOutputMutex = frameBuffers[bufferIdx].first;
	outputBuffer = frameBuffers[bufferIdx].second;
	return r;
}
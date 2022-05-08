#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <stdexcept>

struct fileInputStream {
	int fd;
	char buffer[1 << 13];
	size_t index, bytesRead;

	fileInputStream(const std::string& filename) {
		fd = open(filename.c_str(), O_RDONLY);
		if (fd < 0) {
			throw std::runtime_error("Could not open file '" + filename + '\'');
		}
		posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM); //POSIX_FADV_SEQUENTIAL
		readBuffer();
	}

	~fileInputStream() {
		::close(fd);
	}

	void close() {
		::close(fd);
	}

	__attribute__((always_inline)) void readBuffer() {
		bytesRead = read(fd, buffer, sizeof(buffer));
		if (bytesRead == 0) throw "reached end of file";
		index = 0;
	}

	template<typename T>
	__attribute__((always_inline)) fileInputStream& operator>>(T& dst) {
		if constexpr (sizeof(dst) == 1) {
			if (index == bytesRead) readBuffer();
			dst = buffer[index++]; 
		} else if constexpr (sizeof(T) == 4) {
			size_t bytesLeft = bytesRead - index;
			uint32_t& dstInt = *(uint32_t*)&dst;
			if (sizeof(T) <= bytesLeft) [[likely]] {
				// the compiler knows how to do this efficiently
				dstInt = *((const uint32_t*)(buffer + index));
				index += sizeof(T);
			} else {
				dstInt = *((const uint32_t*)(buffer + index)) & (UINT32_MAX << (sizeof(T) - bytesLeft));
				readBuffer();
				dstInt |= *((const uint32_t*)(buffer + index)) >> bytesLeft;
				index += sizeof(T) - bytesLeft;
			}
		} else {
			char* dstBytes = (char*)&dst;
			size_t copyBytes;
			for (size_t offset = 0; offset < sizeof(T); offset += copyBytes) {
				copyBytes = std::min(sizeof(T) - offset, bytesRead - index);	
				std::copy(buffer + index, buffer + index + copyBytes, &dstBytes[offset]);
				index += copyBytes;
				if (copyBytes < sizeof(T) - offset) readBuffer();
			}
		}
		return *this;
	}
};

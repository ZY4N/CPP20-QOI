#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <stdexcept>

struct fileOutputStream {
	int fd;
	char buffer[1 << 13];
	size_t index;

	fileOutputStream(const std::string& filename) : index{ 0 } {
		fd = creat(filename.c_str(), S_IRWXG | S_IRWXO | S_IRWXU);
		if (fd < 0) {
			throw std::runtime_error("Could not open file '" + filename + '\'');
		}
	}

	~fileOutputStream() {
		flush();
		close();
	}

	void close() {
		::fsync(fd);
		::close(fd);
	}
	
	__attribute__((always_inline)) void flush() {
		write(fd, buffer, index);
		index = 0;
	}

	template<typename T>
	inline fileOutputStream& operator<<(const T& src) {
		if constexpr (sizeof(src) == 1) {
			if (index == sizeof(buffer)) flush();
			buffer[index++] = (char)src;
		} else {
			const char* srcBytes = reinterpret_cast<const char*>(&src);
			size_t bytesCopied;
			for (size_t offset = 0; offset < sizeof(T); offset += bytesCopied) {
				bytesCopied = std::min(sizeof(T) - offset, sizeof(buffer) - index);	
				std::copy(&srcBytes[offset], &srcBytes[offset + bytesCopied], &buffer[index]);
				index += bytesCopied;
				if (bytesCopied < sizeof(T) - offset) flush();
			}
		}
		return *this;
	}
};

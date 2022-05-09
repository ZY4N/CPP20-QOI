#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <stdexcept>

#ifndef CONSTEXPR_FOR
#define CONSTEXPR_FOR
template <auto Start, auto End, auto Inc, class F>
constexpr void constexpr_for(F&& f) {
	if constexpr (Start < End) {
		f(std::integral_constant<decltype(Start), Start>());
		constexpr_for<Start + Inc, End, Inc>(f);
	}
}
#endif

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
		} else {
			size_t bytesLeft = bytesRead - index;
			uint8_t* dstBytes = (uint8_t*)&dst;
			if (sizeof(T) <= bytesLeft) [[likely]] {
				constexpr_for<0, sizeof(T), 1>([&](auto i) {
					if constexpr (std::is_integral<T>::value) {
						dstBytes[sizeof(T) - 1 - i] = buffer[index++];
					} else {
						dstBytes[i] = buffer[index++];
					}
				});
			} else {
				for (size_t i = 0; i < bytesLeft; i++) {
					if constexpr (std::is_integral<T>::value) {
						dstBytes[sizeof(T) - 1 - i] = buffer[index++];
					} else {
						dstBytes[i] = buffer[index++];
					}
				}
				readBuffer();
				for (size_t i = bytesLeft; i < sizeof(T); i++) {
					if constexpr (std::is_integral<T>::value) {
						dstBytes[sizeof(T) - 1 - i] = buffer[index++];
					} else {
						dstBytes[i] = buffer[index++];
					}
				}
			}
		}
		return *this;
	}
};

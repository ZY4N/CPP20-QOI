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
		if constexpr (sizeof(T) == 1) {
			if (index == sizeof(buffer)) flush();
			buffer[index++] = (char)src;
		} else {
			if (sizeof(T) > sizeof(buffer) - index) flush();
			const uint8_t* srcBytes = (const uint8_t*)&src;
			constexpr_for<0, sizeof(T), 1>([&](auto i) {
				if constexpr (std::is_integral<T>::value) {
					buffer[index++] = srcBytes[sizeof(T) - 1 - i];
				} else {
					buffer[index++] = srcBytes[i];
				}
			});
		}
		return *this;
	}
};

#pragma once

#include <algorithm>
#include <cstring>

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;
typedef unsigned long	u64;
typedef size_t			ux;

typedef signed char		i8;
typedef signed short	i16;
typedef signed int		i32;
typedef signed long		i64;
typedef ssize_t			ix;

#define U8_MAX	(255)
#define U16_MAX	(65535)
#define U32_MAX	(4294967295)
#define U64_MAX	(18446744073709551615)

#define I8_MAX	(127)
#define I16_MAX	(32767)
#define I32_MAX	(2147483647)
#define I64_MAX	(9223372036854775807)

#define I8_MIN	(-128)
#define I16_MIN	(-32768)
#define I32_MIN	(-2147483648)
#define I64_MIN	(-9223372036854775808)

#define force_inline __attribute__((always_inline))

namespace qoi {

constexpr char magic[]{ 'q', 'o', 'i', 'f' };

constexpr ux LOOKUP_SIZE	= 64;
constexpr ux MAX_RUN_SIZE	= 62;

enum class tag : u8 {
	INDEX	= 0b00000000,
	DIFF	= 0b01000000,
	LUMA	= 0b10000000,
	RUN		= 0b11000000,
	RGB		= 0b11111110,
	RGBA	= 0b11111111
};

struct color {
	u8 r{ 0 }; u8 g{ 0 }; u8 b{ 0 }; u8 a{ 255 };
	force_inline operator const u32&() const {
		return *(const uint32_t*)this;
	};
	force_inline color operator|(u32 mask) {
		u32 t = ((*(u32*)this) | mask);
		return *(color*)&t;
	};
};

force_inline u8 colorHash(const color& c) {
	return (c.r * 3 + c.g * 5 + c.b * 7 + c.a * 11) % LOOKUP_SIZE;
}

force_inline u8 fitsSigned(auto num, auto limit) {
	return num < limit && num >= -limit;
}

template<int index, typename T, typename... Ts>
constexpr const auto& get(T&& first, Ts&&... rest) {
	if constexpr (index == 0) {
		return first;
	} else {
		static_assert(sizeof...(Ts) > 0	, "Index out of bounds");
		static_assert(index >= 0		, "Index out of bounds");
		return get<index - 1>(std::forward<Ts>(rest)...);
	}
}

template<tag T, class OS, typename... Args>
force_inline void write(OS& os, Args&&... args) {
	#define getArg(I) get<(I)>(std::forward<Args>(args)...)
	u8 firstByte = (u8)T;
	using enum tag;
	if constexpr (T == RGB) {
		const color& pixel = getArg(0);
		os << firstByte << pixel.r << pixel.g << pixel.b;
	} else if constexpr (T == RGBA) {
		os << firstByte << getArg(0);
	} else if constexpr (T == INDEX || T == RUN || T == LUMA) {
		os << (u8)(firstByte | ((u8)getArg(0) & 0x3f));
		if constexpr (T == LUMA) {
			u8 secondByte = ((u8)getArg(1) & 0xf) << 4;
			secondByte   |= ((u8)getArg(2) & 0xf) << 0;
			os << secondByte;
		}
	} else if constexpr (T == DIFF) {
		firstByte |= ((u8)getArg(0) & 0x3) << 4;
		firstByte |= ((u8)getArg(1) & 0x3) << 2;
		firstByte |= ((u8)getArg(2) & 0x3) << 0;
		os << firstByte;
	}
	#undef getArg
}

template<class OS>
void encode(OS& os, const u8* src, u32 width, u32 height, u8 channels, u8 colorspace = 0) {
	color lookup[LOOKUP_SIZE];
	std::memset((u8*)&lookup, 0, sizeof(lookup));

	color pixel, pPixel;
	u8 runLength = 0;

	os << magic << width << height << channels << colorspace;

	const u32 maskRGB = U32_MAX >> (8 * (4 - channels));
	const u32 maskA   = U32_MAX << (8 * channels);

	size_t numPixels = width * height;
	for (ux i = 0; i < numPixels; i++) {
		pPixel = pixel;
		pixel = *(color*)(src + i * channels) | maskA;

		if ((pixel & maskRGB) == (pPixel & maskRGB)) {
			if (runLength++ == MAX_RUN_SIZE || i == numPixels - 1) {
				write<tag::RUN>(os, runLength - 1);
				runLength = 0;
			}
			continue;
		}
		if (runLength > 0) {
			write<tag::RUN>(os, runLength - 1); 
			runLength = 0;
		}

		
		u8 index = colorHash(pixel);
		if ((lookup[index] & maskRGB) == (pixel & maskRGB)) {
			write<tag::INDEX>(os, index);
			continue;
		}

		lookup[index] = pixel;

		if (channels == 3 || pixel.a == pPixel.a) {
			i16 diffR = (i16)pixel.r - (i16)pPixel.r;
			i16 diffG = (i16)pixel.g - (i16)pPixel.g;
			i16 diffB = (i16)pixel.b - (i16)pPixel.b;
			if (fitsSigned(diffR, 2) && fitsSigned(diffG, 2) && fitsSigned(diffB, 2)) {
				write<tag::DIFF>(os, (u8)(2 + diffR), (u8)(2 + diffG), (u8)(2 + diffB));
				continue;
			}
			i16 diffRG = diffR - diffG;
			i16 diffBG = diffB - diffG;
			if (fitsSigned(diffG, 32) && fitsSigned(diffRG, 8) && fitsSigned(diffBG, 8) ) {
				write<tag::LUMA>(os, (u8)(32 + diffG), (u8)(8 + diffRG), (u8)(8 + diffBG));
				continue;
			}
		}
		if (channels == 4) {
			write<tag::RGBA>(os, pixel);
		} else {
			write<tag::RGB>(os, pixel);
		}
	}
	os << 0 << 1;
}

template<class IS>
u8* decode(IS& is, u32* width, u32* height, u8* channels, u8* colorspace) {
	color lookup[LOOKUP_SIZE];
	std::memset((u8*)&lookup, 0, sizeof(lookup));

	char maybeMagic[4];
	is >> maybeMagic;
	if (std::strncmp(maybeMagic, magic, sizeof(magic)))
		throw std::runtime_error("Wrong file format");

	is >> *width >> *height >> *channels >> *colorspace;

	ux pixelSize = (*width) * (*height) * (*channels);
	u8* pixels = new u8[pixelSize + 1]; // extra byte to make decoding easier/faster

	color pPixel;
	u8 pixelRun = 0;

	for (ux i = 0; i < pixelSize; i += *channels) {
		color& pixel = *(color*)(pixels + i);

		if (pixelRun > 0) {
			pixel = pPixel;
			pixelRun--;
			continue;
		}
	
		u8 nextByte; is >> nextByte;
		const tag t = (tag)(nextByte & 0xc0);

		using enum tag;
		if (nextByte == (u8)RGB) {
			is >> pixel.r >> pixel.g >> pixel.b; pixel.a = 255;
		} else if (nextByte == (u8)RGBA) {
			is >> pixel;
		} else if (t == DIFF) {
			pixel.r = pPixel.r + (u8)(((nextByte >> 4) & 3) - 2);
			pixel.g = pPixel.g + (u8)(((nextByte >> 2) & 3) - 2);
			pixel.b = pPixel.b + (u8)(((nextByte >> 0) & 3) - 2);
			pixel.a = pPixel.a;
		} else if (t == INDEX) {
			pixel = lookup[nextByte & 0x3f];
		} else if (t == RUN) {
			pixelRun = nextByte & 0x3f;
			pixel = pPixel;
		} else if (t == LUMA) {
			i8 diffG = (nextByte & 0x3f) - 32;
			is >> nextByte;
			i8 diffRG = (nextByte >> 4 & 0xf) - 8;
			i8 diffBG = (nextByte >> 0 & 0xf) - 8;
			pixel.g = pPixel.g + diffG;
			pixel.r = pPixel.r + diffG + diffRG;
			pixel.b = pPixel.b + diffG + diffBG;
			pixel.a = pPixel.a;
		}
		lookup[colorHash(pixel)] = pPixel = pixel;
	}
	return pixels;
}
}

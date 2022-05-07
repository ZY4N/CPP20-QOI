#pragma once

#include <algorithm>
#include <cstring>

typedef signed char		i8;
typedef signed short	i16;
typedef signed int		i32;
typedef signed long		i64;
typedef ssize_t			ix;

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;
typedef unsigned long	u64;
typedef size_t			ux;


#define force_inline __attribute__((always_inline))

namespace qoi {

constexpr char magic[4]{ 'q', 'o', 'i', 'f' };

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
	force_inline bool operator==(const color&) const = default;
};

force_inline u8 colorHash(const color& c) {
	return (c.r * 3 + c.g * 5 + c.b * 7) % LOOKUP_SIZE;
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
		os << firstByte;
		os << getArg(0);
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
void encode(OS& os, const u8* raw, u32 width, u32 height, u8 channels, u8 colorspace = 0) {
	color lookup[LOOKUP_SIZE];
	std::memset((u8*)&lookup, 0, sizeof(lookup));

	const color* pixels = reinterpret_cast<const color*>(raw);
	color pixel, pPixel;
	u8 runLength = 0;

	os << magic;
	os << width;
	os << height;
	os << channels;
	os << colorspace;

	size_t numPixels = width * height;
	for (ux i = 0; i < numPixels; i++) {
		pPixel = pixel;
		pixel = pixels[i];

		if (pixel == pPixel) {
			if (runLength++ == MAX_RUN_SIZE) {
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
		if (lookup[index] == pixel) {
			write<tag::INDEX>(os, index);
			continue;
		}

		lookup[index] = pixel;

		if (pixel.a == pPixel.a) {
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
	// DIFF with 0 on all channels can never happen -> end_tag
	write<tag::DIFF>(os, (u8)2, (u8)2, (u8)2);
}

template<class IS>
u8* decode(IS& is, u32* width, u32* height, u8* channels, u8* colorspace) {
	color lookup[LOOKUP_SIZE];
	std::memset((u8*)&lookup, 0, sizeof(lookup));

	char maybeMagic[4];
	is >> maybeMagic;
	is >> *width;
	is >> *height;
	is >> *channels;
	is >> *colorspace;

	color* dst = new color[*width * *height];
	color* dstIt = dst;
	color& pPixel = *dst; 

	u8 cByte, endTag = 0b01101010;
	for (is >> cByte; cByte != endTag; is >> cByte) {
		using enum tag;
		if (cByte == (u8)RGB) {
			is >> dstIt->r;
			is >> dstIt->g;
			is >> dstIt->b;
		} else if (cByte == (u8)RGBA) {
			is >> *dstIt;
		} else {
			const tag t = (tag)(cByte & 0xc0);
			if (t == tag::DIFF) {
				dstIt->r = pPixel.r + (u8)(((cByte >> 4) & 3) - 2);
				dstIt->g = pPixel.g + (u8)(((cByte >> 2) & 3) - 2);
				dstIt->b = pPixel.b + (u8)(((cByte >> 0) & 3) - 2);
				dstIt->a = pPixel.a;
			} else if (t == INDEX) {
				*dstIt = lookup[cByte & 0x3f];
			} else if (t == RUN) {
				u8 runLength = cByte & 0x3f;
				for (u8 i = 0; i < runLength + 1; i++) {
					dstIt[i] = pPixel;
				}
				dstIt += runLength;
			} else if (t == LUMA) {
				i8 diffG = (cByte & 0x3f) - 32;
				u8 diffToG; is >> diffToG;
				i8 diffRG = (diffToG >> 4 & 0xf) - 8;
				i8 diffBG = (diffToG >> 0 & 0xf) - 8;
				dstIt->g = pPixel.g + diffG;
				dstIt->r = pPixel.r + diffG + diffRG;
				dstIt->b = pPixel.b + diffG + diffBG;
				dstIt->a = pPixel.a;
			}
		}
		lookup[colorHash(*dstIt)] = pPixel = *dstIt;
		dstIt++;
	}
	return reinterpret_cast<u8*>(dst);
}
}

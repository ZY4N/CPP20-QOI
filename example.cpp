#include <stb_no_warnings.hpp>
#include <fileInputStream.hpp>
#include <fileOutputStream.hpp>
#include <qoi.hpp>

int main() {

	// load and decode qoi
	uint32_t width, height; uint8_t channels, colorspace;
	fileInputStream is("input.qoi");
	uint8_t* pixels = qoi::decode(is, &width, &height, &channels, &colorspace);
	is.close();

	// safe to png using stb
	stbi_write_png("output.png", width, height, channels, pixels, width * channels);
	
	// encode and write qoi
	fileOutputStream os("output.qoi");
	qoi::encode(os, pixels, width, height, channels);
	os.flush();
	os.close();

	delete[] pixels;

	return 0;
}

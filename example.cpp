#include <stb_no_warnings.hpp>
#include <fileInputStream.hpp>
#include <fileOutputStream.hpp>
#include <qoi.hpp>

#include <stdexcept>
#include <iostream>
#include <string>

int main(int numArgs, char* args[]) {

	if (numArgs < 3)
		throw std::runtime_error("Missing input and/or output file");

	const std::string inputFile  = args[1];
	const std::string outputFile = args[2];

	const auto getExtension = [](const std::string& file) {
		return file.substr(file.find_last_of('.') + 1);
	};

	const auto inputExt  = getExtension(inputFile);
	const auto outputExt = getExtension(outputFile);

	if (inputExt == outputExt)
		throw std::runtime_error("same file extensions");


	std::cout << "loading \"" << inputFile << "\"...\n";

	uint32_t width, height; uint8_t channels, colorspace; uint8_t* pixels;

	if (inputExt == "qoi") {
		fileInputStream is(inputFile);
		pixels = qoi::decode(is, &width, &height, &channels, &colorspace);
		is.close();
	} else {
		int w, h, c;
		pixels = (u8*)stbi_load(inputFile.c_str(), &w, &h, &c, 0);
		if (!pixels) throw std::runtime_error("Cannot load image");
		width = w; height = h; channels = c;
	}


	std::cout << "saving  \"" << outputFile << "\"...\n";

	if (outputExt == "qoi") {
		fileOutputStream os(outputFile);
		qoi::encode(os, pixels, width, height, channels);
		os.flush();
		os.close();
	} else {
		stbi_write_png(outputFile.c_str(), width, height, channels, pixels, width * channels);
	}

	delete[] pixels;

	std::cout << "finished!\n";

	return 0;
}

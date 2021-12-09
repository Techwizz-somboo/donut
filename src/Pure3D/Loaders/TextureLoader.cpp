// Copyright 2019-2020 the donut authors. See COPYING.md for legal info.

#include "TextureLoader.h"

#include "Core/Log.h"
#include "Render/OpenGL/GLTexture2D.h"

#include <png.h>

using namespace Donut::GL;

namespace donut::pure3d
{

enum class ImageFormat : uint32_t
{
	Raw = 0,
	PNG = 1,
	TGA = 2,
	BMP = 3,
	IPU = 4,
	DXT = 5,
	DXT1 = 6,
	DXT2 = 7,
	DXT3 = 8,
	DXT4 = 9,
	DXT5 = 10,
};

std::shared_ptr<Entity> TextureLoader::LoadEntity(ChunkFile& file, void* store)
{
	std::string name = file.ReadU8String();
	uint32_t version = file.Read<uint32_t>();

	assert(version == 14000);

	// skip: w, h, bpp, ad, mipmaps, type, usage, priority (we get this from the image)
	file.Seek(32, Donut::SeekOrigin::Current);

	std::shared_ptr<Texture> texture;

	while (file.ChunksRemaining())
	{
		auto const chunkID = file.BeginChunk();

		// TODO: handle TextureVolumeImage
		switch (chunkID)
		{
			case static_cast<ChunkID>(ChunkID::TextureImage) : texture = LoadImage(file); break;
			default: Log::Debug("Unhandled chunk {}\n", chunkID);
		}

		file.EndChunk();
	}

	if (texture != nullptr) {
		texture->SetName(name);
	}

	return texture;
}

std::shared_ptr<Texture> TextureLoader::LoadImage(ChunkFile& file)
{
	std::string name = file.ReadU8String();
	uint32_t version = file.Read<uint32_t>();

	assert(version == 14000);

	// this is all unused, we trust the image data only!
	file.Seek(16, SeekOrigin::Current); // w, h, bpp, pal not used

	bool hasAlpha = file.Read<uint32_t>() == 1;
	uint32_t format = file.Read<uint32_t>();

	GLuint textureHandle;

	glGenTextures(1, &textureHandle);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textureHandle);

	// if hasAlpha then GL_RGBA else GL_RGB?
	// if format = dxt then glCompressedTexImage2D ( GL_COMPRESSED_RGBA_S3TC_DXT1_EXT )

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glGenerateMipmap(GL_TEXTURE_2D);


	// we should probably make the texture here
	// then copy into it the data from the TextureImageData chunk

	// only handle png atm as we are shit
	assert(format == 1); // RAW, PNG, TGA, BMP, IPU, DXT, DXT1, DXT2, DXT3, DXT4, DXT5

	// there should be 1 chunk
	// assert(file.ChunksRemaining());

	auto const chunkID = file.BeginChunk();

	assert(chunkID == ChunkID::TextureImageData);

	/* get the image data size */
	uint32_t size;
	file.Read(&size);

	/* use libpng */
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info = png_create_info_struct(png);
	setjmp(png_jmpbuf(png));

	png_set_read_fn(png, (png_voidp)&file, [](png_struct* ps, png_byte* data, png_size_t length) {
		ChunkFile* f = (ChunkFile*)png_get_io_ptr(ps);
		f->Read(data, length);
	});

	png_uint_32 width, height;
	int bit_depth, color_type;

	png_read_info(png, info);
	png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

	// only support these types
	if (color_type != PNG_COLOR_TYPE_PALETTE &&
		color_type != PNG_COLOR_TYPE_RGB &&
		color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
		Log::Error("Unsupported png_color_type: {}\n", color_type);
		png_destroy_read_struct(&png, 0, 0);
		return nullptr; // todo: return error texture
	}

	// Convert transparency to full alpha
	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);
	
	// Convert paletted images to RGB
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png);
	}

	// ensure 8-bit packing
	if (bit_depth < 8) {
		png_set_packing(png);
	} else if (bit_depth == 16) {
		png_set_scale_16(png);
	}

	png_read_update_info(png, info);

	// read our new color type
	color_type = png_get_color_type(png, info);

	size_t row_bytes = png_get_rowbytes(png, info);

	// pull from png
	std::vector<png_byte> image;
	image.resize(row_bytes * height);

	png_bytep* row_pointers = new png_bytep[height];
	for (int i = 0; i < height; ++i)
		row_pointers[i] = image.data() + i * row_bytes;

	png_read_image(png, row_pointers);

	Texture::Format glFormat = Texture::Format::RGB8;
	if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
		glFormat = Texture::Format::RGBA8;
	}

	auto texture = std::make_shared<Texture>();
	texture->Create(width, height, glFormat, image);

	file.EndChunk();

	return texture;
}
} // namespace Donut

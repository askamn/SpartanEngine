/*
Copyright(c) 2016 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES ================
#include "ImageImporter.h"
#include "../IO/Log.h"
#include "../IO/FileSystem.h"
#include "FreeImagePlus.h"
//===========================

//= NAMESPACES =====
using namespace std;
//==================

ImageImporter::ImageImporter()
{
	m_bitmap = nullptr;
	m_bitmap32 = nullptr;
	m_bitmapScaled = nullptr;
	m_bpp = 0;
	m_width = 0;
	m_height = 0;
	m_path = "";
	m_channels = 4;
	m_grayscale = false;
	m_transparent = false;
	m_graphics = nullptr;

	FreeImage_Initialise(true);
}

ImageImporter::~ImageImporter()
{
	Clear();
	FreeImage_DeInitialise();
}

void ImageImporter::Initialize(Graphics* D3D11evice)
{
	m_graphics = D3D11evice;
}

bool ImageImporter::Load(const string& path)
{
	// keep the path
	m_path = path;

	// clear memory in case there are leftovers from last call
	Clear();

	// try to load the image
	return Load(path, 0, 0, false);
}

bool ImageImporter::Load(const string& path, int width, int height)
{
	// keep the path
	m_path = path;

	// clear memory in case there are leftovers from last call
	Clear();

	// try to load the image
	return Load(path, width, height, true);
}

void ImageImporter::Clear()
{
	m_dataRGBA.clear();
	m_dataRGBA.shrink_to_fit();
	m_bitmap = nullptr;
	m_bitmap32 = nullptr;
	m_bitmapScaled = nullptr;
	m_bpp = 0;
	m_width = 0;
	m_height = 0;
	m_path = "";
	m_grayscale = false;
	m_transparent = false;
}

//= PROPERTIES =====================================================
ID3D11ShaderResourceView* ImageImporter::GetAsD3D11ShaderResourceView()
{
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM; // texture format
	unsigned int mipLevels = 7; // 0 for a full mip chain. The mip chain will extend to 1x1 at the lowest level, even if the dimensions aren't square.

	// texture description
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = m_width;
	textureDesc.Height = m_height;
	textureDesc.MipLevels = mipLevels;
	textureDesc.ArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.Format = format;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	// Create 2D texture from texture description
	ID3D11Texture2D* texture = nullptr;
	HRESULT hResult = m_graphics->GetDevice()->CreateTexture2D(&textureDesc, nullptr, &texture);
	if (FAILED(hResult))
	{
		LOG_ERROR("Failed to create ID3D11Texture2D from imported image data while trying to load " + m_path + ".");
		return nullptr;
	}

	// Resource view description
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;

	// Create shader resource view from resource view description
	ID3D11ShaderResourceView* shaderResourceView = nullptr;
	hResult = m_graphics->GetDevice()->CreateShaderResourceView(texture, &srvDesc, &shaderResourceView);
	if (FAILED(hResult))
	{
		LOG_ERROR("Failed to create the shader resource view.");
		return nullptr;
	}

	// Resource data description
	D3D11_SUBRESOURCE_DATA mapResource{};
	mapResource.pSysMem = m_dataRGBA.data();
	mapResource.SysMemPitch = sizeof(unsigned char) * m_width * m_channels;

	// Copy data from memory to the subresource created in non-mappable memory
	m_graphics->GetDeviceContext()->UpdateSubresource(texture, 0, nullptr, mapResource.pSysMem, mapResource.SysMemPitch, 0);

	// Generate mip chain
	m_graphics->GetDeviceContext()->GenerateMips(shaderResourceView);

	return shaderResourceView;
}

unsigned char* ImageImporter::GetRGBA()
{
	return m_dataRGBA.data();
}

unsigned char* ImageImporter::GetRGBACopy()
{
	unsigned char* dataRGBA = new unsigned char[m_width * m_height * 4];

	for (int i = 0; i < m_height; i++)
		for (int j = 0; j < m_width; j++)
		{
			int red = m_dataRGBA[(i * m_width + j) * 4 + 0];
			int green = m_dataRGBA[(i * m_width + j) * 4 + 1];
			int blue = m_dataRGBA[(i * m_width + j) * 4 + 2];
			int alpha = m_dataRGBA[(i * m_width + j) * 4 + 3];

			dataRGBA[(i * m_width + j) * 3 + 0] = red;
			dataRGBA[(i * m_width + j) * 3 + 1] = green;
			dataRGBA[(i * m_width + j) * 3 + 2] = blue;
			dataRGBA[(i * m_width + j) * 3 + 3] = alpha;
		}

	

	return dataRGBA;
}

unsigned char* ImageImporter::GetRGBCopy()
{
	unsigned char* m_dataRGB = new unsigned char[m_width * m_height * 3];

	for (int i = 0; i < m_height; i++)
		for (int j = 0; j < m_width; j++)
		{
			int red = m_dataRGBA[(i * m_width + j) * 4 + 0];
			int green = m_dataRGBA[(i * m_width + j) * 4 + 1];
			int blue = m_dataRGBA[(i * m_width + j) * 4 + 2];

			m_dataRGB[(i * m_width + j) * 3 + 0] = red;
			m_dataRGB[(i * m_width + j) * 3 + 1] = green;
			m_dataRGB[(i * m_width + j) * 3 + 2] = blue;
		}

	return m_dataRGB;
}

unsigned char* ImageImporter::GetAlphaCopy()
{
	unsigned char* m_dataAlpha = new unsigned char[m_width * m_height];

	for (int i = 0; i < m_height; i++)
		for (int j = 0; j < m_width; j++)
		{
			int alpha = m_dataRGBA[(i * m_width + j) * 4 + 3];
			m_dataAlpha[(i * m_width + j)] = alpha;
		}

	return m_dataAlpha;
}

unsigned ImageImporter::GetBPP()
{
	return m_bpp;
}

unsigned ImageImporter::GetWidth()
{
	return m_width;
}

unsigned ImageImporter::GetHeight()
{
	return m_height;
}

bool ImageImporter::IsGrayscale()
{
	return m_grayscale;
}

bool ImageImporter::IsTransparent()
{
	return m_transparent;
}

string ImageImporter::GetPath()
{
	return m_path;
}

bool ImageImporter::Load(const string& path, int width, int height, bool scale)
{
	// Clear any data left from a previous image loading (if necessary)
	Clear();

	if (!FileSystem::FileExists(path))
	{
		LOG_WARNING("Failed to load image \"" + path + "\", it doesn't exist.");
		return false;
	}
	
	// Get the format of the image
	FREE_IMAGE_FORMAT format = FreeImage_GetFileType(path.c_str(), 0);

	// If the image format couldn't be determined
	if (format == FIF_UNKNOWN)
	{
		// Try getting the format from the file extension
		LOG_WARNING("Couldn't determine image format, attempting to get from file extension...");
		format = FreeImage_GetFIFFromFilename(path.c_str());

		if (!FreeImage_FIFSupportsReading(format))
			LOG_WARNING("Detected image format cannot be read.");
	}

	// Get image format, format == -1 means the file was not found
	// but I am checking against it also, just in case.
	if (format == -1 || format == FIF_UNKNOWN)
		return false;

	// Load the image as a FIBITMAP*
	m_bitmap = FreeImage_Load(format, path.c_str());

	// Flip it vertically
	FreeImage_FlipVertical(m_bitmap);

	// Perform any scaling (if necessary)
	if (scale)
		m_bitmapScaled = FreeImage_Rescale(m_bitmap, width, height, FILTER_LANCZOS3);
	else
		m_bitmapScaled = m_bitmap;

	// Convert it to 32 bits (if necessery)
	m_bpp = FreeImage_GetBPP(m_bitmap); // get bits per pixel
	if (m_bpp != 32)
		m_bitmap32 = FreeImage_ConvertTo32Bits(m_bitmapScaled);
	else
		m_bitmap32 = m_bitmapScaled;

	// Store some useful data	
	m_transparent = bool(FreeImage_IsTransparent(m_bitmap32));
	m_path = path;
	m_width = FreeImage_GetWidth(m_bitmap32);
	m_height = FreeImage_GetHeight(m_bitmap32);
	unsigned int bytespp = m_width != 0 ? FreeImage_GetLine(m_bitmap32) / m_width : -1;
	if (bytespp == -1)
		return false;

	// Construct an RGBA array
	for (unsigned int y = 0; y < m_height; y++)
	{
		unsigned char* bits = (unsigned char*)FreeImage_GetScanLine(m_bitmap32, y);
		for (unsigned int x = 0; x < m_width; x++)
		{
			m_dataRGBA.push_back(bits[FI_RGBA_RED]);
			m_dataRGBA.push_back(bits[FI_RGBA_GREEN]);
			m_dataRGBA.push_back(bits[FI_RGBA_BLUE]);
			m_dataRGBA.push_back(bits[FI_RGBA_ALPHA]);

			// jump to next pixel
			bits += bytespp;
		}
	}

	// Store some useful data that require m_dataRGBA to be filled
	m_grayscale = CheckIfGrayscale();

	//= Free memory =====================================
	// unload the 32-bit bitmap
	FreeImage_Unload(m_bitmap32);

	// unload the scaled bitmap only if it was converted
	if (m_bpp != 32)
		FreeImage_Unload(m_bitmapScaled);

	// unload the non 32-bit bitmap only if it was scaled
	if (scale)
		FreeImage_Unload(m_bitmap);
	//====================================================

	return true;
}

bool ImageImporter::CheckIfGrayscale()
{
	int grayPixels = 0;
	int scannedPixels = 0;

	for (int i = 0; i < m_height; i++)
		for (int j = 0; j < m_width; j++)
		{
			scannedPixels++;

			int red = m_dataRGBA[(i * m_width + j) * 4 + 0];
			int green = m_dataRGBA[(i * m_width + j) * 4 + 1];
			int blue = m_dataRGBA[(i * m_width + j) * 4 + 2];

			if (red == green && red == blue)
				grayPixels++;
		}

	if (grayPixels == scannedPixels)
		return true;

	return false;
}
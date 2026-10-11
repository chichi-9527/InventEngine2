#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace INVENT
{
	namespace ITools
	{
		// ===== DDS 文件头 (与 dds 写文件端共用; #pragma pack 保证磁盘布局) =====
#pragma pack(push, 1)
		struct DDS_Header
		{
			std::uint32_t dwMagic;				// "DDS " = 0x20534444
			std::uint32_t dwSize;				// 恒 124
			std::uint32_t dwFlags;
			std::uint32_t dwHeight;
			std::uint32_t dwWidth;
			std::uint32_t dwPitchOrLinearSize;
			std::uint32_t dwDepth;
			std::uint32_t dwMipMapCount;
			std::uint32_t dwReserved1[11];
			// Pixel Format
			std::uint32_t pfSize;				// 恒 32
			std::uint32_t pfFlags;				// 0x4 = FOURCC
			std::uint32_t pfFourCC;				// "DX10" = 0x30315844
			std::uint32_t pfRGBBitCount;
			std::uint32_t pfRBitMask;
			std::uint32_t pfGBitMask;
			std::uint32_t pfBBitMask;
			std::uint32_t pfABitMask;
			// Caps
			std::uint32_t dwCaps;
			std::uint32_t dwCaps2;
			std::uint32_t dwCaps3;
			std::uint32_t dwCaps4;
			std::uint32_t dwReserved2;
		};
		struct DDS_Header_DX10
		{
			std::uint32_t dxgiFormat;
			std::uint32_t resourceDimension;	// 3 = D3D10_RESOURCE_DIMENSION_TEXTURE2D
			std::uint32_t miscFlag;
			std::uint32_t arraySize;
			std::uint32_t miscFlags2;
		};
#pragma pack(pop)
		static_assert(sizeof(DDS_Header) == 128);
		static_assert(sizeof(DDS_Header_DX10) == 20);

		constexpr std::uint32_t DDS_MAGIC = 0x20534444;			// "DDS "
		constexpr std::uint32_t DDS_FOURCC_DX10 = 0x30315844;	// "DX10"
		constexpr std::uint32_t DDS_HEADER_BYTES = sizeof(DDS_Header) + sizeof(DDS_Header_DX10);	// 148

		// ===== 本管线支持的 BC 格式 DXGI 枚举值 =====
		namespace DXGI_BC
		{
			constexpr std::uint32_t BC1_UNORM = 71, BC1_UNORM_SRGB = 72;
			constexpr std::uint32_t BC3_UNORM = 77, BC3_UNORM_SRGB = 78;
			constexpr std::uint32_t BC4_UNORM = 80, BC4_SNORM = 81;
			constexpr std::uint32_t BC5_UNORM = 83, BC5_SNORM = 84;
			constexpr std::uint32_t BC6H_UF16 = 95, BC6H_SF16 = 96;
			constexpr std::uint32_t BC7_UNORM = 98, BC7_UNORM_SRGB = 99;
		}

		/// <returns> VkFormat; 不支持返回 VK_FORMAT_UNDEFINED </returns>
		inline VkFormat DxgiToVkFormat(std::uint32_t dxgi)
		{
			switch (dxgi)
			{
			case DXGI_BC::BC1_UNORM:		return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
			case DXGI_BC::BC1_UNORM_SRGB:	return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
			case DXGI_BC::BC3_UNORM:		return VK_FORMAT_BC3_UNORM_BLOCK;
			case DXGI_BC::BC3_UNORM_SRGB:	return VK_FORMAT_BC3_SRGB_BLOCK;
			case DXGI_BC::BC4_UNORM:		return VK_FORMAT_BC4_UNORM_BLOCK;
			case DXGI_BC::BC4_SNORM:		return VK_FORMAT_BC4_SNORM_BLOCK;
			case DXGI_BC::BC5_UNORM:		return VK_FORMAT_BC5_UNORM_BLOCK;
			case DXGI_BC::BC5_SNORM:		return VK_FORMAT_BC5_SNORM_BLOCK;
			case DXGI_BC::BC6H_UF16:		return VK_FORMAT_BC6H_UFLOAT_BLOCK;
			case DXGI_BC::BC6H_SF16:		return VK_FORMAT_BC6H_SFLOAT_BLOCK;
			case DXGI_BC::BC7_UNORM:		return VK_FORMAT_BC7_UNORM_BLOCK;
			case DXGI_BC::BC7_UNORM_SRGB:	return VK_FORMAT_BC7_SRGB_BLOCK;
			default:						return VK_FORMAT_UNDEFINED;
			}
		}

		/// <returns> 每个 4x4 块的字节数 (BC1/BC4 = 8, 其余 = 16); 非支持格式返回 0 </returns>
		inline std::uint32_t DxgiBlockBytes(std::uint32_t dxgi)
		{
			switch (dxgi)
			{
			case DXGI_BC::BC1_UNORM: case DXGI_BC::BC1_UNORM_SRGB:
			case DXGI_BC::BC4_UNORM: case DXGI_BC::BC4_SNORM:
				return 8;
			case DXGI_BC::BC3_UNORM: case DXGI_BC::BC3_UNORM_SRGB:
			case DXGI_BC::BC5_UNORM: case DXGI_BC::BC5_SNORM:
			case DXGI_BC::BC6H_UF16: case DXGI_BC::BC6H_SF16:
			case DXGI_BC::BC7_UNORM: case DXGI_BC::BC7_UNORM_SRGB:
				return 16;
			default:
				return 0;
			}
		}
	}
}

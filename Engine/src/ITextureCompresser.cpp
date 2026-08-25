#include "ITextureCompresser.h"

#include "IMemPool/IMemPool.h"
#include "IEngineTools.h"
#include "ILog.h"

#include <ispc_texcomp.h>
#include <StbImage/stb_image_resize2.h>

#include <algorithm>

namespace INVENT
{
	bool ITextureCompresser::CompressTextureRGBA(CompressedTextureData& out, 
		const std::uint8_t* pixels, TextureSize size, TextureCompressionType compression_type, std::uint32_t mip_levels)
	{
		if (!pixels || size.width == 0 || size.height == 0 || !out.data || !out.offsets) return false;
		bc7_enc_settings bc7Settings{};
		switch (compression_type)
		{
		case INVENT::ITextureCompresser::TextureCompressionType::BC1:
			break;
		case INVENT::ITextureCompresser::TextureCompressionType::BC3:
			break;
		case INVENT::ITextureCompresser::TextureCompressionType::BC7_RGB:
			GetProfile_fast(&bc7Settings);
			break;
		case INVENT::ITextureCompresser::TextureCompressionType::BC7_RGBA:
			GetProfile_alpha_fast(&bc7Settings);
			break;
		default:
			INVENT_LOG_ERROR("[ITextureCompresser] 不支持的压缩算法!");
			return false;
		}

		auto pool = IEngineTools::Instance().GetMemPoolPool();
		if (pool == nullptr) return false;

		std::uint32_t base_w = (size.width + 3) & ~3;
		std::uint32_t base_h = (size.height + 3) & ~3;
		if (base_w < 4) base_w = 4;
		if (base_h < 4) base_h = 4;
		out.width = base_w;
		out.height = base_h;

		auto cMips = IEngineTools::CalculateMipLevels(out.width, out.height);
		auto finalMips = mip_levels ? std::clamp(mip_levels, uint32_t{ 1 }, cMips) : cMips;
		out.mipLevels = finalMips;
		out.offsets->resize(finalMips);

		// 当前 mip 未压缩的原始数据
		Uint8Vector current_rgba(base_w * base_h * 4, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
		// 处理 Mip 0 層級的縮放
		stbir_resize_uint8_linear(pixels, size.width, size.height, 0,
			current_rgba.data(), base_w, base_h, 0,
			STBIR_RGBA);
		std::uint32_t current_w = base_w;
		std::uint32_t current_h = base_h;

		for (std::uint32_t l = 0; l < finalMips; ++l)
		{
			(*out.offsets)[l] = static_cast<uint32_t>(out.data->size());

			// 計算 BC 塊尺寸和壓縮後的緩衝區大小
			std::uint32_t block_w = (current_w + 3) & ~3;
			std::uint32_t block_h = (current_h + 3) & ~3;
			if (block_w < 4) block_w = 4;
			if (block_h < 4) block_h = 4;

			// 如果當前 Mip 尺寸不等於對齊後的區塊尺寸，需要再次 Resize
			Uint8Vector aligned_rgba{ IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool) };
			if (current_w != block_w || current_h != block_h)
			{
				aligned_rgba.resize(std::uint64_t{ block_w } * block_h * 4);
				stbir_resize_uint8_linear(current_rgba.data(), current_w, current_h, 0,
					aligned_rgba.data(), block_w, block_h, 0,
					STBIR_RGBA);
			}
			else
			{
				aligned_rgba = current_rgba; // 尺寸剛好對齊，直接複製
			}

			

			// 配置 ispc_texcomp 所需的輸入表面結構體体
			rgba_surface surface{};
			surface.ptr = aligned_rgba.data();
			surface.width = block_w;
			surface.height = block_h;
			surface.stride = block_w * 4; // 每一行的位元組數
			// 計算壓縮後 BC7 的輸出大小：BC7 每個 4x4 區塊佔 16 位元組 (若是 BC1 則改為 8 位元組)
			std::uint32_t num_blocks_x = block_w / 4;
			std::uint32_t num_blocks_y = block_h / 4;
			std::uint32_t block_count = num_blocks_x * num_blocks_y;
			std::uint32_t bytes_per_block = (compression_type == TextureCompressionType::BC1) ? 8 : 16;
			std::uint32_t compressed_size = block_count * bytes_per_block;

			Uint8Vector compressed_blocks(compressed_size, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
			switch (compression_type)
			{
			case INVENT::ITextureCompresser::TextureCompressionType::BC1:
				CompressBlocksBC1(&surface, compressed_blocks.data());
				break;
			case INVENT::ITextureCompresser::TextureCompressionType::BC3:
				CompressBlocksBC3(&surface, compressed_blocks.data());
				break;
			case INVENT::ITextureCompresser::TextureCompressionType::BC7_RGB:
			case INVENT::ITextureCompresser::TextureCompressionType::BC7_RGBA:
				CompressBlocksBC7(&surface, compressed_blocks.data(), &bc7Settings);
				break;
			default:
				return false;
			}
			out.data->insert(out.data->end(), compressed_blocks.begin(), compressed_blocks.end());

			std::uint32_t next_w = std::max(uint32_t{ 1 }, current_w / 2);
			std::uint32_t next_h = std::max(uint32_t{ 1 }, current_h / 2);

			if (l < finalMips - 1)
			{
				Uint8Vector next_rgba(next_w * next_h * 4, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
				stbir_resize_uint8_linear(current_rgba.data(), current_w, current_h, 0,
					next_rgba.data(), next_w, next_h, 0,
					STBIR_RGBA);
				current_rgba = std::move(next_rgba);
				current_w = next_w;
				current_h = next_h;
			}

		}
		return true;
	}

	bool ITextureCompresser::CompressTextureR(CompressedTextureData& out, const std::uint8_t* pixels, TextureSize size, std::uint32_t mip_levels)
	{
		if (!pixels || size.width == 0 || size.height == 0 || !out.data || !out.offsets) return false;

		auto pool = IEngineTools::Instance().GetMemPoolPool();
		if (pool == nullptr) return false;

		std::uint32_t base_w = (size.width + 3) & ~3;
		std::uint32_t base_h = (size.height + 3) & ~3;
		if (base_w < 4) base_w = 4;
		if (base_h < 4) base_h = 4;
		out.width = base_w;
		out.height = base_h;

		auto cMips = IEngineTools::CalculateMipLevels(out.width, out.height);
		auto finalMips = mip_levels ? std::clamp(mip_levels, uint32_t{ 1 }, cMips) : cMips;
		out.mipLevels = finalMips;
		out.offsets->resize(finalMips);

		// 当前 mip 未压缩的原始数据
		Uint8Vector current_rgba(base_w * base_h * 1, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
		// 处理 Mip 0 層級的縮放
		stbir_resize_uint8_linear(pixels, size.width, size.height, 0,
			current_rgba.data(), base_w, base_h, 0,
			STBIR_1CHANNEL);
		std::uint32_t current_w = base_w;
		std::uint32_t current_h = base_h;

		for (std::uint32_t l = 0; l < finalMips; ++l)
		{
			(*out.offsets)[l] = static_cast<uint32_t>(out.data->size());

			// 計算 BC 塊尺寸和壓縮後的緩衝區大小
			std::uint32_t block_w = (current_w + 3) & ~3;
			std::uint32_t block_h = (current_h + 3) & ~3;
			if (block_w < 4) block_w = 4;
			if (block_h < 4) block_h = 4;

			// 如果當前 Mip 尺寸不等於對齊後的區塊尺寸，需要再次 Resize
			Uint8Vector aligned_rgba{ IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool) };
			if (current_w != block_w || current_h != block_h)
			{
				aligned_rgba.resize(std::uint64_t{ block_w } * block_h * 1);
				stbir_resize_uint8_linear(current_rgba.data(), current_w, current_h, 0,
					aligned_rgba.data(), block_w, block_h, 0,
					STBIR_1CHANNEL);
			}
			else
			{
				aligned_rgba = current_rgba; // 尺寸剛好對齊，直接複製
			}

			// 配置 ispc_texcomp 所需的輸入表面結構體体
			rgba_surface surface{};
			surface.ptr = aligned_rgba.data();
			surface.width = block_w;
			surface.height = block_h;
			surface.stride = block_w * 1; // 每一行的位元組數
			// BC4 每個 4x4 區塊佔 8 位元組
			std::uint32_t num_blocks_x = block_w / 4;
			std::uint32_t num_blocks_y = block_h / 4;
			std::uint32_t block_count = num_blocks_x * num_blocks_y;
			std::uint32_t compressed_size = block_count * 8;

			Uint8Vector compressed_blocks(compressed_size, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
			CompressBlocksBC4(&surface, compressed_blocks.data());

			out.data->insert(out.data->end(), compressed_blocks.begin(), compressed_blocks.end());

			std::uint32_t next_w = std::max(uint32_t{ 1 }, current_w / 2);
			std::uint32_t next_h = std::max(uint32_t{ 1 }, current_h / 2);

			if (l < finalMips - 1)
			{
				Uint8Vector next_rgba(next_w * next_h * 1, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
				stbir_resize_uint8_linear(current_rgba.data(), current_w, current_h, 0,
					next_rgba.data(), next_w, next_h, 0,
					STBIR_1CHANNEL);
				current_rgba = std::move(next_rgba);
				current_w = next_w;
				current_h = next_h;
			}

		}
		return true;
	}

	bool ITextureCompresser::CompressTextureRG(CompressedTextureData& out, const std::uint8_t* pixels, TextureSize size, std::uint32_t mip_levels)
	{
		if (!pixels || size.width == 0 || size.height == 0 || !out.data || !out.offsets) return false;

		auto pool = IEngineTools::Instance().GetMemPoolPool();
		if (pool == nullptr) return false;

		std::uint32_t base_w = (size.width + 3) & ~3;
		std::uint32_t base_h = (size.height + 3) & ~3;
		if (base_w < 4) base_w = 4;
		if (base_h < 4) base_h = 4;
		out.width = base_w;
		out.height = base_h;

		auto cMips = IEngineTools::CalculateMipLevels(out.width, out.height);
		auto finalMips = mip_levels ? std::clamp(mip_levels, uint32_t{ 1 }, cMips) : cMips;
		out.mipLevels = finalMips;
		out.offsets->resize(finalMips);

		// 当前 mip 未压缩的原始数据
		Uint8Vector current_rgba(base_w * base_h * 2, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
		// 处理 Mip 0 層級的縮放
		stbir_resize_uint8_linear(pixels, size.width, size.height, 0,
			current_rgba.data(), base_w, base_h, 0,
			STBIR_2CHANNEL);
		std::uint32_t current_w = base_w;
		std::uint32_t current_h = base_h;

		for (std::uint32_t l = 0; l < finalMips; ++l)
		{
			(*out.offsets)[l] = static_cast<uint32_t>(out.data->size());

			// 計算 BC 塊尺寸和壓縮後的緩衝區大小
			std::uint32_t block_w = (current_w + 3) & ~3;
			std::uint32_t block_h = (current_h + 3) & ~3;
			if (block_w < 4) block_w = 4;
			if (block_h < 4) block_h = 4;

			// 如果當前 Mip 尺寸不等於對齊後的區塊尺寸，需要再次 Resize
			Uint8Vector aligned_rgba{ IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool) };
			if (current_w != block_w || current_h != block_h)
			{
				aligned_rgba.resize(std::uint64_t{ block_w } *block_h * 2);
				stbir_resize_uint8_linear(current_rgba.data(), current_w, current_h, 0,
					aligned_rgba.data(), block_w, block_h, 0,
					STBIR_2CHANNEL);
			}
			else
			{
				aligned_rgba = current_rgba; // 尺寸剛好對齊，直接複製
			}

			// 配置 ispc_texcomp 所需的輸入表面結構體体
			rgba_surface surface{};
			surface.ptr = aligned_rgba.data();
			surface.width = block_w;
			surface.height = block_h;
			surface.stride = block_w * 2; // 每一行的位元組數
			// BC5 每個 4x4 區塊佔 16 位元組
			std::uint32_t num_blocks_x = block_w / 4;
			std::uint32_t num_blocks_y = block_h / 4;
			std::uint32_t block_count = num_blocks_x * num_blocks_y;
			std::uint32_t compressed_size = block_count * 16;

			Uint8Vector compressed_blocks(compressed_size, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
			CompressBlocksBC4(&surface, compressed_blocks.data());

			out.data->insert(out.data->end(), compressed_blocks.begin(), compressed_blocks.end());

			std::uint32_t next_w = std::max(uint32_t{ 1 }, current_w / 2);
			std::uint32_t next_h = std::max(uint32_t{ 1 }, current_h / 2);

			if (l < finalMips - 1)
			{
				Uint8Vector next_rgba(next_w * next_h * 2, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
				stbir_resize_uint8_linear(current_rgba.data(), current_w, current_h, 0,
					next_rgba.data(), next_w, next_h, 0,
					STBIR_2CHANNEL);
				current_rgba = std::move(next_rgba);
				current_w = next_w;
				current_h = next_h;
			}

		}
		return true;
	}

	bool ITextureCompresser::CompressTextureFP16RGBA(CompressedTextureData& out, const std::uint16_t* pixels, TextureSize size, std::uint32_t mip_levels)
	{
		INVENT_LOG_WARNING("[ITextureCompresser] CompressTextureFP16RGBA 还未实现，若要实现建议手动写一个双线性过滤.");
		return false;
	}


	bool ITextureCompresser::CompressTextureFRGBA(CompressedTextureData& out, const float* pixels, TextureSize size, std::uint32_t mip_levels)
	{
		if (!pixels || size.width == 0 || size.height == 0 || !out.data || !out.offsets) return false;

		auto pool = IEngineTools::Instance().GetMemPoolPool();
		if (pool == nullptr) return false;

		bc6h_enc_settings bc6hSettings{};
		GetProfile_bc6h_fast(&bc6hSettings);

		std::uint32_t base_w = (size.width + 3) & ~3;
		std::uint32_t base_h = (size.height + 3) & ~3;
		if (base_w < 4) base_w = 4;
		if (base_h < 4) base_h = 4;
		out.width = base_w;
		out.height = base_h;

		auto cMips = IEngineTools::CalculateMipLevels(out.width, out.height);
		auto finalMips = mip_levels ? std::clamp(mip_levels, uint32_t{ 1 }, cMips) : cMips;
		out.mipLevels = finalMips;
		out.offsets->resize(finalMips);

		// 当前 mip 未压缩的原始数据
		FloatVector current_rgba(base_w * base_h * 4, IMemPoolAllocatorOnlyFixedBlock<float>(pool));
		// 处理 Mip 0 層級的縮放
		stbir_resize_float_linear(pixels, size.width, size.height, 0,
			current_rgba.data(), base_w, base_h, 0,
			STBIR_RGBA);
		std::uint32_t current_w = base_w;
		std::uint32_t current_h = base_h;

		Uint16Vector current_fp16{ IMemPoolAllocatorOnlyFixedBlock<std::uint16_t>(pool) };

		for (std::uint32_t l = 0; l < finalMips; ++l)
		{
			(*out.offsets)[l] = static_cast<uint32_t>(out.data->size());

			// 計算 BC 塊尺寸和壓縮後的緩衝區大小
			std::uint32_t block_w = (current_w + 3) & ~3;
			std::uint32_t block_h = (current_h + 3) & ~3;
			if (block_w < 4) block_w = 4;
			if (block_h < 4) block_h = 4;

			// 如果當前 Mip 尺寸不等於對齊後的區塊尺寸，需要再次 Resize
			FloatVector aligned_rgba{ IMemPoolAllocatorOnlyFixedBlock<float>(pool) };
			if (current_w != block_w || current_h != block_h)
			{
				aligned_rgba.resize(std::uint64_t{ block_w } *block_h * 4);
				stbir_resize_float_linear(current_rgba.data(), current_w, current_h, 0,
					aligned_rgba.data(), block_w, block_h, 0,
					STBIR_RGBA);
			}
			else
			{
				aligned_rgba = current_rgba; // 尺寸剛好對齊，直接複製
			}
			current_fp16.resize(aligned_rgba.size());
			size_t i = 0;
			for (float code : aligned_rgba)
			{
				current_fp16[i++] = _float_to_half(code);
			}

			// 配置 ispc_texcomp 所需的輸入表面結構體体
			rgba_surface surface{};
			surface.ptr = reinterpret_cast<uint8_t*>(current_fp16.data());
			surface.width = block_w;
			surface.height = block_h;
			surface.stride = static_cast<int32_t>(std::uint64_t{ block_w } * 4 * sizeof(std::uint16_t)); // 每一行的位元組數
			// BC6H 每個 4x4 區塊佔 16 位元組
			std::uint32_t num_blocks_x = block_w / 4;
			std::uint32_t num_blocks_y = block_h / 4;
			std::uint32_t block_count = num_blocks_x * num_blocks_y;
			std::uint32_t compressed_size = block_count * 16;

			Uint8Vector compressed_blocks(compressed_size, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
			CompressBlocksBC6H(&surface, compressed_blocks.data(), &bc6hSettings);

			out.data->insert(out.data->end(), compressed_blocks.begin(), compressed_blocks.end());

			std::uint32_t next_w = std::max(uint32_t{ 1 }, current_w / 2);
			std::uint32_t next_h = std::max(uint32_t{ 1 }, current_h / 2);

			if (l < finalMips - 1)
			{
				FloatVector next_rgba(next_w * next_h * 4, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>(pool));
				stbir_resize_float_linear(current_rgba.data(), current_w, current_h, 0,
					next_rgba.data(), next_w, next_h, 0,
					STBIR_RGBA);
				current_rgba = std::move(next_rgba);
				current_w = next_w;
				current_h = next_h;
			}

		}
		return true;
	}

	std::uint16_t ITextureCompresser::_float_to_half(float f)
	{
		std::uint32_t bits = *reinterpret_cast<std::uint32_t*>(&f);
		std::uint32_t sign = (bits >> 16) & 0x8000;
		int32_t val = (bits & 0x7FFFFFFF) - 0x38000000;
		if (val <= 0) return sign;
		if (val >= 0x47800000) return sign | 0x7C00;
		return sign | ((val >> 13) & 0x7FFF);
	}
}

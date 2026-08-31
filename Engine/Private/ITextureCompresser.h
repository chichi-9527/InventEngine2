#pragma once

#include <cstdint>
#include <vector>

namespace INVENT
{

	template<typename T>
	class IMemPoolAllocatorOnlyFixedBlock;

	class ITextureCompresser
	{
	public:
		using Uint8Vector = std::vector<std::uint8_t, IMemPoolAllocatorOnlyFixedBlock<std::uint8_t>>;
		using Uint16Vector = std::vector<std::uint16_t, IMemPoolAllocatorOnlyFixedBlock<std::uint16_t>>;
		using Uint32Vector = std::vector<std::uint32_t, IMemPoolAllocatorOnlyFixedBlock<std::uint32_t>>;
		using FloatVector = std::vector<float, IMemPoolAllocatorOnlyFixedBlock<float>>;

		enum class TextureCompressionType : std::uint32_t {
			BC1,
			BC3,
			BC7_RGB,
			BC7_RGBA
		};
		struct CompressedTextureData
		{
			Uint8Vector*		data = nullptr;
			Uint32Vector*		offsets = nullptr;
			std::uint32_t       width{ 0 };
			std::uint32_t       height{ 0 };
			std::uint32_t       mipLevels{ 0 };

			operator bool() const noexcept
			{
				return data != nullptr &&
					offsets != nullptr;
			}
		};
		struct TextureSize 
		{
			std::uint32_t width{ 0 };
			std::uint32_t height{ 0 };
		};
	public:
		/// <param name="pixels"> rgba </param>
		/// <param name="size"> 原始大小 </param>
		/// <param name="mip_levels"> 指定层级数量,若为0或超过可生成最大值生成最大值 </param>
		static bool CompressTextureRGBA(CompressedTextureData& out,
			const std::uint8_t* pixels, TextureSize size,
			TextureCompressionType compression_type = TextureCompressionType::BC1,
			std::uint32_t mip_levels = 0);
		/// <summary>
		/// BC4
		/// </summary>
		/// <param name="pixels"> r </param>
		/// <param name="size"> 原始大小 </param>
		/// <param name="mip_levels"> 指定层级数量,若为0或超过可生成最大值生成最大值 </param>
		static bool CompressTextureR(CompressedTextureData& out,
			const std::uint8_t* pixels, TextureSize size,
			std::uint32_t mip_levels = 0);
		/// <summary>
		/// BC5
		/// </summary>
		/// <param name="pixels"> rgba </param>
		/// <param name="size"> 原始大小 </param>
		/// <param name="mip_levels"> 指定层级数量,若为0或超过可生成最大值生成最大值 </param>
		static bool CompressTextureRG(CompressedTextureData& out,
			const std::uint8_t* pixels, TextureSize size,
			std::uint32_t mip_levels = 0);
		/// <summary>
		/// 未实现，若要实现建议手动写一个双线性过滤
		/// </summary>
		/// <param name="pixels"> fp16 rgba </param>
		/// <param name="size"> 原始大小 </param>
		/// <param name="mip_levels"> 指定层级数量,若为0或超过可生成最大值生成最大值 </param>
		static bool CompressTextureFP16RGBA(CompressedTextureData& out,
			const std::uint16_t* pixels, TextureSize size,
			std::uint32_t mip_levels = 0);
		/// <summary>
		/// BC6H DXGI_FORMAT_BC6H_UF16
		/// </summary>
		/// <param name="pixels"> float rgba </param>
		/// <param name="size"> 原始大小 </param>
		/// <param name="mip_levels"> 指定层级数量,若为0或超过可生成最大值生成最大值 </param>
		static bool CompressTextureFRGBA(CompressedTextureData& out,
			const float* pixels, TextureSize size,
			std::uint32_t mip_levels = 0);

	private:
		static std::uint16_t _float_to_half(float f);
	};

}

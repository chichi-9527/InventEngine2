#ifndef _ITEXT_
#define _ITEXT_

#include "EngineAPI.h"

#include <string>

namespace INVENT
{

	class IText
	{
	public:
		// out len min 5 (chinese max 4 byte and \0)
		static INVENT_API size_t ToUtf8FromUInt32(const uint32_t code_point, char out[5]);

	public:
		INVENT_API ~IText() = default;
		INVENT_API IText() = default;
		INVENT_API IText(const IText& other) = default;
		INVENT_API IText(IText&& other) noexcept = default;
		INVENT_API IText& operator=(IText&& other) noexcept = default;

		IText(const std::string& utf8);
		IText(const std::u8string& utf8);

		void ToUtf8(std::string& out_utf8);
		std::string ToUtf8();

		INVENT_API IText(const char* utf8);
		INVENT_API IText(const char8_t* utf8);

		// 在更改并再次调用此函数前指针总是正确的
		INVENT_API const char* ToUtf8CStr();
		

		char32_t operator[](size_t index)
		{
			return _data[index];
		}
		char32_t at(size_t index)
		{
			return _data.at(index);
		}
		constexpr auto begin() const noexcept
		{
			return _data.begin();
		}
		constexpr auto begin() noexcept
		{
			return _data.begin();
		}
		constexpr auto end() const noexcept
		{
			return _data.end();
		}
		constexpr auto end() noexcept
		{
			return _data.end();
		}
		constexpr auto rbegin() const noexcept
		{
			return _data.rbegin();
		}
		constexpr auto rbegin() noexcept
		{
			return _data.rbegin();
		}
		constexpr auto rend() const noexcept
		{
			return _data.rend();
		}
		constexpr auto rend() noexcept
		{
			return _data.rend();
		}

		constexpr size_t size() const noexcept
		{
			return _data.size();
		}

	private:
		void _from_utf8(const void* utf8_str, size_t str_size);
		
	private:
		std::u32string _data;
		std::string _utf8_cache;
	};


}

//#ifdef __cplusplus
//extern "C"
//{
//#endif // _cplusplus
//	INVENT_API INVENT::IText* INVENT_DLL InventCreateIText(const char* utf8_str);
//	INVENT_API INVENT::IText* INVENT_DLL InventCreateITextFromChar8(const char8_t* utf8_str);
//	INVENT_API size_t INVENT_DLL InventITextToUtf8(const INVENT::IText* text, char* out_buffer, size_t buffer_size);
//	INVENT_API void INVENT_DLL InventDestoryIText(INVENT::IText* text);
//#ifdef __cplusplus
//}
//#endif // _cplusplus



#endif // !_ITEXT_
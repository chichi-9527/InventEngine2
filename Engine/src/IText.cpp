#include "IText.h"

#include "ILog.h"

#include <stdexcept>

namespace INVENT
{
    size_t IText::ToUtf8FromUInt32(const uint32_t code_point, char out[5])
    {
        std::memset(out, 0, 5);
        size_t size = 0;
        if (code_point <= 0x7F)
        {
            out[0] = static_cast<char>(code_point);
            size = 1;
        }
        else if (code_point <= 0x7FF)
        {
            out[0] = static_cast<char>(0xC0 | ((code_point >> 6) & 0x1F));
            out[1] = static_cast<char>(0x80 | (code_point & 0x3F));
            size = 2;
        }
        else if (code_point <= 0xFFFF)
        {
            out[0] = static_cast<char>(0xE0 | ((code_point >> 12) & 0x0F));
            out[1] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
            out[2] = static_cast<char>(0x80 | (code_point & 0x3F));
            size = 3;
        }
        else if (code_point <= 0x10FFFF)
        {
            out[0] = static_cast<char>(0xF0 | ((code_point >> 18) & 0x07));
            out[1] = static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
            out[2] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
            out[3] = static_cast<char>(0x80 | (code_point & 0x3F));
            size = 4;
        }
        else
        {
            ILog::Error(std::format("[IText] utf-32 code error. code point : U+{:04X}.", static_cast<uint32_t>(code_point)));
        }
        return size;
    }

    IText::IText(const std::string& utf8)
	{
        _from_utf8(utf8.data(), utf8.size());
	}

	IText::IText(const std::u8string& utf8)
	{
		_from_utf8(utf8.data(), utf8.size());
	}

	void IText::ToUtf8(std::string& out_utf8)
	{
        out_utf8.clear();
        out_utf8.reserve(_data.size() * 3);

        for (auto code_point : _data)
        {
            if (code_point <= 0x7F)
            {
                out_utf8.push_back(static_cast<char>(code_point));
            }
            else if (code_point <= 0x7FF)
            {
                out_utf8.push_back(static_cast<char>(0xC0 | ((code_point >> 6) & 0x1F)));
                out_utf8.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
            }
            else if (code_point <= 0xFFFF)
            {
                out_utf8.push_back(static_cast<char>(0xE0 | ((code_point >> 12) & 0x0F)));
                out_utf8.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
                out_utf8.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
            }
            else if (code_point <= 0x10FFFF)
            {
                out_utf8.push_back(static_cast<char>(0xF0 | ((code_point >> 18) & 0x07)));
                out_utf8.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
                out_utf8.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
                out_utf8.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
            }
            else
            {
                ILog::Error(std::format("[IText] utf-32 code error. code point : U+{:04X}.", static_cast<uint32_t>(code_point)));
            }
        }
	}

	std::string IText::ToUtf8()
	{
		std::string out;
		ToUtf8(out);
		return out;
	}

	void IText::_from_utf8(const void* utf8_str, size_t str_size)
	{
		_data.clear();
        size_t i = 0;

        auto utf8Str = reinterpret_cast<const uint8_t*>(utf8_str);
        while (i < str_size)
        {
            auto byte1 = utf8Str[i];
            char32_t cp = 0;

            // 1字节：0xxxxxxx
            if ((byte1 & 0x80) == 0x00)
            {           
                cp = byte1;
                i += 1;
            }
            // 2字节：110xxxxx
            else if ((byte1 & 0xE0) == 0xC0)
            {      
                if (i + 1 >= str_size) break;
                uint8_t byte2 = static_cast<uint8_t>(utf8Str[i + 1]);
                cp = ((byte1 & 0x1F) << 6) | (byte2 & 0x3F);
                i += 2;
            }
            // 3字节：1110xxxx
            else if ((byte1 & 0xF0) == 0xE0)
            {      
                if (i + 2 >= str_size) break;
                uint8_t byte2 = static_cast<uint8_t>(utf8Str[i + 1]);
                uint8_t byte3 = static_cast<uint8_t>(utf8Str[i + 2]);
                cp = ((byte1 & 0x0F) << 12) | ((byte2 & 0x3F) << 6) | (byte3 & 0x3F);
                i += 3;
            }
            // 4字节：11110xxx
            else if ((byte1 & 0xF8) == 0xF0)
            {      
                if (i + 3 >= str_size) break;
                uint8_t byte2 = static_cast<uint8_t>(utf8Str[i + 1]);
                uint8_t byte3 = static_cast<uint8_t>(utf8Str[i + 2]);
                uint8_t byte4 = static_cast<uint8_t>(utf8Str[i + 3]);
                cp = ((byte1 & 0x07) << 18) | ((byte2 & 0x3F) << 12) | ((byte3 & 0x3F) << 6) | (byte4 & 0x3F);
                i += 4;
            }
            else
            {
                throw std::runtime_error(std::format("utf8 str 格式不正确: 0x{:X}.", byte1));
            }

            _data += cp;

        }// while
		
	}


}

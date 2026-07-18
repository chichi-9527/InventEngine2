#ifndef _ITEXT_
#define _ITEXT_

#include "EngineAPI.h"

#include <string>

namespace INVENT
{

	class INVENT_API IText
	{
	public:
		~IText() = default;
		IText() = default;
		IText(const IText& other) = default;
		IText(IText&& other) noexcept = default;
		IText& operator=(IText&& other) noexcept = default;

		IText(const std::string& utf8);

		void ToUtf8(std::string& out_utf8);

	private:
		void _from_utf8(const std::string& utf8_str);
		
	private:
		std::u32string _data;
	};


}



#endif // !_ITEXT_
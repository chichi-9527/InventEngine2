#include "IText.h"

#include "ILog.h"


namespace INVENT
{
	IText::IText(const std::string& utf8)
	{
		_from_utf8(utf8);
	}

	void IText::ToUtf8(std::string& out_utf8)
	{
		out_utf8.clear();
		
	}

	void IText::_from_utf8(const std::string& utf8_str)
	{
		_data.clear();
		
	}
}

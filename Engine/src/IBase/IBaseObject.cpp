#include "IBase/IBaseObject.h"

namespace INVENT
{
	ObjectRegistry& ObjectRegistry::Instance()
	{
		static ObjectRegistry o;
		return o;
	}

	void ObjectRegistry::RegisterClass(const char* name, ObjectCreatorFunc creator)
	{
		if (name != nullptr && creator != nullptr)
			_registries[name] = creator;
	}

	IUniquePtr<IBaseObject> ObjectRegistry::Create(const char* name) const
	{
		if (name == nullptr) return nullptr;
		auto iter = _registries.find(name);
		if (iter == _registries.end()) return nullptr;
		return (iter->second)();
	}

	void ObjectRegistry::Clear()
	{
		_registries.clear();
	}
}

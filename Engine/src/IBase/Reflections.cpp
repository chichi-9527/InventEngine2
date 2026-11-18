#include "IBase/IBaseObject.h"
#include "IMath/IMathematics.h"

#include <entt/meta/meta.hpp>
#include <entt/meta/factory.hpp>

#include <cstdint>

namespace INVENT
{
	static bool RegBuilth()
	{
		auto& ctx = GetEngineEnttMetaContext();
		entt::meta_factory<void>{ctx}.type("void");
		entt::meta_factory<float>{ctx}.type("float");
		entt::meta_factory<double>{ctx}.type("double");
		entt::meta_factory<std::int8_t>{ctx}.type("std_int8");
		entt::meta_factory<std::int16_t>{ctx}.type("std_int16");
		entt::meta_factory<std::int32_t>{ctx}.type("std_int32");
		entt::meta_factory<std::int64_t>{ctx}.type("std_int64");
		entt::meta_factory<std::uint8_t>{ctx}.type("std_uint8");
		entt::meta_factory<std::uint16_t>{ctx}.type("std_uint16");
		entt::meta_factory<std::uint32_t>{ctx}.type("std_uint32");
		entt::meta_factory<std::uint64_t>{ctx}.type("std_uint64");
		entt::meta_factory<std::string>{ctx}.type("std_string");
		return true;
	}

	template<typename T>
	void RegBaseVec2(entt::meta_ctx& ctx, const char* name)
	{
		entt::meta_factory<base_vec2<T>>{ctx}.type(name)
			.data<&base_vec2<T>::x>("x")
			.data<&base_vec2<T>::y>("y");
	}
	template<typename T>
	void RegBaseVec3(entt::meta_ctx& ctx, const char* name)
	{
		entt::meta_factory<base_vec3<T>>{ctx}.type(name)
			.data<&base_vec3<T>::x>("x")
			.data<&base_vec3<T>::y>("y")
			.data<&base_vec3<T>::z>("z");
	}
	template<typename T>
	void RegBaseVec4(entt::meta_ctx& ctx, const char* name)
	{
		entt::meta_factory<base_vec4<T>>{ctx}.type(name)
			.data<&base_vec4<T>::x>("x")
			.data<&base_vec4<T>::y>("y")
			.data<&base_vec4<T>::z>("z")
			.data<&base_vec4<T>::w>("w");
	}
	template<typename T>
	void RegBaseQuat(entt::meta_ctx& ctx, const char* name)
	{
		entt::meta_factory<base_quat<T>>{ctx}.type(name)
			.data<&base_quat<T>::x>("x")
			.data<&base_quat<T>::y>("y")
			.data<&base_quat<T>::z>("z")
			.data<&base_quat<T>::w>("w");
	}

	static bool RegMath()
	{
		auto& ctx = GetEngineEnttMetaContext();
		// vec
		//
		RegBaseVec2<float>(ctx, "Vec2");
		RegBaseVec2<double>(ctx, "DVec2");
		//
		RegBaseVec3<float>(ctx, "Vec3");
		RegBaseVec3<double>(ctx, "DVec3");
		//
		RegBaseVec4<float>(ctx, "Vec4");
		RegBaseVec4<double>(ctx, "DVec4");

		// quat
		//
		RegBaseQuat<float>(ctx, "Quat");
		RegBaseQuat<double>(ctx, "Quat2");

		return true;
	}

	entt::meta_ctx& GetEngineEnttMetaContext()
	{
		static entt::meta_ctx ctx;
		return ctx;
	}

}

namespace
{
	[[maybe_unused]] const bool _autoRegBuilth = INVENT::RegBuilth();
	[[maybe_unused]] const bool _autoRegMath = INVENT::RegMath();


}


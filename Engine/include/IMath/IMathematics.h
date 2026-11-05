#pragma once

#include "EngineAPI.h"

#include <cstdint>

namespace INVENT
{
	template<typename T>
	struct base_vec2
	{
		T x{};
		T y{};
		base_vec2() = default;
		base_vec2(T v) : x(v), y(v) {}
		base_vec2(T in_x, T in_y) : x(in_x), y(in_y) {}
		base_vec2<T> operator+(const base_vec2<T>& other) const
		{
			return { x + other.x, y + other.y };
		}
		base_vec2<T> operator-(const base_vec2<T>& other) const
		{
			return { x - other.x, y - other.y };
		}
	};
	using Vec2		= base_vec2<float>;
	using DVec2		= base_vec2<double>;
	
	using I8Vec2	= base_vec2<std::int8_t>;
	using I16Vec2	= base_vec2<std::int16_t>;
	using I32Vec2	= base_vec2<std::int32_t>;
	using IVec2		= base_vec2<std::int32_t>;
	using I64Vec2	= base_vec2<std::int64_t>;

	using U8Vec2	= base_vec2<std::uint8_t>;
	using U16Vec2	= base_vec2<std::uint16_t>;
	using U32Vec2	= base_vec2<std::uint32_t>;
	using UVec2		= base_vec2<std::uint32_t>;
	using U64Vec2	= base_vec2<std::uint64_t>;


	template<typename T>
	struct base_vec3
	{
		T x{};
		T y{};
		T z{};
		base_vec3() = default;
		base_vec3(T v) : x(v), y(v), z(v) {}
		base_vec3(const base_vec2<T>& vec2, T in_z) : x(vec2.x), y(vec2.y), z(in_z) {}
		base_vec3(T in_x, T in_y, T in_z) : x(in_x), y(in_y), z(in_z) {}
		base_vec3<T> operator+(const base_vec3<T>& other) const
		{
			return { x + other.x,y + other.y,z + other.z };
		}
		base_vec3<T> operator-(const base_vec3<T>& other) const
		{
			return { x - other.x, y - other.y,z - other.z };
		}
	};
	using Vec3		= base_vec3<float>;
	using DVec3		= base_vec3<double>;
							  
	using I8Vec3	= base_vec3<std::int8_t>;
	using I16Vec3	= base_vec3<std::int16_t>;
	using I32Vec3	= base_vec3<std::int32_t>;
	using IVec3		= base_vec3<std::int32_t>;
	using I64Vec3	= base_vec3<std::int64_t>;
							  
	using U8Vec3	= base_vec3<std::uint8_t>;
	using U16Vec3	= base_vec3<std::uint16_t>;
	using U32Vec3	= base_vec3<std::uint32_t>;
	using UVec3		= base_vec3<std::uint32_t>;
	using U64Vec3	= base_vec3<std::uint64_t>;


	template<typename T>
	struct base_vec4
	{
		T x{};
		T y{};
		T z{};
		T w{};
		base_vec4() = default;
		base_vec4(T v) : x(v), y(v), z(v), w(v) {}
		base_vec4(const base_vec2<T>& vec2) : x(vec2.x), y(vec2.y) {}
		base_vec4(const base_vec3<T>& vec3) : x(vec3.x), y(vec3.y), z(vec3.z) {}
		base_vec4(T in_x, T in_y, T in_z, T in_w) : x(in_x), y(in_y), z(in_z), w(in_w) {}
		base_vec4<T> operator+(const base_vec4<T>& other) const
		{ 
			return { x + other.x,y + other.y,z + other.z,w + other.w };
		}
		base_vec4<T> operator-(const base_vec4<T>& other) const
		{
			return { x - other.x,y - other.y,z - other.z,w - other.w };
		}
	};
	using Vec4		= base_vec4<float>;
	using DVec4		= base_vec4<double>;
							  
	using I8Vec4	= base_vec4<std::int8_t>;
	using I16Vec4	= base_vec4<std::int16_t>;
	using I32Vec4	= base_vec4<std::int32_t>;
	using IVec4		= base_vec4<std::int32_t>;
	using I64Vec4	= base_vec4<std::int64_t>;
							  
	using U8Vec4	= base_vec4<std::uint8_t>;
	using U16Vec4	= base_vec4<std::uint16_t>;
	using U32Vec4	= base_vec4<std::uint32_t>;
	using UVec4		= base_vec4<std::uint32_t>;
	using U64Vec4	= base_vec4<std::uint64_t>;



	template<typename T>
	struct base_mat4x4
	{

	private:
		base_vec4<T> vecs[4];
	};

}

#pragma once

#include "EngineAPI.h"

#include <cstdint>

namespace INVENT
{
	struct DVec3;
	struct Vec2;
	struct Vec3;

	template<typename T, typename Derived>
	struct base_vec2
	{
		T x{};
		T y{};
		base_vec2() = default;
		base_vec2(T v) : x(v), y(v) {}
		base_vec2(T in_x, T in_y) : x(in_x), y(in_y) {}
		Derived operator+(const Derived& other) const 
		{
			return Derived{ static_cast<const Derived*>(this)->x + other.x,
							static_cast<const Derived*>(this)->y + other.y };
		}
		Derived operator-(const Derived& other) const
		{
			return Derived{ static_cast<const Derived*>(this)->x - other.x,
					   static_cast<const Derived*>(this)->y - other.y };
		}
	};
	template<typename T, typename Derived>
	struct base_vec3
	{
		T x{};
		T y{};
		T z{};
		base_vec3() = default;
		base_vec3(T v) : x(v), y(v), z(v) {}
		template<typename _>
		base_vec3(const base_vec2<T, _>& vec2) : x(vec2.x), y(vec2.y) {}
		base_vec3(T in_x, T in_y, T in_z) : x(in_x), y(in_y), z(in_z) {}
		Derived operator+(Derived& other) const 
		{
			return Derived{ static_cast<const Derived*>(this)->x + other.x,
					   static_cast<const Derived*>(this)->y + other.y,
					   static_cast<const Derived*>(this)->z + other.z };
		}
		Derived operator-(Derived& other) const 
		{
			return Derived{ static_cast<const Derived*>(this)->x - other.x,
					   static_cast<const Derived*>(this)->y - other.y,
					   static_cast<const Derived*>(this)->z - other.z };
		}
	};
	template<typename T, typename Derived>
	struct base_vec4
	{
		T x{};
		T y{};
		T z{};
		T w{};
		base_vec4() = default;
		base_vec4(T v) : x(v), y(v), z(v), w(v) {}
		template<typename _>
		base_vec4(const base_vec2<T, _>& vec2) : x(vec2.x), y(vec2.y) {}
		template<typename _>
		base_vec4(const base_vec3<T, _>& vec3) : x(vec3.x), y(vec3.y), z(vec3.z) {}
		base_vec4(T in_x, T in_y, T in_z, T in_w) : x(in_x), y(in_y), z(in_z), w(in_w) {}
		Derived operator+(const Derived& other) const 
		{ 
			return Derived{ static_cast<const Derived*>(this)->x + other.x,
					   static_cast<const Derived*>(this)->y + other.y,
					   static_cast<const Derived*>(this)->z + other.z,
					   static_cast<const Derived*>(this)->w + other.w };
		}
		Derived operator-(const Derived& other) const 
		{
			return Derived{ static_cast<const Derived*>(this)->x - other.x,
					  static_cast<const Derived*>(this)->y - other.y,
					  static_cast<const Derived*>(this)->z - other.z,
					  static_cast<const Derived*>(this)->w - other.w };
		}
	};


	struct DVec3 : public base_vec3<double, DVec3>
	{
		using base_vec3<double, DVec3>::base_vec3;
	};

	struct Vec2 : public base_vec2<float, Vec2>
	{
		using base_vec2<float, Vec2>::base_vec2;
	};

	struct Vec3 : public base_vec3<float, Vec3>
	{
		using base_vec3<float, Vec3>::base_vec3;
	};
}

#pragma once

#include "EngineAPI.h"

namespace INVENT
{
	struct Vec2
	{
		float x{ 0.0f };
		float y{ 0.0f };

		Vec2() = default;
		Vec2(float inX, float inY) : x(inX), y(inY) {}

		Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
	};

	struct Vec3
	{
		float x{ 0.0f };
		float y{ 0.0f };
		float z{ 0.0f };

		Vec3() = default;
		Vec3(float inX, float inY, float inZ) : x(inX), y(inY), z(inZ) {}

		Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
	};
}

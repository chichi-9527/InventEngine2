#pragma once

#include "EngineAPI.h"

#include <cstddef>
#include <cstdint>

namespace INVENT
{
	template<typename T> struct base_vec2;
	template<typename T> struct base_vec3;
	template<typename T> struct base_vec4;
	template<typename T> struct base_quat;

	template<typename T>
	struct base_vec2 {
		T x{ T(0) };
		T y{ T(0) };

		base_vec2() = default;
		explicit base_vec2(T v) : x(v), y(v) {}
		base_vec2(T in_x, T in_y) : x(in_x), y(in_y) {}
		template<typename U>
		explicit base_vec2(const base_vec2<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}

		T operator[](size_t idx) const { return (&x)[idx]; }
		T& operator[](size_t idx) { return (&x)[idx]; }

		base_vec2 operator-() const { return { -x, -y }; }

		base_vec2 operator+(T s) const { return { x + s, y + s }; }
		base_vec2 operator-(T s) const { return { x - s, y - s }; }
		base_vec2 operator*(T s) const { return { x * s, y * s }; }
		base_vec2 operator/(T s) const { return { x / s, y / s }; }

		base_vec2 operator+(const base_vec2& o) const { return { x + o.x, y + o.y }; }
		base_vec2 operator-(const base_vec2& o) const { return { x - o.x, y - o.y }; }
		base_vec2 operator*(const base_vec2& o) const { return { x * o.x, y * o.y }; }
		base_vec2 operator/(const base_vec2& o) const { return { x / o.x, y / o.y }; }

		base_vec2& operator+=(T s) { x += s; y += s; return *this; }
		base_vec2& operator-=(T s) { x -= s; y -= s; return *this; }
		base_vec2& operator*=(T s) { x *= s; y *= s; return *this; }
		base_vec2& operator/=(T s) { x /= s; y /= s; return *this; }

		base_vec2& operator+=(const base_vec2& o) { x += o.x; y += o.y; return *this; }
		base_vec2& operator-=(const base_vec2& o) { x -= o.x; y -= o.y; return *this; }
		base_vec2& operator*=(const base_vec2& o) { x *= o.x; y *= o.y; return *this; }
		base_vec2& operator/=(const base_vec2& o) { x /= o.x; y /= o.y; return *this; }

		bool operator==(const base_vec2& o) const { return x == o.x && y == o.y; }
		bool operator!=(const base_vec2& o) const { return !(*this == o); }
	};

	template<typename T>
	struct base_vec3 {
		T x{ T(0) };
		T y{ T(0) };
		T z{ T(0) };

		base_vec3() = default;
		explicit base_vec3(T v) : x(v), y(v), z(v) {}
		base_vec3(T in_x, T in_y, T in_z) : x(in_x), y(in_y), z(in_z) {}
		base_vec3(const base_vec2<T>& v2, T in_z = T(0)) : x(v2.x), y(v2.y), z(in_z) {}
		template<typename U>
		explicit base_vec3(const base_vec3<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) {}

		T operator[](size_t idx) const { return (&x)[idx]; }
		T& operator[](size_t idx) { return (&x)[idx]; }

		base_vec3 operator-() const { return { -x, -y, -z }; }

		base_vec3 operator+(T s) const { return { x + s, y + s, z + s }; }
		base_vec3 operator-(T s) const { return { x - s, y - s, z - s }; }
		base_vec3 operator*(T s) const { return { x * s, y * s, z * s }; }
		base_vec3 operator/(T s) const { return { x / s, y / s, z / s }; }

		base_vec3 operator+(const base_vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
		base_vec3 operator-(const base_vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
		base_vec3 operator*(const base_vec3& o) const { return { x * o.x, y * o.y, z * o.z }; }
		base_vec3 operator/(const base_vec3& o) const { return { x / o.x, y / o.y, z / o.z }; }

		base_vec3& operator+=(T s) { x += s; y += s; z += s; return *this; }
		base_vec3& operator-=(T s) { x -= s; y -= s; z -= s; return *this; }
		base_vec3& operator*=(T s) { x *= s; y *= s; z *= s; return *this; }
		base_vec3& operator/=(T s) { x /= s; y /= s; z /= s; return *this; }

		base_vec3& operator+=(const base_vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
		base_vec3& operator-=(const base_vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
		base_vec3& operator*=(const base_vec3& o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
		base_vec3& operator/=(const base_vec3& o) { x /= o.x; y /= o.y; z /= o.z; return *this; }

		bool operator==(const base_vec3& o) const { return x == o.x && y == o.y && z == o.z; }
		bool operator!=(const base_vec3& o) const { return !(*this == o); }
	};
	
	template<typename T>
	struct base_vec4 {
		T x{ T(0) };
		T y{ T(0) };
		T z{ T(0) };
		T w{ T(0) };

		base_vec4() = default;
		explicit base_vec4(T v) : x(v), y(v), z(v), w(v) {}
		base_vec4(T in_x, T in_y, T in_z, T in_w) : x(in_x), y(in_y), z(in_z), w(in_w) {}
		base_vec4(const base_vec2<T>& v2, T in_z = T(0), T in_w = T(0)) : x(v2.x), y(v2.y), z(in_z), w(in_w) {}
		base_vec4(const base_vec3<T>& v3, T in_w = T(0)) : x(v3.x), y(v3.y), z(v3.z), w(in_w) {}
		template<typename U>
		explicit base_vec4(const base_vec4<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)), w(static_cast<T>(v.w)) {}

		T operator[](size_t idx) const { return (&x)[idx]; }
		T& operator[](size_t idx) { return (&x)[idx]; }

		base_vec4 operator-() const { return { -x, -y, -z, -w }; }

		base_vec4 operator+(T s) const { return { x + s, y + s, z + s, w + s }; }
		base_vec4 operator-(T s) const { return { x - s, y - s, z - s, w - s }; }
		base_vec4 operator*(T s) const { return { x * s, y * s, z * s, w * s }; }
		base_vec4 operator/(T s) const { return { x / s, y / s, z / s, w / s }; }

		base_vec4 operator+(const base_vec4& o) const { return { x + o.x, y + o.y, z + o.z, w + o.w }; }
		base_vec4 operator-(const base_vec4& o) const { return { x - o.x, y - o.y, z - o.z, w - o.w }; }
		base_vec4 operator*(const base_vec4& o) const { return { x * o.x, y * o.y, z * o.z, w * o.w }; }
		base_vec4 operator/(const base_vec4& o) const { return { x / o.x, y / o.y, z / o.z, w / o.w }; }

		base_vec4& operator+=(T s) { x += s; y += s; z += s; w += s; return *this; }
		base_vec4& operator-=(T s) { x -= s; y -= s; z -= s; w -= s; return *this; }
		base_vec4& operator*=(T s) { x *= s; y *= s; z *= s; w *= s; return *this; }
		base_vec4& operator/=(T s) { x /= s; y /= s; z /= s; w /= s; return *this; }

		base_vec4& operator+=(const base_vec4& o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
		base_vec4& operator-=(const base_vec4& o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
		base_vec4& operator*=(const base_vec4& o) { x *= o.x; y *= o.y; z *= o.z; w *= o.w; return *this; }
		base_vec4& operator/=(const base_vec4& o) { x /= o.x; y /= o.y; z /= o.z; w /= o.w; return *this; }

		bool operator==(const base_vec4& o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
		bool operator!=(const base_vec4& o) const { return !(*this == o); }
	};

	template<typename T> inline base_vec2<T> operator*(T s, const base_vec2<T>& v) { return v * s; }
	template<typename T> inline base_vec3<T> operator*(T s, const base_vec3<T>& v) { return v * s; }
	template<typename T> inline base_vec4<T> operator*(T s, const base_vec4<T>& v) { return v * s; }
	template<typename T> inline base_vec2<T> operator+(T s, const base_vec2<T>& v) { return v + s; }
	template<typename T> inline base_vec3<T> operator+(T s, const base_vec3<T>& v) { return v + s; }
	template<typename T> inline base_vec4<T> operator+(T s, const base_vec4<T>& v) { return v + s; }

	// 為了避免 9 個矩陣重複寫相同的加減法與純量乘除，定義一組通用矩陣運算巨集或輔助
#define DEFINE_MAT_COMMON_OPS(MatType, ColCount) \
    MatType operator-() const { \
        MatType res; \
        for (size_t i = 0; i < ColCount; ++i) res.vecs[i] = -vecs[i]; \
        return res; \
    } \
    MatType operator+(const MatType& o) const { \
        MatType res; \
        for (size_t i = 0; i < ColCount; ++i) res.vecs[i] = vecs[i] + o.vecs[i]; \
        return res; \
    } \
    MatType operator-(const MatType& o) const { \
        MatType res; \
        for (size_t i = 0; i < ColCount; ++i) res.vecs[i] = vecs[i] - o.vecs[i]; \
        return res; \
    } \
    MatType operator*(T s) const { \
        MatType res; \
        for (size_t i = 0; i < ColCount; ++i) res.vecs[i] = vecs[i] * s; \
        return res; \
    } \
    MatType operator/(T s) const { \
        MatType res; \
        for (size_t i = 0; i < ColCount; ++i) res.vecs[i] = vecs[i] / s; \
        return res; \
    } \
    MatType& operator+=(const MatType& o) { \
        for (size_t i = 0; i < ColCount; ++i) vecs[i] += o.vecs[i]; \
        return *this; \
    } \
    MatType& operator-=(const MatType& o) { \
        for (size_t i = 0; i < ColCount; ++i) vecs[i] -= o.vecs[i]; \
        return *this; \
    } \
    MatType& operator*=(T s) { \
        for (size_t i = 0; i < ColCount; ++i) vecs[i] *= s; \
        return *this; \
    } \
    MatType& operator/=(T s) { \
        for (size_t i = 0; i < ColCount; ++i) vecs[i] /= s; \
        return *this; \
    } \
    bool operator==(const MatType& o) const { \
        for (size_t i = 0; i < ColCount; ++i) if (vecs[i] != o.vecs[i]) return false; \
        return true; \
    } \
    bool operator!=(const MatType& o) const { return !(*this == o); }

	// --- 2 行系列 (2 Columns) ---
	template<typename T>
	struct base_mat2x2 {
		base_mat2x2() : vecs{ {T(1), T(0)}, {T(0), T(1)} } {}
		explicit base_mat2x2(T s) : vecs{ {s, T(0)}, {T(0), s} } {}
		base_mat2x2(const base_vec2<T>& v0, const base_vec2<T>& v1) : vecs{ v0, v1 } {}
		template<typename U> explicit base_mat2x2(const base_mat2x2<U>& m) : vecs{ base_vec2<T>(m[0]), base_vec2<T>(m[1])} {}

		const base_vec2<T>& operator[](size_t idx) const { return vecs[idx]; }
		base_vec2<T>& operator[](size_t idx) { return vecs[idx]; }
		DEFINE_MAT_COMMON_OPS(base_mat2x2, 2)
	private:
		base_vec2<T> vecs[2];
	};

	template<typename T>
	struct base_mat2x3 {
		base_mat2x3() : vecs{ {T(1), T(0), T(0)}, {T(0), T(1), T(0)} } {}
		explicit base_mat2x3(T s) : vecs{ {s, T(0), T(0)}, {T(0), s, T(0)} } {}
		base_mat2x3(const base_vec3<T>& v0, const base_vec3<T>& v1) : vecs{ v0, v1 } {}
		template<typename U> explicit base_mat2x3(const base_mat2x3<U>& m) : vecs{ base_vec3<T>(m[0]), base_vec3<T>(m[1])} {}

		const base_vec3<T>& operator[](size_t idx) const { return vecs[idx]; }
		base_vec3<T>& operator[](size_t idx) { return vecs[idx]; }
		DEFINE_MAT_COMMON_OPS(base_mat2x3, 2)
	private:
		base_vec3<T> vecs[2];
	};

	template<typename T>
	struct base_mat2x4 {
		base_mat2x4() : vecs{ {T(1), T(0), T(0), T(0)}, {T(0), T(1), T(0), T(0)} } {}
		explicit base_mat2x4(T s) : vecs{ {s, T(0), T(0), T(0)}, {T(0), s, T(0), T(0)} } {}
		base_mat2x4(const base_vec4<T>& v0, const base_vec4<T>& v1) : vecs{ v0, v1 } {}
		template<typename U> explicit base_mat2x4(const base_mat2x4<U>& m) : vecs{ base_vec4<T>(m[0]), base_vec4<T>(m[1])} {}

		const base_vec4<T>& operator[](size_t idx) const { return vecs[idx]; }
		base_vec4<T>& operator[](size_t idx) { return vecs[idx]; }
		DEFINE_MAT_COMMON_OPS(base_mat2x4, 2)
	private:
		base_vec4<T> vecs[2];
	};

	// --- 3 行系列 (3 Columns) ---
	template<typename T>
	struct base_mat3x2 {
		base_mat3x2() : vecs{ {T(1), T(0)}, {T(0), T(1)}, {T(0), T(0)} } {}
		explicit base_mat3x2(T s) : vecs{ {s, T(0)}, {T(0), s}, {T(0), T(0)} } {}
		base_mat3x2(const base_vec2<T>& v0, const base_vec2<T>& v1, const base_vec2<T>& v2) : vecs{ v0, v1, v2 } {}
		template<typename U> 
		explicit base_mat3x2(const base_mat3x2<U>& m) : vecs{ base_vec2<T>(m[0]), base_vec2<T>(m[1]), base_vec2<T>(m[2])} {}

		const base_vec2<T>& operator[](size_t idx) const { return vecs[idx]; }
		base_vec2<T>& operator[](size_t idx) { return vecs[idx]; }
		DEFINE_MAT_COMMON_OPS(base_mat3x2, 3)
	private:
		base_vec2<T> vecs[3];
	};

	template<typename T>
	struct base_mat3x3 {
		base_mat3x3() : vecs{ {T(1), T(0), T(0)}, {T(0), T(1), T(0)}, {T(0), T(0), T(1)} } {}
		explicit base_mat3x3(T s) : vecs{ {s, T(0), T(0)}, {T(0), s, T(0)}, {T(0), T(0), s} } {}
		base_mat3x3(const base_vec3<T>& v0, const base_vec3<T>& v1, const base_vec3<T>& v2) : vecs{ v0, v1, v2 } {}
		template<typename U> 
		explicit base_mat3x3(const base_mat3x3<U>& m) : vecs{ base_vec3<T>(m[0]), base_vec3<T>(m[1]), base_vec3<T>(m[2])} {}

		const base_vec3<T>& operator[](size_t idx) const { return vecs[idx]; }
		base_vec3<T>& operator[](size_t idx) { return vecs[idx]; }
		DEFINE_MAT_COMMON_OPS(base_mat3x3, 3)
	private:
		base_vec3<T> vecs[3];
	};

	template<typename T>
	struct base_mat3x4 {
		base_mat3x4() : vecs{ {T(1), T(0), T(0), T(0)}, {T(0), T(1), T(0), T(0)}, {T(0), T(0), T(1), T(0)} } {}
		explicit base_mat3x4(T s) : vecs{ {s, T(0), T(0), T(0)}, {T(0), s, T(0), T(0)}, {T(0), T(0), s, T(0)} } {}
		base_mat3x4(const base_vec4<T>& v0, const base_vec4<T>& v1, const base_vec4<T>& v2) : vecs{ v0, v1, v2 } {}
		template<typename U>
		explicit base_mat3x4(const base_mat3x4<U>& m) : vecs{ base_vec4<T>(m[0]), base_vec4<T>(m[1]), base_vec4<T>(m[2])} {}

		const base_vec4<T>& operator[](size_t idx) const { return vecs[idx]; }
		base_vec4<T>& operator[](size_t idx) { return vecs[idx]; }
		DEFINE_MAT_COMMON_OPS(base_mat3x4, 3)
	private:
		base_vec4<T> vecs[3];
	};

	// --- 4 行系列 (4 Columns) ---
	template<typename T>
	struct base_mat4x2 {
		base_mat4x2() : vecs{ {T(1), T(0)}, {T(0), T(1)}, {T(0), T(0)}, {T(0), T(0)} } {}
		explicit base_mat4x2(T s) : vecs{ {s, T(0)}, {T(0), s}, {T(0), T(0)}, {T(0), T(0)} } {}
		base_mat4x2(const base_vec2<T>& v0, const base_vec2<T>& v1, const base_vec2<T>& v2, const base_vec2<T>& v3) : vecs{ v0, v1, v2, v3 } {}
		template<typename U>
		explicit base_mat4x2(const base_mat4x2<U>& m) : vecs{ base_vec2<T>(m[0]), base_vec2<T>(m[1]), base_vec2<T>(m[2]), base_vec2<T>(m[3])} {}

		const base_vec2<T>& operator[](size_t idx) const { return vecs[idx]; }
		base_vec2<T>& operator[](size_t idx) { return vecs[idx]; }
		DEFINE_MAT_COMMON_OPS(base_mat4x2, 4)
	private:
		base_vec2<T> vecs[4];
	};

	template<typename T>
	struct base_mat4x3 {
		base_mat4x3() : vecs{ {T(1), T(0), T(0)}, {T(0), T(1), T(0)}, {T(0), T(0), T(1)}, {T(0), T(0), T(0)} } {}
		explicit base_mat4x3(T s) : vecs{ {s, T(0), T(0)}, {T(0), s, T(0)}, {T(0), T(0), s}, {T(0), T(0), T(0)} } {}
		base_mat4x3(const base_vec3<T>& v0, const base_vec3<T>& v1, const base_vec3<T>& v2, const base_vec3<T>& v3) : vecs{ v0, v1, v2, v3 } {}
		template<typename U>
		explicit base_mat4x3(const base_mat4x3<U>& m) : vecs{ base_vec3<T>(m[0]), base_vec3<T>(m[1]), base_vec3<T>(m[2]), base_vec3<T>(m[3])} {}

		const base_vec3<T>& operator[](size_t idx) const { return vecs[idx]; }
		base_vec3<T>& operator[](size_t idx) { return vecs[idx]; }
		DEFINE_MAT_COMMON_OPS(base_mat4x3, 4)
	private:
		base_vec3<T> vecs[4];
	};

	template<typename T>
	struct base_mat4x4 {
		base_mat4x4() : vecs{ {T(1), T(0), T(0), T(0)}, {T(0), T(1), T(0), T(0)}, {T(0), T(0), T(1), T(0)}, {T(0), T(0), T(0), T(1)} } {}
		explicit base_mat4x4(T s) : vecs{ {s, T(0), T(0), T(0)}, {T(0), s, T(0), T(0)}, {T(0), T(0), s, T(0)}, {T(0), T(0), T(0), s} } {}
		base_mat4x4(const base_vec4<T>& v0, const base_vec4<T>& v1, const base_vec4<T>& v2, const base_vec4<T>& v3) : vecs{ v0, v1, v2, v3 } {}
		template<typename U>
		explicit base_mat4x4(const base_mat4x4<U>& m) : vecs{ base_vec4<T>(m[0]), base_vec4<T>(m[1]), base_vec4<T>(m[2]), base_vec4<T>(m[3])} {}

		const base_vec4<T>& operator[](size_t idx) const { return vecs[idx]; }
		base_vec4<T>& operator[](size_t idx) { return vecs[idx]; }
		DEFINE_MAT_COMMON_OPS(base_mat4x4, 4)
	private:
		base_vec4<T> vecs[4];
	};

#undef DEFINE_MAT_COMMON_OPS

	// 支援純量在左側的矩陣乘法 (例如: 2.0f * mat4)
	template<typename T> inline base_mat2x2<T> operator*(T s, const base_mat2x2<T>& m) { return m * s; }
	template<typename T> inline base_mat2x3<T> operator*(T s, const base_mat2x3<T>& m) { return m * s; }
	template<typename T> inline base_mat2x4<T> operator*(T s, const base_mat2x4<T>& m) { return m * s; }
	template<typename T> inline base_mat3x2<T> operator*(T s, const base_mat3x2<T>& m) { return m * s; }
	template<typename T> inline base_mat3x3<T> operator*(T s, const base_mat3x3<T>& m) { return m * s; }
	template<typename T> inline base_mat3x4<T> operator*(T s, const base_mat3x4<T>& m) { return m * s; }
	template<typename T> inline base_mat4x2<T> operator*(T s, const base_mat4x2<T>& m) { return m * s; }
	template<typename T> inline base_mat4x3<T> operator*(T s, const base_mat4x3<T>& m) { return m * s; }
	template<typename T> inline base_mat4x4<T> operator*(T s, const base_mat4x4<T>& m) { return m * s; }

	//  四元數 (Quaternion)
	template<typename T>
	struct base_quat {
		// 預設 GLM 記憶體順序為 x, y, z, w (注意建構子參數順序是 w, x, y, z！)
		T x{ T(0) };
		T y{ T(0) };
		T z{ T(0) };
		T w{ T(1) };

		base_quat() = default;
		// 與 glm::quat(w, x, y, z) 完全一致的參數順序
		base_quat(T in_w, T in_x, T in_y, T in_z) : x(in_x), y(in_y), z(in_z), w(in_w) {}
		// 從純量 w 與向量 (x, y, z) 建構
		base_quat(T s, const base_vec3<T>& v) : x(v.x), y(v.y), z(v.z), w(s) {}
		template<typename U>
		explicit base_quat(const base_quat<U>& q) : x(static_cast<T>(q.x)), y(static_cast<T>(q.y)), z(static_cast<T>(q.z)), w(static_cast<T>(q.w)) {}

		T operator[](size_t idx) const
		{
			switch (idx)
			{
			case 0: return x;
			case 1: return y;
			case 2: return z;
			case 3: return w;
			default: return x;
			}
		}
		T& operator[](size_t idx)
		{
			switch (idx)
			{
			case 0: return x;
			case 1: return y;
			case 2: return z;
			case 3: return w;
			default: return x;
			}
		}
		base_quat<T> operator-() const { return { -w, -x, -y, -z }; }
		base_quat<T> operator+(const base_quat<T>& o) const { return { w + o.w, x + o.x, y + o.y, z + o.z }; }
		base_quat<T> operator-(const base_quat<T>& o) const { return { w - o.w, x - o.x, y - o.y, z - o.z }; }
		base_quat<T> operator*(T s) const { return { w * s, x * s, y * s, z * s }; }
		base_quat<T> operator/(T s) const { return { w / s, x / s, y / s, z / s }; }
		base_quat<T>& operator+=(const base_quat<T>& o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
		base_quat<T>& operator-=(const base_quat<T>& o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
		base_quat<T>& operator*=(T s) { x *= s; y *= s; z *= s; w *= s; return *this; }
		base_quat<T>& operator/=(T s) { x /= s; y /= s; z /= s; w /= s; return *this; }
		bool operator==(const base_quat<T>& o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
		bool operator!=(const base_quat<T>& o) const { return !(*this == o); }
	};

	template<typename T>
	inline base_quat<T> operator*(T s, const base_quat<T>& q) { return q * s; }

	// 型別別名 (Float 單精度 & Double 雙精度 LWC 支援)

	// 單精度 (Float)
	using Vec2 = base_vec2<float>;
	using Vec3 = base_vec3<float>;
	using Vec4 = base_vec4<float>;
	using Mat2 = base_mat2x2<float>;
	using Mat2x2 = base_mat2x2<float>;
	using Mat2x3 = base_mat2x3<float>;
	using Mat2x4 = base_mat2x4<float>;
	using Mat3 = base_mat3x3<float>;
	using Mat3x2 = base_mat3x2<float>;
	using Mat3x3 = base_mat3x3<float>;
	using Mat3x4 = base_mat3x4<float>;
	using Mat4 = base_mat4x4<float>;
	using Mat4x2 = base_mat4x2<float>;
	using Mat4x3 = base_mat4x3<float>;
	using Mat4x4 = base_mat4x4<float>;
	using Mat = base_mat4x4<float>;
	using Quat = base_quat<float>;
	// 雙精度 (Double)
	using DVec2 = base_vec2<double>;
	using DVec3 = base_vec3<double>;
	using DVec4 = base_vec4<double>;
	using DMat2 = base_mat2x2<double>;
	using DMat2x2 = base_mat2x2<double>;
	using DMat2x3 = base_mat2x3<double>;
	using DMat2x4 = base_mat2x4<double>;
	using DMat3 = base_mat3x3<double>;
	using DMat3x2 = base_mat3x2<double>;
	using DMat3x3 = base_mat3x3<double>;
	using DMat3x4 = base_mat3x4<double>;
	using DMat4 = base_mat4x4<double>;
	using DMat4x2 = base_mat4x2<double>;
	using DMat4x3 = base_mat4x3<double>;
	using DMat4x4 = base_mat4x4<double>;
	using DMat = base_mat4x4<double>;
	using DQuat = base_quat<double>;

	namespace Math
	{
		// =========================================================================
		// 1. 運算子重載 (矩陣與四元數乘法)
		// =========================================================================
		
		// 常用方陣相乘
		INVENT_API Mat2  INVENT_DLL operator*(const Mat2& a, const Mat2& b);
		INVENT_API Mat3  INVENT_DLL operator*(const Mat3& a, const Mat3& b);
		INVENT_API Mat4  INVENT_DLL operator*(const Mat4& a, const Mat4& b);
		INVENT_API DMat2 INVENT_DLL operator*(const DMat2& a, const DMat2& b);
		INVENT_API DMat3 INVENT_DLL operator*(const DMat3& a, const DMat3& b);
		INVENT_API DMat4 INVENT_DLL operator*(const DMat4& a, const DMat4& b);

		// 矩陣變換向量
		INVENT_API Vec2  INVENT_DLL operator*(const Mat2& m, const Vec2& v);
		INVENT_API Vec3  INVENT_DLL operator*(const Mat3& m, const Vec3& v);
		INVENT_API Vec4  INVENT_DLL operator*(const Mat4& m, const Vec4& v);
		INVENT_API DVec2 INVENT_DLL operator*(const DMat2& m, const DVec2& v);
		INVENT_API DVec3 INVENT_DLL operator*(const DMat3& m, const DVec3& v);
		INVENT_API DVec4 INVENT_DLL operator*(const DMat4& m, const DVec4& v);

		// 四元數乘法與旋轉向量
		INVENT_API Quat  INVENT_DLL operator*(const Quat& a, const Quat& b);
		INVENT_API Vec3  INVENT_DLL operator*(const Quat& q, const Vec3& v);
		INVENT_API DQuat INVENT_DLL operator*(const DQuat& a, const DQuat& b);
		INVENT_API DVec3 INVENT_DLL operator*(const DQuat& q, const DVec3& v);

		// =========================================================================
		// 2. 向量幾何運算 (Vector Geometry)
		// =========================================================================

		// 長度與距離
		INVENT_API float  INVENT_DLL Length(const Vec2& v);
		INVENT_API float  INVENT_DLL Length(const Vec3& v);
		INVENT_API float  INVENT_DLL Length(const Vec4& v);
		INVENT_API double INVENT_DLL Length(const DVec2& v);
		INVENT_API double INVENT_DLL Length(const DVec3& v);
		INVENT_API double INVENT_DLL Length(const DVec4& v);

		INVENT_API float  INVENT_DLL LengthSq(const Vec2& v);
		INVENT_API float  INVENT_DLL LengthSq(const Vec3& v);
		INVENT_API float  INVENT_DLL LengthSq(const Vec4& v);
		INVENT_API double INVENT_DLL LengthSq(const DVec2& v);
		INVENT_API double INVENT_DLL LengthSq(const DVec3& v);
		INVENT_API double INVENT_DLL LengthSq(const DVec4& v);

		INVENT_API float  INVENT_DLL Distance(const Vec2& a, const Vec2& b);
		INVENT_API float  INVENT_DLL Distance(const Vec3& a, const Vec3& b);
		INVENT_API float  INVENT_DLL Distance(const Vec4& a, const Vec4& b);
		INVENT_API double INVENT_DLL Distance(const DVec2& a, const DVec2& b);
		INVENT_API double INVENT_DLL Distance(const DVec3& a, const DVec3& b);
		INVENT_API double INVENT_DLL Distance(const DVec4& a, const DVec4& b);

		// 正規化
		INVENT_API Vec2  INVENT_DLL Normalize(const Vec2& v);
		INVENT_API Vec3  INVENT_DLL Normalize(const Vec3& v);
		INVENT_API Vec4  INVENT_DLL Normalize(const Vec4& v);
		INVENT_API DVec2 INVENT_DLL Normalize(const DVec2& v);
		INVENT_API DVec3 INVENT_DLL Normalize(const DVec3& v);
		INVENT_API DVec4 INVENT_DLL Normalize(const DVec4& v);

		// 內積、外積與反射
		INVENT_API float  INVENT_DLL Dot(const Vec2& a, const Vec2& b);
		INVENT_API float  INVENT_DLL Dot(const Vec3& a, const Vec3& b);
		INVENT_API float  INVENT_DLL Dot(const Vec4& a, const Vec4& b);
		INVENT_API double INVENT_DLL Dot(const DVec2& a, const DVec2& b);
		INVENT_API double INVENT_DLL Dot(const DVec3& a, const DVec3& b);
		INVENT_API double INVENT_DLL Dot(const DVec4& a, const DVec4& b);

		INVENT_API Vec3  INVENT_DLL Cross(const Vec3& a, const Vec3& b);
		INVENT_API DVec3 INVENT_DLL Cross(const DVec3& a, const DVec3& b);

		INVENT_API Vec2  INVENT_DLL Reflect(const Vec2& incident, const Vec2& normal);
		INVENT_API Vec3  INVENT_DLL Reflect(const Vec3& incident, const Vec3& normal);
		INVENT_API DVec2 INVENT_DLL Reflect(const DVec2& incident, const DVec2& normal);
		INVENT_API DVec3 INVENT_DLL Reflect(const DVec3& incident, const DVec3& normal);

		// =========================================================================
		// 3. 插值與數值限制 (Interpolation & Clamping)
		// =========================================================================

		INVENT_API float  INVENT_DLL Lerp(float a, float b, float t);
		INVENT_API double INVENT_DLL Lerp(double a, double b, double t);
		INVENT_API Vec2   INVENT_DLL Lerp(const Vec2& a, const Vec2& b, float t);
		INVENT_API Vec3   INVENT_DLL Lerp(const Vec3& a, const Vec3& b, float t);
		INVENT_API Vec4   INVENT_DLL Lerp(const Vec4& a, const Vec4& b, float t);
		INVENT_API DVec2  INVENT_DLL Lerp(const DVec2& a, const DVec2& b, double t);
		INVENT_API DVec3  INVENT_DLL Lerp(const DVec3& a, const DVec3& b, double t);
		INVENT_API DVec4  INVENT_DLL Lerp(const DVec4& a, const DVec4& b, double t);

		INVENT_API float  INVENT_DLL Clamp(float val, float minVal, float maxVal);
		INVENT_API double INVENT_DLL Clamp(double val, double minVal, double maxVal);
		INVENT_API Vec2   INVENT_DLL Clamp(const Vec2& val, const Vec2& minVal, const Vec2& maxVal);
		INVENT_API Vec3   INVENT_DLL Clamp(const Vec3& val, const Vec3& minVal, const Vec3& maxVal);
		INVENT_API Vec4   INVENT_DLL Clamp(const Vec4& val, const Vec4& minVal, const Vec4& maxVal);
		INVENT_API DVec2  INVENT_DLL Clamp(const DVec2& val, const DVec2& minVal, const DVec2& maxVal);
		INVENT_API DVec3  INVENT_DLL Clamp(const DVec3& val, const DVec3& minVal, const DVec3& maxVal);
		INVENT_API DVec4  INVENT_DLL Clamp(const DVec4& val, const DVec4& minVal, const DVec4& maxVal);

		INVENT_API float  INVENT_DLL SmoothStep(float edge0, float edge1, float x);
		INVENT_API double INVENT_DLL SmoothStep(double edge0, double edge1, double x);

		INVENT_API float  INVENT_DLL Radians(float degrees);
		INVENT_API double INVENT_DLL Radians(double degrees);
		INVENT_API float  INVENT_DLL Degrees(float radians);
		INVENT_API double INVENT_DLL Degrees(double radians);

		// =========================================================================
		// 4. 矩陣基礎操作與變換 (Matrix Operations)
		// =========================================================================

		INVENT_API Mat2  INVENT_DLL Inverse(const Mat2& m);
		INVENT_API Mat3  INVENT_DLL Inverse(const Mat3& m);
		INVENT_API Mat4  INVENT_DLL Inverse(const Mat4& m);
		INVENT_API DMat2 INVENT_DLL Inverse(const DMat2& m);
		INVENT_API DMat3 INVENT_DLL Inverse(const DMat3& m);
		INVENT_API DMat4 INVENT_DLL Inverse(const DMat4& m);

		INVENT_API Mat2  INVENT_DLL Transpose(const Mat2& m);
		INVENT_API Mat3  INVENT_DLL Transpose(const Mat3& m);
		INVENT_API Mat4  INVENT_DLL Transpose(const Mat4& m);
		INVENT_API DMat2 INVENT_DLL Transpose(const DMat2& m);
		INVENT_API DMat3 INVENT_DLL Transpose(const DMat3& m);
		INVENT_API DMat4 INVENT_DLL Transpose(const DMat4& m);

		INVENT_API float  INVENT_DLL Determinant(const Mat2& m);
		INVENT_API float  INVENT_DLL Determinant(const Mat3& m);
		INVENT_API float  INVENT_DLL Determinant(const Mat4& m);
		INVENT_API double INVENT_DLL Determinant(const DMat2& m);
		INVENT_API double INVENT_DLL Determinant(const DMat3& m);
		INVENT_API double INVENT_DLL Determinant(const DMat4& m);

		// 變換矩陣建構
		INVENT_API Mat4  INVENT_DLL Translate(const Mat4& m, const Vec3& v);
		INVENT_API Mat4  INVENT_DLL Rotate(const Mat4& m, float angleRadians, const Vec3& axis);
		INVENT_API Mat4  INVENT_DLL Scale(const Mat4& m, const Vec3& v);
		INVENT_API DMat4 INVENT_DLL Translate(const DMat4& m, const DVec3& v);
		INVENT_API DMat4 INVENT_DLL Rotate(const DMat4& m, double angleRadians, const DVec3& axis);
		INVENT_API DMat4 INVENT_DLL Scale(const DMat4& m, const DVec3& v);

		// 組合與拆解 Transform (遊戲物件最常用)
		INVENT_API Mat4  INVENT_DLL ComposeTransform(const Vec3& translation, const Quat& rotation, const Vec3& scale);
		INVENT_API bool  INVENT_DLL DecomposeTransform(const Mat4& transform, Vec3& outTranslation, Quat& outRotation, Vec3& outScale);
		INVENT_API DMat4 INVENT_DLL ComposeTransform(const DVec3& translation, const DQuat& rotation, const DVec3& scale);
		INVENT_API bool  INVENT_DLL DecomposeTransform(const DMat4& transform, DVec3& outTranslation, DQuat& outRotation, DVec3& outScale);

		// =========================================================================
		// 5. 四元數旋轉操作 (Quaternion Operations)
		// =========================================================================

		INVENT_API Quat  INVENT_DLL Normalize(const Quat& q);
		INVENT_API DQuat INVENT_DLL Normalize(const DQuat& q);

		INVENT_API Quat  INVENT_DLL Inverse(const Quat& q);
		INVENT_API DQuat INVENT_DLL Inverse(const DQuat& q);

		INVENT_API Quat  INVENT_DLL Conjugate(const Quat& q);
		INVENT_API DQuat INVENT_DLL Conjugate(const DQuat& q);

		INVENT_API float  INVENT_DLL Dot(const Quat& a, const Quat& b);
		INVENT_API double INVENT_DLL Dot(const DQuat& a, const DQuat& b);

		INVENT_API Quat  INVENT_DLL Slerp(const Quat& a, const Quat& b, float t);
		INVENT_API DQuat INVENT_DLL Slerp(const DQuat& a, const DQuat& b, double t);

		// 建構與格式互轉
		INVENT_API Quat  INVENT_DLL QuatFromAxisAngle(const Vec3& axis, float angleRadians);
		INVENT_API DQuat INVENT_DLL QuatFromAxisAngle(const DVec3& axis, double angleRadians);

		INVENT_API Quat  INVENT_DLL QuatFromEuler(const Vec3& eulerRadians);
		INVENT_API DQuat INVENT_DLL QuatFromEuler(const DVec3& eulerRadians);
		INVENT_API Vec3  INVENT_DLL QuatToEuler(const Quat& q);
		INVENT_API DVec3 INVENT_DLL QuatToEuler(const DQuat& q);

		INVENT_API Quat  INVENT_DLL QuatLookRotation(const Vec3& forward, const Vec3& up = Vec3(0.0f, 1.0f, 0.0f));
		INVENT_API DQuat INVENT_DLL QuatLookRotation(const DVec3& forward, const DVec3& up = DVec3(0.0, 1.0, 0.0));

		INVENT_API Mat3  INVENT_DLL QuatToMat3(const Quat& q);
		INVENT_API Mat4  INVENT_DLL QuatToMat4(const Quat& q);
		INVENT_API Quat  INVENT_DLL MatToQuat(const Mat3& m);
		INVENT_API Quat  INVENT_DLL MatToQuat(const Mat4& m);
		INVENT_API DMat3 INVENT_DLL QuatToMat3(const DQuat& q);
		INVENT_API DMat4 INVENT_DLL QuatToMat4(const DQuat& q);
		INVENT_API DQuat INVENT_DLL MatToQuat(const DMat3& m);
		INVENT_API DQuat INVENT_DLL MatToQuat(const DMat4& m);

	}
}

// =========================================================================
// C ABI (供 C# / 其他語言 P/Invoke 使用)
// 命名規則：InventMath + 類別 + 操作 + F(float)/D(double)
// =========================================================================

#define INVENT_C_API_VEC2(SUFFIX, VT, ST) \
    INVENT_API void INVENT_DLL InventMathVec2Add##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec2Sub##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec2Mul##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec2Div##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec2MulScalar##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v, IEngine_IN ST s); \
    INVENT_API void INVENT_DLL InventMathVec2Length##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathVec2LengthSq##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathVec2Distance##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec2Normalize##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathVec2Dot##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec2Reflect##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* incident, IEngine_IN INVENT::VT* normal); \
    INVENT_API void INVENT_DLL InventMathVec2Lerp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b, IEngine_IN ST t); \
    INVENT_API void INVENT_DLL InventMathVec2Clamp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* val, IEngine_IN INVENT::VT* minVal, IEngine_IN INVENT::VT* maxVal);

#define INVENT_C_API_VEC3(SUFFIX, VT, ST) \
    INVENT_API void INVENT_DLL InventMathVec3Add##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec3Sub##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec3Mul##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec3Div##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec3MulScalar##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v, IEngine_IN ST s); \
    INVENT_API void INVENT_DLL InventMathVec3Length##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathVec3LengthSq##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathVec3Distance##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec3Normalize##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathVec3Dot##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec3Cross##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec3Reflect##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* incident, IEngine_IN INVENT::VT* normal); \
    INVENT_API void INVENT_DLL InventMathVec3Lerp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b, IEngine_IN ST t); \
    INVENT_API void INVENT_DLL InventMathVec3Clamp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* val, IEngine_IN INVENT::VT* minVal, IEngine_IN INVENT::VT* maxVal);

#define INVENT_C_API_VEC4(SUFFIX, VT, ST) \
    INVENT_API void INVENT_DLL InventMathVec4Add##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec4Sub##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec4Mul##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec4Div##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec4MulScalar##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v, IEngine_IN ST s); \
    INVENT_API void INVENT_DLL InventMathVec4Length##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathVec4LengthSq##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathVec4Distance##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec4Normalize##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathVec4Dot##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b); \
    INVENT_API void INVENT_DLL InventMathVec4Lerp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b, IEngine_IN ST t); \
    INVENT_API void INVENT_DLL InventMathVec4Clamp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* val, IEngine_IN INVENT::VT* minVal, IEngine_IN INVENT::VT* maxVal);

#define INVENT_C_API_MAT2(SUFFIX, MT, VT, ST) \
    INVENT_API void INVENT_DLL InventMathTwoMat2Multiply##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* a, IEngine_IN INVENT::MT* b); \
    INVENT_API void INVENT_DLL InventMathMat2MultiplyVec2##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::MT* m, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathMat2Inverse##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m); \
    INVENT_API void INVENT_DLL InventMathMat2Transpose##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m); \
    INVENT_API void INVENT_DLL InventMathMat2Determinant##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::MT* m);

#define INVENT_C_API_MAT3(SUFFIX, MT, VT, ST) \
    INVENT_API void INVENT_DLL InventMathTwoMat3Multiply##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* a, IEngine_IN INVENT::MT* b); \
    INVENT_API void INVENT_DLL InventMathMat3MultiplyVec3##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::MT* m, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathMat3Inverse##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m); \
    INVENT_API void INVENT_DLL InventMathMat3Transpose##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m); \
    INVENT_API void INVENT_DLL InventMathMat3Determinant##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::MT* m);

#define INVENT_C_API_MAT4(SUFFIX, MT, VT, ST) \
    INVENT_API void INVENT_DLL InventMathTwoMat4Multiply##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* a, IEngine_IN INVENT::MT* b); \
    INVENT_API void INVENT_DLL InventMathMat4MultiplyVec4##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::MT* m, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathMat4Inverse##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m); \
    INVENT_API void INVENT_DLL InventMathMat4Transpose##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m); \
    INVENT_API void INVENT_DLL InventMathMat4Determinant##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::MT* m);

#define INVENT_C_API_MAT4_EXTRA(SUFFIX, MT, VT, QT, ST) \
    INVENT_API void INVENT_DLL InventMathMat4Translate##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathMat4Rotate##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m, IEngine_IN ST angleRadians, IEngine_IN INVENT::VT* axis); \
    INVENT_API void INVENT_DLL InventMathMat4Scale##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathMat4ComposeTransform##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::VT* translation, IEngine_IN INVENT::QT* rotation, IEngine_IN INVENT::VT* scale); \
    INVENT_API void INVENT_DLL InventMathMat4DecomposeTransform##SUFFIX(IEngine_OUT int* outOk, IEngine_OUT INVENT::VT* outTranslation, IEngine_OUT INVENT::QT* outRotation, IEngine_OUT INVENT::VT* outScale, IEngine_IN INVENT::MT* transform);

#define INVENT_C_API_QUAT(SUFFIX, QT, VT, MT3, MT4, ST) \
    INVENT_API void INVENT_DLL InventMathQuatMultiply##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::QT* a, IEngine_IN INVENT::QT* b); \
    INVENT_API void INVENT_DLL InventMathQuatRotateVec3##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::QT* q, IEngine_IN INVENT::VT* v); \
    INVENT_API void INVENT_DLL InventMathQuatNormalize##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::QT* q); \
    INVENT_API void INVENT_DLL InventMathQuatInverse##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::QT* q); \
    INVENT_API void INVENT_DLL InventMathQuatConjugate##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::QT* q); \
    INVENT_API void INVENT_DLL InventMathQuatDot##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::QT* a, IEngine_IN INVENT::QT* b); \
    INVENT_API void INVENT_DLL InventMathQuatSlerp##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::QT* a, IEngine_IN INVENT::QT* b, IEngine_IN ST t); \
    INVENT_API void INVENT_DLL InventMathQuatFromAxisAngle##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::VT* axis, IEngine_IN ST angleRadians); \
    INVENT_API void INVENT_DLL InventMathQuatFromEuler##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::VT* eulerRadians); \
    INVENT_API void INVENT_DLL InventMathQuatToEuler##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::QT* q); \
    INVENT_API void INVENT_DLL InventMathQuatLookRotation##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::VT* forward, IEngine_IN INVENT::VT* up); \
    INVENT_API void INVENT_DLL InventMathQuatToMat3##SUFFIX(IEngine_OUT INVENT::MT3* out, IEngine_IN INVENT::QT* q); \
    INVENT_API void INVENT_DLL InventMathQuatToMat4##SUFFIX(IEngine_OUT INVENT::MT4* out, IEngine_IN INVENT::QT* q); \
    INVENT_API void INVENT_DLL InventMathMat3ToQuat##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::MT3* m); \
    INVENT_API void INVENT_DLL InventMathMat4ToQuat##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::MT4* m);

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

	// 純量
	INVENT_API void INVENT_DLL InventMathLerpF(float* out, float a, float b, float t);
	INVENT_API void INVENT_DLL InventMathLerpD(double* out, double a, double b, double t);

	INVENT_API void INVENT_DLL InventMathClampF(float* out, float val, float minVal, float maxVal);
	INVENT_API void INVENT_DLL InventMathClampD(double* out, double val, double minVal, double maxVal);

	INVENT_API void INVENT_DLL InventMathSmoothStepF(float* out, float edge0, float edge1, float x);
	INVENT_API void INVENT_DLL InventMathSmoothStepD(double* out, double edge0, double edge1, double x);

	INVENT_API void INVENT_DLL InventMathRadiansF(float* out, float degrees);
	INVENT_API void INVENT_DLL InventMathRadiansD(double* out, double degrees);

	INVENT_API void INVENT_DLL InventMathDegreesF(float* out, float radians);
	INVENT_API void INVENT_DLL InventMathDegreesD(double* out, double radians);

	// 向量
	INVENT_C_API_VEC2(F, Vec2, float)
		INVENT_C_API_VEC2(D, DVec2, double)

		INVENT_C_API_VEC3(F, Vec3, float)
		INVENT_C_API_VEC3(D, DVec3, double)

		INVENT_C_API_VEC4(F, Vec4, float)
		INVENT_C_API_VEC4(D, DVec4, double)

		// 矩陣
		INVENT_C_API_MAT2(F, Mat2, Vec2, float)
		INVENT_C_API_MAT2(D, DMat2, DVec2, double)

		INVENT_C_API_MAT3(F, Mat3, Vec3, float)
		INVENT_C_API_MAT3(D, DMat3, DVec3, double)

		INVENT_C_API_MAT4(F, Mat4, Vec4, float)
		INVENT_C_API_MAT4(D, DMat4, DVec4, double)

		INVENT_C_API_MAT4_EXTRA(F, Mat4, Vec3, Quat, float)
		INVENT_C_API_MAT4_EXTRA(D, DMat4, DVec3, DQuat, double)

		// 四元數
		INVENT_C_API_QUAT(F, Quat, Vec3, Mat3, Mat4, float)
		INVENT_C_API_QUAT(D, DQuat, DVec3, DMat3, DMat4, double)

#ifdef __cplusplus
}
#endif // __cplusplus

#undef INVENT_C_API_VEC2
#undef INVENT_C_API_VEC3
#undef INVENT_C_API_VEC4
#undef INVENT_C_API_MAT2
#undef INVENT_C_API_MAT3
#undef INVENT_C_API_MAT4
#undef INVENT_C_API_MAT4_EXTRA
#undef INVENT_C_API_QUAT

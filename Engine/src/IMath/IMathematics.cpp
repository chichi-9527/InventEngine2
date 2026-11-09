#include "IMath/IMathematics.h"

#define GLM_ENABLE_EXPERIMENTAL         // GLM: GLM_GTX_matrix_decompose is an experimental extension and may change in the future.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <limits>
#include <type_traits>

namespace INVENT
{
    namespace detail
    {
        // 1. 自製型別 -> GLM 型別映射
        template<typename T> struct GlmTypeMap;

        template<typename T> struct GlmTypeMap<base_vec2<T>> { using type = glm::vec<2, T>; };
        template<typename T> struct GlmTypeMap<base_vec3<T>> { using type = glm::vec<3, T>; };
        template<typename T> struct GlmTypeMap<base_vec4<T>> { using type = glm::vec<4, T>; };

        template<typename T> struct GlmTypeMap<base_mat2x2<T>> { using type = glm::mat<2, 2, T>; };
        template<typename T> struct GlmTypeMap<base_mat2x3<T>> { using type = glm::mat<2, 3, T>; };
        template<typename T> struct GlmTypeMap<base_mat2x4<T>> { using type = glm::mat<2, 4, T>; };

        template<typename T> struct GlmTypeMap<base_mat3x2<T>> { using type = glm::mat<3, 2, T>; };
        template<typename T> struct GlmTypeMap<base_mat3x3<T>> { using type = glm::mat<3, 3, T>; };
        template<typename T> struct GlmTypeMap<base_mat3x4<T>> { using type = glm::mat<3, 4, T>; };

        template<typename T> struct GlmTypeMap<base_mat4x2<T>> { using type = glm::mat<4, 2, T>; };
        template<typename T> struct GlmTypeMap<base_mat4x3<T>> { using type = glm::mat<4, 3, T>; };
        template<typename T> struct GlmTypeMap<base_mat4x4<T>> { using type = glm::mat<4, 4, T>; };

        template<typename T> struct GlmTypeMap<base_quat<T>> { using type = glm::qua<T>; };

        template<typename T>
        using GlmType_t = typename GlmTypeMap<std::remove_cvref_t<T>>::type;

        // 2. 自製結構 -> GLM const 參考
        template<typename CustomT>
        inline const GlmType_t<CustomT>& ToGlm(const CustomT& v) noexcept
        {
            using GlmT = GlmType_t<CustomT>;
            static_assert(sizeof(CustomT) == sizeof(GlmT), "Size mismatch with GLM!");
            static_assert(alignof(CustomT) == alignof(GlmT), "Alignment mismatch with GLM!");
            static_assert(std::is_standard_layout_v<CustomT>, "Must be standard layout!");
            return *reinterpret_cast<const GlmT*>(&v);
        }

        // 3. 自製結構 -> GLM 可寫參考
        template<typename CustomT>
        inline GlmType_t<CustomT>& ToGlm(CustomT& v) noexcept
        {
            using GlmT = GlmType_t<CustomT>;
            static_assert(sizeof(CustomT) == sizeof(GlmT), "Size mismatch with GLM!");
            static_assert(alignof(CustomT) == alignof(GlmT), "Alignment mismatch with GLM!");
            static_assert(std::is_standard_layout_v<CustomT>, "Must be standard layout!");
            return *reinterpret_cast<GlmT*>(&v);
        }

        // 4. GLM 結構 -> 自製結構
        template<typename CustomT>
        inline CustomT FromGlm(const GlmType_t<CustomT>& v) noexcept
        {
            using GlmT = GlmType_t<CustomT>;
            static_assert(sizeof(CustomT) == sizeof(GlmT), "Size mismatch with GLM!");
            static_assert(alignof(CustomT) == alignof(GlmT), "Alignment mismatch with GLM!");
            return *reinterpret_cast<const CustomT*>(&v);
        }
    }

    namespace Math
    {
        using detail::FromGlm;
        using detail::ToGlm;

        // =========================================================================
        // 1. 運算子重載
        // =========================================================================

        Mat2 operator*(const Mat2& a, const Mat2& b)
        {
            return FromGlm<Mat2>(ToGlm(a) * ToGlm(b));
        }

        Mat3 operator*(const Mat3& a, const Mat3& b)
        {
            return FromGlm<Mat3>(ToGlm(a) * ToGlm(b));
        }

        Mat4 operator*(const Mat4& a, const Mat4& b)
        {
            return FromGlm<Mat4>(ToGlm(a) * ToGlm(b));
        }

        DMat2 operator*(const DMat2& a, const DMat2& b)
        {
            return FromGlm<DMat2>(ToGlm(a) * ToGlm(b));
        }

        DMat3 operator*(const DMat3& a, const DMat3& b)
        {
            return FromGlm<DMat3>(ToGlm(a) * ToGlm(b));
        }

        DMat4 operator*(const DMat4& a, const DMat4& b)
        {
            return FromGlm<DMat4>(ToGlm(a) * ToGlm(b));
        }

        Vec2 operator*(const Mat2& m, const Vec2& v)
        {
            return FromGlm<Vec2>(ToGlm(m) * ToGlm(v));
        }

        Vec3 operator*(const Mat3& m, const Vec3& v)
        {
            return FromGlm<Vec3>(ToGlm(m) * ToGlm(v));
        }

        Vec4 operator*(const Mat4& m, const Vec4& v)
        {
            return FromGlm<Vec4>(ToGlm(m) * ToGlm(v));
        }

        DVec2 operator*(const DMat2& m, const DVec2& v)
        {
            return FromGlm<DVec2>(ToGlm(m) * ToGlm(v));
        }

        DVec3 operator*(const DMat3& m, const DVec3& v)
        {
            return FromGlm<DVec3>(ToGlm(m) * ToGlm(v));
        }

        DVec4 operator*(const DMat4& m, const DVec4& v)
        {
            return FromGlm<DVec4>(ToGlm(m) * ToGlm(v));
        }

        Quat operator*(const Quat& a, const Quat& b)
        {
            return FromGlm<Quat>(ToGlm(a) * ToGlm(b));
        }

        Vec3 operator*(const Quat& q, const Vec3& v)
        {
            return FromGlm<Vec3>(ToGlm(q) * ToGlm(v));
        }

        DQuat operator*(const DQuat& a, const DQuat& b)
        {
            return FromGlm<DQuat>(ToGlm(a) * ToGlm(b));
        }

        DVec3 operator*(const DQuat& q, const DVec3& v)
        {
            return FromGlm<DVec3>(ToGlm(q) * ToGlm(v));
        }

        // =========================================================================
        // 2. 向量幾何運算
        // =========================================================================

        float Length(const Vec2& v) { return glm::length(ToGlm(v)); }
        float Length(const Vec3& v) { return glm::length(ToGlm(v)); }
        float Length(const Vec4& v) { return glm::length(ToGlm(v)); }
        double Length(const DVec2& v) { return glm::length(ToGlm(v)); }
        double Length(const DVec3& v) { return glm::length(ToGlm(v)); }
        double Length(const DVec4& v) { return glm::length(ToGlm(v)); }

        float LengthSq(const Vec2& v) { return glm::dot(ToGlm(v), ToGlm(v)); }
        float LengthSq(const Vec3& v) { return glm::dot(ToGlm(v), ToGlm(v)); }
        float LengthSq(const Vec4& v) { return glm::dot(ToGlm(v), ToGlm(v)); }
        double LengthSq(const DVec2& v) { return glm::dot(ToGlm(v), ToGlm(v)); }
        double LengthSq(const DVec3& v) { return glm::dot(ToGlm(v), ToGlm(v)); }
        double LengthSq(const DVec4& v) { return glm::dot(ToGlm(v), ToGlm(v)); }

        float Distance(const Vec2& a, const Vec2& b) { return glm::distance(ToGlm(a), ToGlm(b)); }
        float Distance(const Vec3& a, const Vec3& b) { return glm::distance(ToGlm(a), ToGlm(b)); }
        float Distance(const Vec4& a, const Vec4& b) { return glm::distance(ToGlm(a), ToGlm(b)); }
        double Distance(const DVec2& a, const DVec2& b) { return glm::distance(ToGlm(a), ToGlm(b)); }
        double Distance(const DVec3& a, const DVec3& b) { return glm::distance(ToGlm(a), ToGlm(b)); }
        double Distance(const DVec4& a, const DVec4& b) { return glm::distance(ToGlm(a), ToGlm(b)); }

        Vec2 Normalize(const Vec2& v) { return FromGlm<Vec2>(glm::normalize(ToGlm(v))); }
        Vec3 Normalize(const Vec3& v) { return FromGlm<Vec3>(glm::normalize(ToGlm(v))); }
        Vec4 Normalize(const Vec4& v) { return FromGlm<Vec4>(glm::normalize(ToGlm(v))); }
        DVec2 Normalize(const DVec2& v) { return FromGlm<DVec2>(glm::normalize(ToGlm(v))); }
        DVec3 Normalize(const DVec3& v) { return FromGlm<DVec3>(glm::normalize(ToGlm(v))); }
        DVec4 Normalize(const DVec4& v) { return FromGlm<DVec4>(glm::normalize(ToGlm(v))); }

        float Dot(const Vec2& a, const Vec2& b) { return glm::dot(ToGlm(a), ToGlm(b)); }
        float Dot(const Vec3& a, const Vec3& b) { return glm::dot(ToGlm(a), ToGlm(b)); }
        float Dot(const Vec4& a, const Vec4& b) { return glm::dot(ToGlm(a), ToGlm(b)); }
        double Dot(const DVec2& a, const DVec2& b) { return glm::dot(ToGlm(a), ToGlm(b)); }
        double Dot(const DVec3& a, const DVec3& b) { return glm::dot(ToGlm(a), ToGlm(b)); }
        double Dot(const DVec4& a, const DVec4& b) { return glm::dot(ToGlm(a), ToGlm(b)); }

        Vec3 Cross(const Vec3& a, const Vec3& b)
        {
            return FromGlm<Vec3>(glm::cross(ToGlm(a), ToGlm(b)));
        }

        DVec3 Cross(const DVec3& a, const DVec3& b)
        {
            return FromGlm<DVec3>(glm::cross(ToGlm(a), ToGlm(b)));
        }

        Vec2 Reflect(const Vec2& incident, const Vec2& normal)
        {
            return FromGlm<Vec2>(glm::reflect(ToGlm(incident), ToGlm(normal)));
        }

        Vec3 Reflect(const Vec3& incident, const Vec3& normal)
        {
            return FromGlm<Vec3>(glm::reflect(ToGlm(incident), ToGlm(normal)));
        }

        DVec2 Reflect(const DVec2& incident, const DVec2& normal)
        {
            return FromGlm<DVec2>(glm::reflect(ToGlm(incident), ToGlm(normal)));
        }

        DVec3 Reflect(const DVec3& incident, const DVec3& normal)
        {
            return FromGlm<DVec3>(glm::reflect(ToGlm(incident), ToGlm(normal)));
        }

        // =========================================================================
        // 3. 插值與數值限制
        // =========================================================================

        float Lerp(float a, float b, float t) { return glm::mix(a, b, t); }
        double Lerp(double a, double b, double t) { return glm::mix(a, b, t); }

        Vec2 Lerp(const Vec2& a, const Vec2& b, float t) { return FromGlm<Vec2>(glm::mix(ToGlm(a), ToGlm(b), t)); }
        Vec3 Lerp(const Vec3& a, const Vec3& b, float t) { return FromGlm<Vec3>(glm::mix(ToGlm(a), ToGlm(b), t)); }
        Vec4 Lerp(const Vec4& a, const Vec4& b, float t) { return FromGlm<Vec4>(glm::mix(ToGlm(a), ToGlm(b), t)); }

        DVec2 Lerp(const DVec2& a, const DVec2& b, double t) { return FromGlm<DVec2>(glm::mix(ToGlm(a), ToGlm(b), t)); }
        DVec3 Lerp(const DVec3& a, const DVec3& b, double t) { return FromGlm<DVec3>(glm::mix(ToGlm(a), ToGlm(b), t)); }
        DVec4 Lerp(const DVec4& a, const DVec4& b, double t) { return FromGlm<DVec4>(glm::mix(ToGlm(a), ToGlm(b), t)); }

        float Clamp(float val, float minVal, float maxVal) { return glm::clamp(val, minVal, maxVal); }
        double Clamp(double val, double minVal, double maxVal) { return glm::clamp(val, minVal, maxVal); }

        Vec2 Clamp(const Vec2& val, const Vec2& minVal, const Vec2& maxVal) { return FromGlm<Vec2>(glm::clamp(ToGlm(val), ToGlm(minVal), ToGlm(maxVal))); }
        Vec3 Clamp(const Vec3& val, const Vec3& minVal, const Vec3& maxVal) { return FromGlm<Vec3>(glm::clamp(ToGlm(val), ToGlm(minVal), ToGlm(maxVal))); }
        Vec4 Clamp(const Vec4& val, const Vec4& minVal, const Vec4& maxVal) { return FromGlm<Vec4>(glm::clamp(ToGlm(val), ToGlm(minVal), ToGlm(maxVal))); }

        DVec2 Clamp(const DVec2& val, const DVec2& minVal, const DVec2& maxVal) { return FromGlm<DVec2>(glm::clamp(ToGlm(val), ToGlm(minVal), ToGlm(maxVal))); }
        DVec3 Clamp(const DVec3& val, const DVec3& minVal, const DVec3& maxVal) { return FromGlm<DVec3>(glm::clamp(ToGlm(val), ToGlm(minVal), ToGlm(maxVal))); }
        DVec4 Clamp(const DVec4& val, const DVec4& minVal, const DVec4& maxVal) { return FromGlm<DVec4>(glm::clamp(ToGlm(val), ToGlm(minVal), ToGlm(maxVal))); }

        float SmoothStep(float edge0, float edge1, float x) { return glm::smoothstep(edge0, edge1, x); }
        double SmoothStep(double edge0, double edge1, double x) { return glm::smoothstep(edge0, edge1, x); }

        float Radians(float degrees) { return glm::radians(degrees); }
        double Radians(double degrees) { return glm::radians(degrees); }
        float Degrees(float radians) { return glm::degrees(radians); }
        double Degrees(double radians) { return glm::degrees(radians); }

        // =========================================================================
        // 4. 矩陣操作
        // =========================================================================

        Mat2 Inverse(const Mat2& m) { return FromGlm<Mat2>(glm::inverse(ToGlm(m))); }
        Mat3 Inverse(const Mat3& m) { return FromGlm<Mat3>(glm::inverse(ToGlm(m))); }
        Mat4 Inverse(const Mat4& m) { return FromGlm<Mat4>(glm::inverse(ToGlm(m))); }

        DMat2 Inverse(const DMat2& m) { return FromGlm<DMat2>(glm::inverse(ToGlm(m))); }
        DMat3 Inverse(const DMat3& m) { return FromGlm<DMat3>(glm::inverse(ToGlm(m))); }
        DMat4 Inverse(const DMat4& m) { return FromGlm<DMat4>(glm::inverse(ToGlm(m))); }

        Mat2 Transpose(const Mat2& m) { return FromGlm<Mat2>(glm::transpose(ToGlm(m))); }
        Mat3 Transpose(const Mat3& m) { return FromGlm<Mat3>(glm::transpose(ToGlm(m))); }
        Mat4 Transpose(const Mat4& m) { return FromGlm<Mat4>(glm::transpose(ToGlm(m))); }

        DMat2 Transpose(const DMat2& m) { return FromGlm<DMat2>(glm::transpose(ToGlm(m))); }
        DMat3 Transpose(const DMat3& m) { return FromGlm<DMat3>(glm::transpose(ToGlm(m))); }
        DMat4 Transpose(const DMat4& m) { return FromGlm<DMat4>(glm::transpose(ToGlm(m))); }

        float Determinant(const Mat2& m) { return glm::determinant(ToGlm(m)); }
        float Determinant(const Mat3& m) { return glm::determinant(ToGlm(m)); }
        float Determinant(const Mat4& m) { return glm::determinant(ToGlm(m)); }

        double Determinant(const DMat2& m) { return glm::determinant(ToGlm(m)); }
        double Determinant(const DMat3& m) { return glm::determinant(ToGlm(m)); }
        double Determinant(const DMat4& m) { return glm::determinant(ToGlm(m)); }

        Mat4 Translate(const Mat4& m, const Vec3& v)
        {
            return FromGlm<Mat4>(glm::translate(ToGlm(m), ToGlm(v)));
        }

        Mat4 Rotate(const Mat4& m, float angleRadians, const Vec3& axis)
        {
            return FromGlm<Mat4>(glm::rotate(ToGlm(m), angleRadians, ToGlm(axis)));
        }

        Mat4 Scale(const Mat4& m, const Vec3& v)
        {
            return FromGlm<Mat4>(glm::scale(ToGlm(m), ToGlm(v)));
        }

        DMat4 Translate(const DMat4& m, const DVec3& v)
        {
            return FromGlm<DMat4>(glm::translate(ToGlm(m), ToGlm(v)));
        }

        DMat4 Rotate(const DMat4& m, double angleRadians, const DVec3& axis)
        {
            return FromGlm<DMat4>(glm::rotate(ToGlm(m), angleRadians, ToGlm(axis)));
        }

        DMat4 Scale(const DMat4& m, const DVec3& v)
        {
            return FromGlm<DMat4>(glm::scale(ToGlm(m), ToGlm(v)));
        }

        Mat4 ComposeTransform(const Vec3& translation, const Quat& rotation, const Vec3& scale)
        {
            detail::GlmType_t<Mat4> t = glm::translate(detail::GlmType_t<Mat4>(1.0f), ToGlm(translation));
            detail::GlmType_t<Mat4> r = glm::mat4_cast(ToGlm(rotation));
            detail::GlmType_t<Mat4> s = glm::scale(detail::GlmType_t<Mat4>(1.0f), ToGlm(scale));
            return FromGlm<Mat4>(t * r * s);
        }

        bool DecomposeTransform(const Mat4& transform, Vec3& outTranslation, Quat& outRotation, Vec3& outScale)
        {
            detail::GlmType_t<Vec3> scale, translation, skew;
            detail::GlmType_t<Vec4> perspective;
            detail::GlmType_t<Quat> rotation;

            if (!glm::decompose(ToGlm(transform), scale, rotation, translation, skew, perspective))
                return false;

            outTranslation = FromGlm<Vec3>(translation);
            outRotation = FromGlm<Quat>(rotation);
            outScale = FromGlm<Vec3>(scale);
            return true;
        }

        DMat4 ComposeTransform(const DVec3& translation, const DQuat& rotation, const DVec3& scale)
        {
            detail::GlmType_t<DMat4> t = glm::translate(detail::GlmType_t<DMat4>(1.0), ToGlm(translation));
            detail::GlmType_t<DMat4> r = glm::mat4_cast(ToGlm(rotation));
            detail::GlmType_t<DMat4> s = glm::scale(detail::GlmType_t<DMat4>(1.0), ToGlm(scale));
            return FromGlm<DMat4>(t * r * s);
        }

        bool DecomposeTransform(const DMat4& transform, DVec3& outTranslation, DQuat& outRotation, DVec3& outScale)
        {
            detail::GlmType_t<DVec3> scale, translation, skew;
            detail::GlmType_t<DVec4> perspective;
            detail::GlmType_t<DQuat> rotation;

            if (!glm::decompose(ToGlm(transform), scale, rotation, translation, skew, perspective))
                return false;

            outTranslation = FromGlm<DVec3>(translation);
            outRotation = FromGlm<DQuat>(rotation);
            outScale = FromGlm<DVec3>(scale);
            return true;
        }

        // =========================================================================
        // 5. 四元數操作
        // =========================================================================

        Quat Normalize(const Quat& q)
        {
            return FromGlm<Quat>(glm::normalize(ToGlm(q)));
        }

        DQuat Normalize(const DQuat& q)
        {
            return FromGlm<DQuat>(glm::normalize(ToGlm(q)));
        }

        Quat Inverse(const Quat& q)
        {
            return FromGlm<Quat>(glm::inverse(ToGlm(q)));
        }

        DQuat Inverse(const DQuat& q)
        {
            return FromGlm<DQuat>(glm::inverse(ToGlm(q)));
        }

        Quat Conjugate(const Quat& q)
        {
            return FromGlm<Quat>(glm::conjugate(ToGlm(q)));
        }

        DQuat Conjugate(const DQuat& q)
        {
            return FromGlm<DQuat>(glm::conjugate(ToGlm(q)));
        }

        float Dot(const Quat& a, const Quat& b)
        {
            return glm::dot(ToGlm(a), ToGlm(b));
        }

        double Dot(const DQuat& a, const DQuat& b)
        {
            return glm::dot(ToGlm(a), ToGlm(b));
        }

        Quat Slerp(const Quat& a, const Quat& b, float t)
        {
            return FromGlm<Quat>(glm::slerp(ToGlm(a), ToGlm(b), t));
        }

        DQuat Slerp(const DQuat& a, const DQuat& b, double t)
        {
            return FromGlm<DQuat>(glm::slerp(ToGlm(a), ToGlm(b), t));
        }

        Quat QuatFromAxisAngle(const Vec3& axis, float angleRadians)
        {
            const auto& a = ToGlm(axis);
            const float len = glm::length(a);
            if (len <= std::numeric_limits<float>::epsilon())
                return Quat(1.0f, 0.0f, 0.0f, 0.0f);

            return FromGlm<Quat>(glm::angleAxis(angleRadians, a / len));
        }

        DQuat QuatFromAxisAngle(const DVec3& axis, double angleRadians)
        {
            const auto& a = ToGlm(axis);
            const double len = glm::length(a);
            if (len <= std::numeric_limits<double>::epsilon())
                return DQuat(1.0, 0.0, 0.0, 0.0);

            return FromGlm<DQuat>(glm::angleAxis(angleRadians, a / len));
        }

        Quat QuatFromEuler(const Vec3& eulerRadians)
        {
            detail::GlmType_t<Quat> q(ToGlm(eulerRadians));
            return FromGlm<Quat>(q);
        }

        DQuat QuatFromEuler(const DVec3& eulerRadians)
        {
            detail::GlmType_t<DQuat> q(ToGlm(eulerRadians));
            return FromGlm<DQuat>(q);
        }

        Vec3 QuatToEuler(const Quat& q)
        {
            return FromGlm<Vec3>(glm::eulerAngles(ToGlm(q)));
        }

        DVec3 QuatToEuler(const DQuat& q)
        {
            return FromGlm<DVec3>(glm::eulerAngles(ToGlm(q)));
        }

        Quat QuatLookRotation(const Vec3& forward, const Vec3& up)
        {
            return FromGlm<Quat>(glm::quatLookAt(ToGlm(forward), ToGlm(up)));
        }

        DQuat QuatLookRotation(const DVec3& forward, const DVec3& up)
        {
            return FromGlm<DQuat>(glm::quatLookAt(ToGlm(forward), ToGlm(up)));
        }

        Mat3 QuatToMat3(const Quat& q)
        {
            return FromGlm<Mat3>(glm::mat3_cast(ToGlm(q)));
        }

        Mat4 QuatToMat4(const Quat& q)
        {
            return FromGlm<Mat4>(glm::mat4_cast(ToGlm(q)));
        }

        Quat MatToQuat(const Mat3& m)
        {
            return FromGlm<Quat>(glm::quat_cast(ToGlm(m)));
        }

        Quat MatToQuat(const Mat4& m)
        {
            return FromGlm<Quat>(glm::quat_cast(ToGlm(m)));
        }

        DMat3 QuatToMat3(const DQuat& q)
        {
            return FromGlm<DMat3>(glm::mat3_cast(ToGlm(q)));
        }

        DMat4 QuatToMat4(const DQuat& q)
        {
            return FromGlm<DMat4>(glm::mat4_cast(ToGlm(q)));
        }

        DQuat MatToQuat(const DMat3& m)
        {
            return FromGlm<DQuat>(glm::quat_cast(ToGlm(m)));
        }

        DQuat MatToQuat(const DMat4& m)
        {
            return FromGlm<DQuat>(glm::quat_cast(ToGlm(m)));
        }
    }
}

// =========================================================================
// C ABI 實作
// =========================================================================

#define INVENT_C_IMPL_VEC2(SUFFIX, VT, ST) \
    void InventMathVec2Add##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) + (*b); } \
    void InventMathVec2Sub##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) - (*b); } \
    void InventMathVec2Mul##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) * (*b); } \
    void InventMathVec2Div##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) / (*b); } \
    void InventMathVec2MulScalar##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v, IEngine_IN ST s) { if (out && v) *out = (*v) * s; } \
    void InventMathVec2Length##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v) { if (out && v) *out = INVENT::Math::Length(*v); } \
    void InventMathVec2LengthSq##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v) { if (out && v) *out = INVENT::Math::LengthSq(*v); } \
    void InventMathVec2Distance##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = INVENT::Math::Distance(*a, *b); } \
    void InventMathVec2Normalize##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v) { if (out && v) *out = INVENT::Math::Normalize(*v); } \
    void InventMathVec2Dot##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = INVENT::Math::Dot(*a, *b); } \
    void InventMathVec2Reflect##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* incident, IEngine_IN INVENT::VT* normal) { if (out && incident && normal) *out = INVENT::Math::Reflect(*incident, *normal); } \
    void InventMathVec2Lerp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b, IEngine_IN ST t) { if (out && a && b) *out = INVENT::Math::Lerp(*a, *b, t); } \
    void InventMathVec2Clamp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* val, IEngine_IN INVENT::VT* minVal, IEngine_IN INVENT::VT* maxVal) { if (out && val && minVal && maxVal) *out = INVENT::Math::Clamp(*val, *minVal, *maxVal); }

#define INVENT_C_IMPL_VEC3(SUFFIX, VT, ST) \
    void InventMathVec3Add##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) + (*b); } \
    void InventMathVec3Sub##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) - (*b); } \
    void InventMathVec3Mul##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) * (*b); } \
    void InventMathVec3Div##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) / (*b); } \
    void InventMathVec3MulScalar##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v, IEngine_IN ST s) { if (out && v) *out = (*v) * s; } \
    void InventMathVec3Length##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v) { if (out && v) *out = INVENT::Math::Length(*v); } \
    void InventMathVec3LengthSq##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v) { if (out && v) *out = INVENT::Math::LengthSq(*v); } \
    void InventMathVec3Distance##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = INVENT::Math::Distance(*a, *b); } \
    void InventMathVec3Normalize##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v) { if (out && v) *out = INVENT::Math::Normalize(*v); } \
    void InventMathVec3Dot##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = INVENT::Math::Dot(*a, *b); } \
    void InventMathVec3Cross##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = INVENT::Math::Cross(*a, *b); } \
    void InventMathVec3Reflect##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* incident, IEngine_IN INVENT::VT* normal) { if (out && incident && normal) *out = INVENT::Math::Reflect(*incident, *normal); } \
    void InventMathVec3Lerp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b, IEngine_IN ST t) { if (out && a && b) *out = INVENT::Math::Lerp(*a, *b, t); } \
    void InventMathVec3Clamp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* val, IEngine_IN INVENT::VT* minVal, IEngine_IN INVENT::VT* maxVal) { if (out && val && minVal && maxVal) *out = INVENT::Math::Clamp(*val, *minVal, *maxVal); }

#define INVENT_C_IMPL_VEC4(SUFFIX, VT, ST) \
    void InventMathVec4Add##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) + (*b); } \
    void InventMathVec4Sub##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) - (*b); } \
    void InventMathVec4Mul##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) * (*b); } \
    void InventMathVec4Div##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = (*a) / (*b); } \
    void InventMathVec4MulScalar##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v, IEngine_IN ST s) { if (out && v) *out = (*v) * s; } \
    void InventMathVec4Length##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v) { if (out && v) *out = INVENT::Math::Length(*v); } \
    void InventMathVec4LengthSq##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* v) { if (out && v) *out = INVENT::Math::LengthSq(*v); } \
    void InventMathVec4Distance##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = INVENT::Math::Distance(*a, *b); } \
    void InventMathVec4Normalize##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* v) { if (out && v) *out = INVENT::Math::Normalize(*v); } \
    void InventMathVec4Dot##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b) { if (out && a && b) *out = INVENT::Math::Dot(*a, *b); } \
    void InventMathVec4Lerp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* a, IEngine_IN INVENT::VT* b, IEngine_IN ST t) { if (out && a && b) *out = INVENT::Math::Lerp(*a, *b, t); } \
    void InventMathVec4Clamp##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::VT* val, IEngine_IN INVENT::VT* minVal, IEngine_IN INVENT::VT* maxVal) { if (out && val && minVal && maxVal) *out = INVENT::Math::Clamp(*val, *minVal, *maxVal); }

#define INVENT_C_IMPL_MAT2(SUFFIX, MT, VT, ST) \
    void InventMathTwoMat2Multiply##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* a, IEngine_IN INVENT::MT* b) { if (out && a && b) *out = INVENT::Math::operator*(*a, *b); } \
    void InventMathMat2MultiplyVec2##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::MT* m, IEngine_IN INVENT::VT* v) { if (out && m && v) *out = INVENT::Math::operator*(*m, *v); } \
    void InventMathMat2Inverse##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m) { if (out && m) *out = INVENT::Math::Inverse(*m); } \
    void InventMathMat2Transpose##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m) { if (out && m) *out = INVENT::Math::Transpose(*m); } \
    void InventMathMat2Determinant##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::MT* m) { if (out && m) *out = INVENT::Math::Determinant(*m); }

#define INVENT_C_IMPL_MAT3(SUFFIX, MT, VT, ST) \
    void InventMathTwoMat3Multiply##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* a, IEngine_IN INVENT::MT* b) { if (out && a && b) *out = INVENT::Math::operator*(*a, *b); } \
    void InventMathMat3MultiplyVec3##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::MT* m, IEngine_IN INVENT::VT* v) { if (out && m && v) *out = INVENT::Math::operator*(*m, *v); } \
    void InventMathMat3Inverse##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m) { if (out && m) *out = INVENT::Math::Inverse(*m); } \
    void InventMathMat3Transpose##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m) { if (out && m) *out = INVENT::Math::Transpose(*m); } \
    void InventMathMat3Determinant##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::MT* m) { if (out && m) *out = INVENT::Math::Determinant(*m); }

#define INVENT_C_IMPL_MAT4(SUFFIX, MT, VT, ST) \
    void InventMathTwoMat4Multiply##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* a, IEngine_IN INVENT::MT* b) { if (out && a && b) *out = INVENT::Math::operator*(*a, *b); } \
    void InventMathMat4MultiplyVec4##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::MT* m, IEngine_IN INVENT::VT* v) { if (out && m && v) *out = INVENT::Math::operator*(*m, *v); } \
    void InventMathMat4Inverse##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m) { if (out && m) *out = INVENT::Math::Inverse(*m); } \
    void InventMathMat4Transpose##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m) { if (out && m) *out = INVENT::Math::Transpose(*m); } \
    void InventMathMat4Determinant##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::MT* m) { if (out && m) *out = INVENT::Math::Determinant(*m); }

#define INVENT_C_IMPL_MAT4_EXTRA(SUFFIX, MT, VT, QT, ST) \
    void InventMathMat4Translate##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m, IEngine_IN INVENT::VT* v) { if (out && m && v) *out = INVENT::Math::Translate(*m, *v); } \
    void InventMathMat4Rotate##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m, IEngine_IN ST angleRadians, IEngine_IN INVENT::VT* axis) { if (out && m && axis) *out = INVENT::Math::Rotate(*m, angleRadians, *axis); } \
    void InventMathMat4Scale##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::MT* m, IEngine_IN INVENT::VT* v) { if (out && m && v) *out = INVENT::Math::Scale(*m, *v); } \
    void InventMathMat4ComposeTransform##SUFFIX(IEngine_OUT INVENT::MT* out, IEngine_IN INVENT::VT* translation, IEngine_IN INVENT::QT* rotation, IEngine_IN INVENT::VT* scale) { if (out && translation && rotation && scale) *out = INVENT::Math::ComposeTransform(*translation, *rotation, *scale); } \
    void InventMathMat4DecomposeTransform##SUFFIX(IEngine_OUT int* outOk, IEngine_OUT INVENT::VT* outTranslation, IEngine_OUT INVENT::QT* outRotation, IEngine_OUT INVENT::VT* outScale, IEngine_IN INVENT::MT* transform) { \
        INVENT::VT t, s; \
        INVENT::QT r; \
        bool ok = false; \
        if (transform) ok = INVENT::Math::DecomposeTransform(*transform, t, r, s); \
        if (outOk) *outOk = ok ? 1 : 0; \
        if (ok) { \
            if (outTranslation) *outTranslation = t; \
            if (outRotation) *outRotation = r; \
            if (outScale) *outScale = s; \
        } \
    }

#define INVENT_C_IMPL_QUAT(SUFFIX, QT, VT, MT3, MT4, ST) \
    void InventMathQuatMultiply##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::QT* a, IEngine_IN INVENT::QT* b) { if (out && a && b) *out = INVENT::Math::operator*(*a, *b); } \
    void InventMathQuatRotateVec3##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::QT* q, IEngine_IN INVENT::VT* v) { if (out && q && v) *out = INVENT::Math::operator*(*q, *v); } \
    void InventMathQuatNormalize##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::QT* q) { if (out && q) *out = INVENT::Math::Normalize(*q); } \
    void InventMathQuatInverse##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::QT* q) { if (out && q) *out = INVENT::Math::Inverse(*q); } \
    void InventMathQuatConjugate##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::QT* q) { if (out && q) *out = INVENT::Math::Conjugate(*q); } \
    void InventMathQuatDot##SUFFIX(IEngine_OUT ST* out, IEngine_IN INVENT::QT* a, IEngine_IN INVENT::QT* b) { if (out && a && b) *out = INVENT::Math::Dot(*a, *b); } \
    void InventMathQuatSlerp##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::QT* a, IEngine_IN INVENT::QT* b, IEngine_IN ST t) { if (out && a && b) *out = INVENT::Math::Slerp(*a, *b, t); } \
    void InventMathQuatFromAxisAngle##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::VT* axis, IEngine_IN ST angleRadians) { if (out && axis) *out = INVENT::Math::QuatFromAxisAngle(*axis, angleRadians); } \
    void InventMathQuatFromEuler##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::VT* eulerRadians) { if (out && eulerRadians) *out = INVENT::Math::QuatFromEuler(*eulerRadians); } \
    void InventMathQuatToEuler##SUFFIX(IEngine_OUT INVENT::VT* out, IEngine_IN INVENT::QT* q) { if (out && q) *out = INVENT::Math::QuatToEuler(*q); } \
    void InventMathQuatLookRotation##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::VT* forward, IEngine_IN INVENT::VT* up) { if (out && forward) *out = up ? INVENT::Math::QuatLookRotation(*forward, *up) : INVENT::Math::QuatLookRotation(*forward); } \
    void InventMathQuatToMat3##SUFFIX(IEngine_OUT INVENT::MT3* out, IEngine_IN INVENT::QT* q) { if (out && q) *out = INVENT::Math::QuatToMat3(*q); } \
    void InventMathQuatToMat4##SUFFIX(IEngine_OUT INVENT::MT4* out, IEngine_IN INVENT::QT* q) { if (out && q) *out = INVENT::Math::QuatToMat4(*q); } \
    void InventMathMat3ToQuat##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::MT3* m) { if (out && m) *out = INVENT::Math::MatToQuat(*m); } \
    void InventMathMat4ToQuat##SUFFIX(IEngine_OUT INVENT::QT* out, IEngine_IN INVENT::MT4* m) { if (out && m) *out = INVENT::Math::MatToQuat(*m); }

extern "C"
{
    void InventMathLerpF(float* out, float a, float b, float t) { if (out) *out = INVENT::Math::Lerp(a, b, t); }
    void InventMathLerpD(double* out, double a, double b, double t) { if (out) *out = INVENT::Math::Lerp(a, b, t); }

    void InventMathClampF(float* out, float val, float minVal, float maxVal) { if (out) *out = INVENT::Math::Clamp(val, minVal, maxVal); }
    void InventMathClampD(double* out, double val, double minVal, double maxVal) { if (out) *out = INVENT::Math::Clamp(val, minVal, maxVal); }

    void InventMathSmoothStepF(float* out, float edge0, float edge1, float x) { if (out) *out = INVENT::Math::SmoothStep(edge0, edge1, x); }
    void InventMathSmoothStepD(double* out, double edge0, double edge1, double x) { if (out) *out = INVENT::Math::SmoothStep(edge0, edge1, x); }

    void InventMathRadiansF(float* out, float degrees) { if (out) *out = INVENT::Math::Radians(degrees); }
    void InventMathRadiansD(double* out, double degrees) { if (out) *out = INVENT::Math::Radians(degrees); }

    void InventMathDegreesF(float* out, float radians) { if (out) *out = INVENT::Math::Degrees(radians); }
    void InventMathDegreesD(double* out, double radians) { if (out) *out = INVENT::Math::Degrees(radians); }

    INVENT_C_IMPL_VEC2(F, Vec2, float)
        INVENT_C_IMPL_VEC2(D, DVec2, double)

        INVENT_C_IMPL_VEC3(F, Vec3, float)
        INVENT_C_IMPL_VEC3(D, DVec3, double)

        INVENT_C_IMPL_VEC4(F, Vec4, float)
        INVENT_C_IMPL_VEC4(D, DVec4, double)

        INVENT_C_IMPL_MAT2(F, Mat2, Vec2, float)
        INVENT_C_IMPL_MAT2(D, DMat2, DVec2, double)

        INVENT_C_IMPL_MAT3(F, Mat3, Vec3, float)
        INVENT_C_IMPL_MAT3(D, DMat3, DVec3, double)

        INVENT_C_IMPL_MAT4(F, Mat4, Vec4, float)
        INVENT_C_IMPL_MAT4(D, DMat4, DVec4, double)

        INVENT_C_IMPL_MAT4_EXTRA(F, Mat4, Vec3, Quat, float)
        INVENT_C_IMPL_MAT4_EXTRA(D, DMat4, DVec3, DQuat, double)

        INVENT_C_IMPL_QUAT(F, Quat, Vec3, Mat3, Mat4, float)
        INVENT_C_IMPL_QUAT(D, DQuat, DVec3, DMat3, DMat4, double)
}

#undef INVENT_C_IMPL_VEC2
#undef INVENT_C_IMPL_VEC3
#undef INVENT_C_IMPL_VEC4
#undef INVENT_C_IMPL_MAT2
#undef INVENT_C_IMPL_MAT3
#undef INVENT_C_IMPL_MAT4
#undef INVENT_C_IMPL_MAT4_EXTRA
#undef INVENT_C_IMPL_QUAT

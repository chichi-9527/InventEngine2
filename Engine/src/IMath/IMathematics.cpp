#include  "IMath/IMathematics.h"

#include <glm/glm.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <type_traits>

namespace INVENT
{
    namespace detail
    {
        // 1. 建立自製型別到 GLM 型別的編譯期映射
        template<typename T> struct GlmTypeMap;

        // 向量映射
        template<typename T> struct GlmTypeMap<base_vec2<T>> { using type = glm::vec<2, T>; };
        template<typename T> struct GlmTypeMap<base_vec3<T>> { using type = glm::vec<3, T>; };
        template<typename T> struct GlmTypeMap<base_vec4<T>> { using type = glm::vec<4, T>; };

        // 矩陣映射 (2x2 ~ 4x4)
        template<typename T> struct GlmTypeMap<base_mat2x2<T>> { using type = glm::mat<2, 2, T>; };
        template<typename T> struct GlmTypeMap<base_mat2x3<T>> { using type = glm::mat<2, 3, T>; };
        template<typename T> struct GlmTypeMap<base_mat2x4<T>> { using type = glm::mat<2, 4, T>; };

        template<typename T> struct GlmTypeMap<base_mat3x2<T>> { using type = glm::mat<3, 2, T>; };
        template<typename T> struct GlmTypeMap<base_mat3x3<T>> { using type = glm::mat<3, 3, T>; };
        template<typename T> struct GlmTypeMap<base_mat3x4<T>> { using type = glm::mat<3, 4, T>; };

        template<typename T> struct GlmTypeMap<base_mat4x2<T>> { using type = glm::mat<4, 2, T>; };
        template<typename T> struct GlmTypeMap<base_mat4x3<T>> { using type = glm::mat<4, 3, T>; };
        template<typename T> struct GlmTypeMap<base_mat4x4<T>> { using type = glm::mat<4, 4, T>; };

        // 四元數映射
        template<typename T> struct GlmTypeMap<base_quat<T>> { using type = glm::qua<T>; };

        template<typename T>
        using GlmType_t = typename GlmTypeMap<std::remove_cvref_t<T>>::type;

        // 2. 自製結構 -> GLM 唯讀參考 (零複製，傳參用)
        template<typename CustomT>
        inline const GlmType_t<CustomT>& ToGlm(const CustomT& v) noexcept
        {
            using GlmT = GlmType_t<CustomT>;
            static_assert(sizeof(CustomT) == sizeof(GlmT), "Size mismatch with GLM!");
            static_assert(alignof(CustomT) == alignof(GlmT), "Alignment mismatch with GLM!");
            static_assert(std::is_standard_layout_v<CustomT>, "Must be standard layout!");
            return *reinterpret_cast<const GlmT*>(&v);
        }

        // 3. 自製結構 -> GLM 可寫參考 (零複製，原地修改用)
        template<typename CustomT>
        inline GlmType_t<CustomT>& ToGlm(CustomT& v) noexcept
        {
            using GlmT = GlmType_t<CustomT>;
            static_assert(sizeof(CustomT) == sizeof(GlmT), "Size mismatch with GLM!");
            static_assert(alignof(CustomT) == alignof(GlmT), "Alignment mismatch with GLM!");
            static_assert(std::is_standard_layout_v<CustomT>, "Must be standard layout!");
            return *reinterpret_cast<GlmT*>(&v);
        }

        // 4. GLM 結構 -> 自製結構 (編譯器會透過 RVO 優化，無額外開銷)
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
        



	}
	
	
}


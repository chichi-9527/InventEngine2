#ifndef _IBITARRAY_
#define _IBITARRAY_

#include <cstdint>
#include <array>
#include <vector>
#include <utility>
#include <concepts>
#include <functional>
#include <bit>
#include <algorithm>
#include <stdexcept>
#include <limits>

namespace INVENT
{
	constexpr std::size_t MaxSizeTValue = std::numeric_limits<std::size_t>::max();

	struct IHandle
	{
		size_t BitSetIndex = MaxSizeTValue;
		// BitSet 中的索引(0~63)
		size_t BitIndex = 64;

		IHandle() = default;
		constexpr IHandle(size_t n) noexcept
			: BitSetIndex(n / 64)
			, BitIndex(n % 64)
		{}
		IHandle(const std::pair<size_t, size_t>& v)
			: BitSetIndex(v.first)
			, BitIndex(v.second)
		{}
		IHandle(const IHandle&) = default;
		IHandle(IHandle&&) noexcept = default;

		IHandle& operator=(const std::pair<size_t, size_t>& v)
		{
			BitSetIndex = v.first;
			BitIndex = v.second;
			return *this;
		}
		IHandle& operator=(const IHandle&) = default;
		IHandle& operator=(IHandle&&) noexcept = default;

		friend bool operator==(const IHandle& handle, const std::pair<size_t, size_t>& v)
		{
			return handle.BitSetIndex == v.first &&
				handle.BitIndex == v.second;
		}
		friend bool operator==(const IHandle& handle1, const IHandle& handle2)
		{
			return handle1.BitSetIndex == handle2.BitSetIndex &&
				handle1.BitIndex == handle2.BitIndex;
		}

		size_t GetRealIndex() const noexcept
		{
			return BitSetIndex * 64 + BitIndex;
		}

		bool IsVaild() const noexcept
		{
			return BitSetIndex != MaxSizeTValue &&
				BitIndex < 64;
		}
	};

	struct IBitSet64
	{
		std::uint64_t Data = 0;

		IBitSet64() = default;

		IBitSet64(std::uint64_t data) : Data(data) {}

		IBitSet64(const IBitSet64&) = default;
		IBitSet64(IBitSet64&&) noexcept = default;
		IBitSet64& operator=(const IBitSet64&) = default;
		IBitSet64& operator=(IBitSet64&&) noexcept = default;

		template<typename T>
		IBitSet64& operator=(T) = delete;

		bool operator[](size_t n) const
		{
			return (Data & (std::uint64_t{ 1 } << n)) != 0;
		}

		bool At(size_t n) const
		{
			if (n >= 64)return false;
			return (Data & (std::uint64_t{ 1 } << n));
		}

		template<bool V>
		void SetValue(size_t index)
		{
			if (index >= 64)return;
			if constexpr (V)
			{
				Data |= std::uint64_t{ 1 } << index;
			}
			else
			{
				Data &= ~(std::uint64_t{ 1 } << index);
			}
		}

		// 如果找不到（全都是 1），則回傳 64
		size_t FindFirstZeroBit() const
		{
			int trailing_ones = std::countr_one(Data);
			return static_cast<size_t>(trailing_ones);
		}
		
	};

	// 只能设置 IBitSet64 的数量，无法设置具体位数
	template<size_t Size>
	class IBitArray
	{
	public:
		IBitArray() = default;
		~IBitArray() = default;
		IBitArray(const IBitArray&) = delete;
		IBitArray(IBitArray&&) noexcept = delete;
		IBitArray& operator=(const IBitArray&) = delete;
		IBitArray& operator=(IBitArray&&) noexcept = delete;

		const IBitSet64& operator[](size_t n) const
		{
			return _array[n];
		}
		const IBitSet64& At(size_t n) const
		{
			return _array.at(n);
		}

		IBitSet64& operator[](size_t n)
		{
			return _array[n];
		}
		IBitSet64& At(size_t n)
		{
			return _array.at(n);
		}

		template<bool V>
		void SetValue(size_t arr_index, size_t bit_index)
		{
			if (arr_index >= Size) return;
			_array[arr_index].template SetValue<V>(bit_index);
		}
		template<bool V>
		void SetValue(IHandle h)
		{
			this->SetValue<V>(h.BitSetIndex, h.BitIndex);
		}

		/// <summary>
		/// 查找并返回第一个值为0的位的位置。
		/// </summary>
		/// <returns>第一个元素为包含该位的数组索引（arr_idx），第二个元素为该数组内的位索引（bit_idx）。若未找到零位，返回 { MaxSizeTValue, 64 }</returns>
		std::pair<size_t, size_t> FindFirstZero() const
		{
			for (size_t arr_idx = 0; arr_idx < Size; ++arr_idx)
			{
				size_t bit_idx = _array[arr_idx].FindFirstZeroBit();

				if (bit_idx < 64)
				{
					return { arr_idx, bit_idx };
				}
			}

			return { MaxSizeTValue, 64 };
		}

		template<typename Func>
		requires std::invocable<Func, size_t, bool>
		void ForEach(Func&& func) const
		{
			size_t index = 0;
			for (const auto& bitSet : _array)
			{
				std::uint64_t data = bitSet.Data;

				for (size_t i = 0; i < 64; ++i)
				{
					bool bitValue = (data & std::uint64_t{ 1 }) != 0;
					std::invoke(std::forward<Func>(func), index, bitValue);

					data >>= 1;
					++index;
				}
			}
		}

		void ResetBitToZero()
		{
			_array.fill(IBitSet64{ 0 });
		}

		constexpr size_t ArrSize() const
		{
			return Size;
		}

		constexpr size_t BitCount() const
		{
			return Size * 64;
		}


	private:
		std::array<IBitSet64, Size> _array = {};
	};

	class IBitVector
	{
	public:
		explicit IBitVector(size_t size = 0) : _vector(size, IBitSet64{}) {}
		~IBitVector() = default;
		IBitVector(const IBitVector&) = delete;
		IBitVector(IBitVector&&) noexcept = delete;
		IBitVector& operator=(const IBitVector&) = delete;
		IBitVector& operator=(IBitVector&&) noexcept = delete;

		const IBitSet64& operator[](size_t n) const
		{
			return _vector[n];
		}
		const IBitSet64& At(size_t n) const
		{
			return _vector.at(n);
		}

		IBitSet64& operator[](size_t n)
		{
			return _vector.at(n);
		}

		template<bool V>
		void SetValue(size_t arr_index, size_t bit_index)
		{
			if (arr_index >= _vector.size()) return;
			_vector[arr_index].SetValue<V>(bit_index);
		}
		template<bool V>
		void SetValue(IHandle h)
		{
			this->SetValue<V>(h.BitSetIndex, h.BitIndex);
		}

		/// <summary>
		/// 查找并返回第一个值为0的位的位置。
		/// 若设置了具体位数（即 bit count 不等于 ArrSize*64）会找至截止位置
		/// </summary>
		/// <returns>第一个元素为包含该位的数组索引（arr_idx），第二个元素为该数组内的位索引（bit_idx）。若未找到零位，返回 { MaxSizeTValue, 64 }</returns>
		std::pair<size_t, size_t> FindFirstZero() const
		{
			const size_t vec_size = _vector.size();

			for (size_t arr_idx = 0; arr_idx < vec_size; ++arr_idx)
			{
				size_t bit_idx = _vector[arr_idx].FindFirstZeroBit();

				if (bit_idx < 64 && (arr_idx * 64 + bit_idx) < _bit_count)
				{
					return { arr_idx, bit_idx };
				}
			}

			return { MaxSizeTValue, 64 };
		}

		// Func : void(size_t index, bool bit_value)
		// 虽支持设置具体位数，但此循环依然会循环所有具体存在的位，即使它不应使用
		// 但不应使用的位总是 0 ，即第二个参数 bit_value 总是 false
		template<typename Func>
		requires std::invocable<Func, size_t, bool>
		void ForEach(Func&& func) const
		{
			size_t index = 0;
			for (const auto& bitSet : _vector)
			{
				std::uint64_t data = bitSet.Data;

				for (size_t i = 0; i < 64; ++i)
				{
					bool bitValue = (data & std::uint64_t{ 1 }) != 0;
					std::invoke(std::forward<Func>(func), index, bitValue);

					data >>= 1;
					++index;
				}
			}
		}

		void Resize(size_t new_size)
		{
			_vector.resize(new_size, IBitSet64{});
			_bit_count = new_size * 64;
		}

		void ResizeBitCount(size_t new_size)
		{
			_vector.resize((new_size + 63) / 64, IBitSet64{});
			_bit_count = new_size;
		}

		void ResetBitToZero()
		{
			std::fill(_vector.begin(), _vector.end(), IBitSet64{ 0 });
		}

		void Clear() noexcept
		{
			_vector.clear();
		}

		size_t ArrSize() const noexcept
		{
			return _vector.size();
		}

		size_t BitCount() const noexcept
		{
			return _bit_count;
		}

	private:

		std::vector<IBitSet64> _vector;
		size_t _bit_count = 0;
	};

}

#endif // !_IBITARRAY_
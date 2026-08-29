/*license*/
#pragma once
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <memory>
#include <vector>

// 要素のアドレスが動かない可変長配列。
//
// std::vector だと push_back の再確保で既存要素が移動するため、
//     x->vec.push_back(f());   // f() が同じプールに確保すると x->vec が無効化される
// のような式が静かに壊れる。Ref は -> のたびに引き直すが、1 つの式の中では
// 引き直さないので防げない。ここは固定長ブロックを継ぎ足すだけなので要素は動かない。
//
// std::deque でも規格上は参照が保たれるが、MSVC は 8 バイトを超える型で
// 1 ブロック 1 要素になり (deque の _Block_size)、ノード 1 個ごとに確保が走る。
// 既定のブロック要素数。計測して決める (build.py --chunk で上書きできる)。
#ifndef NAST_POOL_CHUNK
#define NAST_POOL_CHUNK 8
#endif

namespace brgen::nast {

    template <class T, std::size_t ChunkSize = NAST_POOL_CHUNK>
    struct StablePool {
        using value_type = T;
        static constexpr std::size_t chunk_size = ChunkSize;

       private:
        std::vector<std::unique_ptr<T[]>> chunks_;
        std::size_t size_ = 0;

       public:
        constexpr std::size_t size() const {
            return size_;
        }

        constexpr std::size_t capacity() const {
            return chunks_.size() * ChunkSize;
        }

        constexpr bool empty() const {
            return size_ == 0;
        }

        constexpr T& operator[](std::size_t i) {
            return chunks_[i / ChunkSize][i % ChunkSize];
        }

        constexpr const T& operator[](std::size_t i) const {
            return chunks_[i / ChunkSize][i % ChunkSize];
        }

        void push_back(T v) {
            if (size_ == capacity()) {
                chunks_.push_back(std::make_unique<T[]>(ChunkSize));
            }
            (*this)[size_] = std::move(v);
            size_++;
        }

        void clear() {
            chunks_.clear();
            size_ = 0;
        }

        void resize(std::size_t n) {
            while (size_ < n) {
                push_back(T{});
            }
            // 縮小はブロックを返さない。要素のアドレスを動かさないため。
            size_ = n;
        }

        // as_json が futils の Stringer に渡すので std::ranges::range を満たす必要がある。
        // weakly_incrementable が difference_type と後置 ++ を要求する。
        template <class P, class V>
        struct iter {
            using value_type = std::remove_const_t<V>;
            using difference_type = std::ptrdiff_t;

            P* pool = nullptr;
            std::size_t i = 0;

            constexpr V& operator*() const {
                return (*pool)[i];
            }

            constexpr iter& operator++() {
                i++;
                return *this;
            }

            constexpr iter operator++(int) {
                auto copy = *this;
                i++;
                return copy;
            }

            constexpr bool operator!=(const iter& o) const {
                return i != o.i;
            }

            constexpr bool operator==(const iter& o) const {
                return i == o.i;
            }
        };

        constexpr auto begin() {
            return iter<StablePool, T>{this, 0};
        }

        constexpr auto end() {
            return iter<StablePool, T>{this, size_};
        }

        constexpr auto begin() const {
            return iter<const StablePool, const T>{this, 0};
        }

        constexpr auto end() const {
            return iter<const StablePool, const T>{this, size_};
        }
    };

}  // namespace brgen::nast

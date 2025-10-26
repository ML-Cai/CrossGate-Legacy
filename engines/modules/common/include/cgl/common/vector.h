// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <array>

namespace cgl {

// -----------------------------------------------------------------------------
template <int N, typename T>
class Vector {
 public:
    Vector() : data_() { }

    template<typename... Args>
    Vector(Args... args) : data_({static_cast<T>(args)...}) {
        static_assert(sizeof...(Args) <= N,
                      "Too many arguments for Vector constructor.");
    }

    Vector(const Vector&) = default;

    ~Vector() = default;

    Vector& operator=(const Vector&) = default;

    T& operator[](int i) noexcept {
        return data_[i];
    }

    const T& operator[](int i) const noexcept {
        return data_[i];
    }

    Vector<N, T> operator+(const Vector<N, T>& other) const {
        Vector<N, T> result;
        for (int i = 0; i < N; ++i) {
            result.data_[i] = data_[i] + other.data_[i];
        }
        return result;
    }

    Vector<N, T> operator-(const Vector<N, T>& other) const {
        Vector<N, T> result;
        for (int i = 0; i < N; ++i) {
            result.data_[i] = data_[i] - other.data_[i];
        }
        return result;
    }

    Vector<N, T> operator*(T v) const {
        Vector<N, T> result;
        for (int i = 0; i < N; ++i) {
            result.data_[i] = data_[i] * v;
        }
        return result;
    }

    Vector<N, T> operator/(T v) const {
        Vector<N, T> result;
        for (int i = 0; i < N; ++i) {
            result.data_[i] = data_[i] / v;
        }
        return result;
    }

    const T* data() const noexcept {
        return data_.data();
    }

 private:
    std::array<T, N> data_;
};

using Vec2f = cgl::Vector<2, float>;
using Vec3f = cgl::Vector<3, float>;
using Vec4f = cgl::Vector<4, float>;
using Vec2i = cgl::Vector<2, int>;
using Vec3i = cgl::Vector<3, int>;
using Vec4i = cgl::Vector<4, int>;

using Point2f = cgl::Vec2f;
using Point3f = cgl::Vec3f;
using Point4f = cgl::Vec4f;
using Point2i = cgl::Vec2i;
using Point3i = cgl::Vec3i;
using Point4i = cgl::Vec4i;

// -----------------------------------------------------------------------------
template <int N, typename T>
inline T dot(const cgl::Vector<N, T>& a, const cgl::Vector<N, T>& b) {
    T result = T(0);
    for (int i = 0; i < N; ++i) {
        result += a[i] * b[i];
    }
    return result;
}

// -----------------------------------------------------------------------------

}   // namespace cgl

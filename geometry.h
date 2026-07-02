#pragma once
#include <cmath>
#include <cassert>
#include <iostream>

template <int n>
struct vec
{
    double data[n] = {0};

    vec() = default;

    double &operator[](const int i)
    {
        assert(i >= 0 && i < n);
        return data[i];
    }
    const double &operator[](const int i) const
    {
        assert(i >= 0 && i < n);
        return data[i];
    }
};

template <>
struct vec<3>
{
    double x = 0, y = 0, z = 0;

    vec() = default;
    vec(const double x, const double y, const double z) : x(x), y(y), z(z) {}

    double &operator[](const int i)
    {
        assert(i >= 0 && i < 3);
        return i == 0 ? x : (i == 1 ? y : z);
    }
    const double &operator[](const int i) const
    {
        assert(i >= 0 && i < 3);
        return i == 0 ? x : (i == 1 ? y : z);
    }
};

template <>
struct vec<2>
{
    double x = 0, y = 0;

    vec() = default;
    vec(const double x, const double y) : x(x), y(y) {}

    double &operator[](const int i)
    {
        assert((i >= 0) && i < 2);
        return i ? y : x;
    }
    const double &operator[](const int i) const
    {
        assert((i >= 0) && i < 2);
        return i ? y : x;
    }

    double cross(const vec &other) const
    {
        return x * other.y - y * other.x;
    }
};

template <>
struct vec<4>
{
    double x = 0, y = 0, z = 0, w = 1;

    vec() = default;
    vec(const double x, const double y, const double z, const double w = 1) : x(x), y(y), z(z), w(w) {}

    double &operator[](const int i)
    {
        assert((i >= 0) && i < 4);
        return i == 0 ? x : (i == 1 ? y : (i == 2 ? z : w));
    }
    const double &operator[](const int i) const
    {
        assert((i >= 0) && i < 4);
        return i == 0 ? x : (i == 1 ? y : (i == 2 ? z : w));
    }
};

typedef vec<3> vec3;
typedef vec<2> vec2;
typedef vec<4> vec4;

template <int n>
std::ostream &operator<<(std::ostream &out, const vec<n> &v)
{
    for (int i = 0; i < n; i++)
        out << v[i] << " ";
    return out;
}

template <int n>
vec<n> &operator+=(vec<n> &lhs, const vec<n> &rhs)
{
    for (int i = 0; i < n; i++)
        lhs[i] += rhs[i];
    return lhs;
}

template <int n>
vec<n> &operator-=(vec<n> &lhs, const vec<n> &rhs)
{
    for (int i = 0; i < n; i++)
        lhs[i] -= rhs[i];
    return lhs;
}

template <int n>
vec<n> &operator*=(vec<n> &lhs, const double scalar)
{
    for (int i = 0; i < n; i++)
        lhs[i] *= scalar;
    return lhs;
}

template <int n>
vec<n> &operator/=(vec<n> &lhs, const double scalar)
{
    for (int i = 0; i < n; i++)
        lhs[i] /= scalar;
    return lhs;
}

template <int n>
vec<n> operator+(vec<n> lhs, const vec<n> &rhs)
{
    lhs += rhs;
    return lhs;
}

template <int n>
vec<n> operator-(vec<n> lhs, const vec<n> &rhs)
{
    lhs -= rhs;
    return lhs;
}

template <int n>
vec<n> operator*(vec<n> lhs, const double scalar)
{
    lhs *= scalar;
    return lhs;
}

template <int n>
vec<n> operator*(const double scalar, vec<n> rhs)
{
    rhs *= scalar;
    return rhs;
}

template <int n>
vec<n> operator/(vec<n> lhs, const double scalar)
{
    lhs /= scalar;
    return lhs;
}

template <int n>
double operator*(const vec<n> &lhs, const vec<n> &rhs)
{
    double ret = 0;
    for (int i = 0; i < n; i++)
        ret += lhs[i] * rhs[i];
    return ret;
}

template <int n>
double dot(const vec<n> &lhs, const vec<n> &rhs)
{
    double ret = 0;
    for (int i = 0; i < n; i++)
        ret += lhs[i] * rhs[i];
    return ret;
}

inline double cross(const vec2 &lhs, const vec2 &rhs)
{
    return lhs.x * rhs.y - lhs.y * rhs.x;
}

inline vec3 cross(const vec3 &lhs, const vec3 &rhs)
{
    return vec3(
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x);
}

template <int nrows, int ncols>
struct mat
{
    vec<ncols> rows[nrows]; // 矩阵的核心数据：nrows 个长度为 ncols 的横向向量

    vec<ncols> &operator[](const int i)
    {
        assert(i >= 0 && i < nrows);
        return rows[i];
    }

    const vec<ncols> &operator[](const int i) const
    {
        assert(i >= 0 && i < nrows);
        return rows[i];
    }

    vec<nrows> col(const int idx) const
    {
        assert(idx >= 0 && idx < ncols);
        vec<nrows> column;
        for (int i = 0; i < nrows; i++)
        {
            column[i] = rows[i][idx];
        }
        return column;
    }

    static mat<nrows, ncols> identity()
    {
        static_assert(nrows == ncols, "identity matrix must be square");
        mat<nrows, ncols> eye;
        for (int i = 0; i < nrows; i++)
            for (int j = 0; j < ncols; j++)
            {
                eye[i][j] = i == j ? 1. : 0.;
            }
        return eye;
    }

    static mat<nrows, ncols> zeros()
    {
        mat<nrows, ncols> zeros;
        for (int i = 0; i < nrows; i++)
            for (int j = 0; j < ncols; j++)
            {
                zeros[i][j] = 0.;
            }
        return zeros;
    }

    mat<ncols, nrows> transpose() const
    {
        mat<ncols, nrows> ret;
        for (int i = 0; i < ncols; i++)
        {
            ret[i] = col(i);
        }
        return ret;
    }

    // 技能 1：求代数余子式矩阵 (Minor)
    // 作用：划掉第 row 行和第 col 列，返回剩下的元素组成的小一号的矩阵。
    mat<nrows - 1, ncols - 1> get_minor(int row, int col) const
    {
        mat<nrows - 1, ncols - 1> ret;
        for (int i = 0; i < nrows - 1; i++)
        {
            for (int j = 0; j < ncols - 1; j++)
            {
                ret[i][j] = rows[i < row ? i : i + 1][j < col ? j : j + 1];
            }
        }
        return ret;
    }

    // 技能 2：求行列式 (Determinant)
    // 作用：递归调用 get_minor，计算出一个代表矩阵缩放体积的数字。
    // 如果行列式等于 0，说明矩阵被压扁了，没有逆矩阵！
    double det() const
    {
        static_assert(nrows == ncols, "determinant only exists for square matrices");
        // 【第一步：递归的终止条件】
        // 使用 if constexpr 让编译器在矩阵缩小到 1x1 时，直接停止往下编译！
        if constexpr (nrows == 1)
        {
            return rows[0][0];
        }
        else
        {
            double result = 0.;
            for (int j = 0; j < ncols; j++)
            {
                mat<nrows - 1, ncols - 1> M = get_minor(0, j);
                double det_M = M.det();
                double sign = (j % 2 == 0) ? 1. : -1.;
                result += det_M * sign * rows[0][j];
            }
            return result;
        }
    }

    // 技能 3：求伴随矩阵 (Adjugate)
    // 作用：计算所有位置的代数余子式，附加上正负号，然后整体转置（transpose）。
    mat<nrows, ncols> adjugate() const
    {
        static_assert(nrows == ncols, "adjugate only exists for square matrices");

        mat<nrows, ncols> ret;
        if constexpr (nrows == 1)
        {
            ret[0][0] = 1.;
            return ret;
        }
        else
        {
            for (int i = 0; i < nrows; i++)
            {
                for (int j = 0; j < ncols; j++)
                {
                    double sign = (i + j) % 2 == 0 ? 1. : -1.;
                    mat<nrows - 1, ncols - 1> M = get_minor(i, j);
                    ret[i][j] = sign * M.det();
                }
            }
        }

        return ret.transpose();
    }

    // 终极技能 4：求逆矩阵 (Inverse)
    // 核心公式：逆矩阵 = 伴随矩阵 / 行列式
    mat<nrows, ncols> invert() const
    {
        static_assert(nrows == ncols, "inverse only exists for square matrices");

        double d = det();
        assert(std::abs(d) > 1e-12);

        return adjugate() / d;
    }
};

template <int nrows, int ncols>
mat<nrows, ncols> &operator+=(mat<nrows, ncols> &lhs, mat<nrows, ncols> &rhs)
{
    for (int i = 0; i < nrows; i++)
        for (int j = 0; j < ncols; j++)
        {
            lhs[i][j] += rhs[i][j];
        }
    return lhs;
}

template <int nrows, int ncols>
mat<nrows, ncols> &operator-=(mat<nrows, ncols> &lhs, mat<nrows, ncols> &rhs)
{
    for (int i = 0; i < nrows; i++)
        for (int j = 0; j < ncols; j++)
        {
            lhs[i][j] -= rhs[i][j];
        }
    return lhs;
}

template <int nrows, int ncols>
mat<nrows, ncols> &operator*=(mat<nrows, ncols> &lhs, double scalar)
{
    for (int i = 0; i < nrows; i++)
        for (int j = 0; j < ncols; j++)
        {
            lhs[i][j] *= scalar;
        }
    return lhs;
}

template <int nrows, int ncols>
mat<nrows, ncols> &operator/=(mat<nrows, ncols> &lhs, double scalar)
{
    for (int i = 0; i < nrows; i++)
        for (int j = 0; j < ncols; j++)
        {
            lhs[i][j] /= scalar;
        }
    return lhs;
}

template <int nrows, int ncols>
mat<nrows, ncols> operator+(mat<nrows, ncols> lhs, const mat<nrows, ncols> &rhs)
{
    lhs += rhs;
    return lhs;
}

template <int nrows, int ncols>
mat<nrows, ncols> operator-(mat<nrows, ncols> lhs, const mat<nrows, ncols> &rhs)
{
    lhs -= rhs;
    return lhs;
}

template <int nrows, int ncols>
mat<nrows, ncols> operator*(mat<nrows, ncols> lhs, const double scalar)
{
    lhs *= scalar;
    return lhs;
}

template <int nrows, int ncols>
mat<nrows, ncols> operator*(const double scalar, mat<nrows, ncols> rhs)
{
    rhs *= scalar;
    return rhs;
}

template <int nrows, int ncols>
mat<nrows, ncols> operator/(mat<nrows, ncols> lhs, const double scalar)
{
    lhs /= scalar;
    return lhs;
}

// 矩阵乘法：矩阵乘以向量
template <int nrows, int ncols>
vec<nrows> operator*(const mat<nrows, ncols> &m, const vec<ncols> &v)
{
    vec<nrows> ret;
    for (int i = 0; i < nrows; i++)
    {
        ret[i] = m[i] * v;
    }
    return ret;
}

// // 矩阵乘法：矩阵乘以矩阵
template <int r1, int c1, int c2>
mat<r1, c2> operator*(const mat<r1, c1> &lhs, const mat<c1, c2> &rhs)
{
    mat<r1, c2> ret;
    for (int i = 0; i < r1; i++)
        for (int j = 0; j < c2; j++)
            ret[i][j] = lhs[i] * rhs.col(j);

    return ret;
}
#include <cmath>
#include "tgaimage.h"
#include <cstdlib>
#include <ctime>
#include "model.h"
#include <iostream>
#include <algorithm>
#include <limits>
#include <vector>

constexpr TGAColor white = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green = {0, 255, 0, 255};
constexpr TGAColor red = {0, 0, 255, 255};
constexpr TGAColor blue = {255, 128, 64, 255};
constexpr TGAColor yellow = {0, 200, 255, 255};

constexpr int width = 800;
constexpr int height = 800;

// Attempt 1: sampling issue!
// void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color)
// {
//     for (float t = 0.; t < 1.; t += 0.01)
//     {
//         int x = std::round((1 - t) * ax + t * bx);
//         int y = std::round((1 - t) * ay + t * by);
//         framebuffer.set(x, y, color);
//     }
// }

// Attempt 2: can be optimized
//  void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color)
//  {
//      bool steep = std::abs(ay - by) > std::abs(ax - bx);
//      if (steep)
//      {
//          std::swap(ax, ay);
//          std::swap(bx, by);
//      }
//      if (ax > bx) // make it left(small) to right(large)
//      {
//          std::swap(ax, bx);
//          std::swap(ay, by);
//      }
//     for (int x = ax; x <= bx; x++)
//     {
//         float t = (x - ax) / static_cast<float>(bx - ax);
//         int y = std::round(ay + (by - ay) * t);
//         if (steep)
//         {
//             framebuffer.set(y, x, color);
//         }
//         else
//         {
//             framebuffer.set(x, y, color);
//         }
//     }
// }

void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color)
{
    bool steep = std::abs(ax - bx) < std::abs(ay - by);
    if (steep)
    { // if the line is steep, we transpose the image
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax > bx)
    { // make it left−to−right
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    int y = ay;
    int ierror = 0;
    for (int x = ax; x <= bx; x++)
    {
        if (steep) // if transposed, de−transpose
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
        ierror += 2 * std::abs(by - ay);
        y += (by > ay ? 1 : -1) * (ierror > bx - ax);
        ierror -= 2 * (bx - ax) * (ierror > bx - ax);
    }
}

bool isinside(vec2 P, vec2 A, vec2 B, vec2 C) // 用向量叉乘判断任意点是否在三角形之内（包含边界）
{
    vec2 AP = vec2(P.x - A.x, P.y - A.y);
    vec2 BP = vec2(P.x - B.x, P.y - B.y);
    vec2 CP = vec2(P.x - C.x, P.y - C.y);

    vec2 AB = vec2(B.x - A.x, B.y - A.y);
    vec2 BC = vec2(C.x - B.x, C.y - B.y);
    vec2 CA = vec2(A.x - C.x, A.y - C.y);

    float cross_ap = AB.cross(AP);
    float cross_bp = BC.cross(BP);
    float cross_cp = CA.cross(CP);

    if ((cross_ap >= 0 && cross_bp >= 0 && cross_cp >= 0) || (cross_ap <= 0 && cross_bp <= 0 && cross_cp <= 0))
    {
        return true;
    }
    else
    {
        return false;
    }
}

float signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy) // 用鞋带公式shoelace formula计算有向面积
{
    return .5 * ((ax * by - ay * bx) + (bx * cy - by * cx) + (cx * ay - cy * ax));
}

void triangle(int ax, int ay, double az, TGAColor color_a,
              int bx, int by, double bz, TGAColor color_b,
              int cx, int cy, double cz, TGAColor color_c,
              TGAImage &framebuffer, std::vector<double> &zbuffer,
              TGAImage &zbuffer_img,
              bool hole = false)
{
    int width = framebuffer.width();
    int height = framebuffer.height();

    int bbmin_x = std::max(0, std::min({ax, bx, cx}));
    int bbmax_x = std::min(width - 1, std::max({ax, bx, cx}));
    int bbmin_y = std::max(0, std::min({ay, by, cy}));
    int bbmax_y = std::min(height - 1, std::max({ay, by, cy}));

    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area < 1)
        return; // backface culling + discarding triangles that cover less than a pixel

    // vec2 A = vec2(ax, ay);
    // vec2 B = vec2(bx, by);
    // vec2 C = vec2(cx, cy);

    for (int x = bbmin_x; x <= bbmax_x; x++)
    {
        for (int y = bbmin_y; y <= bbmax_y; y++)
        {
            // vec2 p = vec2(x, y);
            // if (isinside(p, A, B, C))
            // {
            //     framebuffer.set(x, y, color);
            // }

            float alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            float beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            float gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            // unsigned char z = static_cast<unsigned char>(alpha * az + beta * bz + gamma * cz);
            double z = alpha * az + beta * bz + gamma * cz;
            int idx = x + y * width;

            if ((alpha < 0 || beta < 0 || gamma < 0))
                continue;

            if (zbuffer[idx] >= z)
                continue;
            zbuffer[idx] = z;

            std::uint8_t gray = static_cast<std::uint8_t>(std::clamp(z, 0.0, 255.0));
            zbuffer_img.set(x, y, {gray});

            if (hole == true && alpha > 1. / 6. && beta > 1. / 6. && gamma > 1. / 6.)
                continue;

            TGAColor color;
            for (int channel = 0; channel < 4; channel++)
            {
                float value = alpha * color_a.bgra[channel] + beta * color_b.bgra[channel] + gamma * color_c.bgra[channel];
                color.bgra[channel] =
                    static_cast<std::uint8_t>(
                        std::clamp(value, 0.0f, 255.0f));
            }
            color.bgra[3] = 255;
            framebuffer.set(x, y, color);
        }
    }
}

vec3 project(vec3 v)
{
    return vec3((v.x + 1.0f) * width / 2.,
                (v.y + 1.0f) * height / 2., // 注意：y 用 height 而不是 width
                (v.z + 1.0f) * 255.f / 2.); // 乘以 255，将 [0, 1] 的区间放大到 [0, 255] 的颜色亮度区间
}

vec3 rotate(vec3 v)
{
    const double theta = M_PI / 6.0;

    const mat<3, 3> R = {{vec3{std::cos(theta), 0., std::sin(theta)},
                          vec3{0., 1., 0.},
                          vec3{-std::sin(theta), 0., std::cos(theta)}}};

    return R * v;
}

vec3 persp(vec3 v)
{
    constexpr double c = 3.;
    double w = 1. - v.z / c;
    return vec3{v.x / w, v.y / w, v.z};
}

int main(int argc, char **argv)

{

    TGAImage framebuffer(width, height, TGAImage::RGB);
    TGAImage zbuffer_img(width, height, TGAImage::GRAYSCALE);
    std::vector<double> zbuffer(width * height, -std::numeric_limits<double>::infinity());

    // 默认模型
    const char *filename = "obj/diablo3_pose/diablo3_pose.obj";

    if (argc == 2)
    {
        filename = argv[1];
    }
    else if (argc > 2)
    {
        std::cerr << "Usage: " << argv[0] << " [obj_filepath]" << std::endl;
        return 1;
    }

    std::cout << "Loading model: " << filename << std::endl;
    Model *model = new Model(filename);

    for (int idx = 0; idx < model->nfaces(); idx++)
    {
        std::vector<int> f = model->face(idx); // 获取当前第idx面的所有顶点全局索引

        auto A = project(persp(rotate(model->vert(f[0]))));
        auto B = project(persp(rotate(model->vert(f[1]))));
        auto C = project(persp(rotate(model->vert(f[2]))));

        TGAColor rnd_a, rnd_b, rnd_c;
        for (int k = 0; k < 3; ++k)
        {
            rnd_a[k] = std::rand() % 255;
            rnd_b[k] = std::rand() % 255;
            rnd_c[k] = std::rand() % 255;
        }
        triangle(A.x, A.y, A.z, rnd_a,
                 B.x, B.y, B.z, rnd_b,
                 C.x, C.y, C.z, rnd_c,
                 framebuffer,
                 zbuffer,
                 zbuffer_img);
    }

    zbuffer_img.write_tga_file("zbuffer.tga");
    framebuffer.write_tga_file("framebuffer.tga");
    delete model;
    return 0;
}

#include <cstdlib>
#include <cstdint>
#include "our_gl.h"
#include "model.h"
#include "shaders.h"
#include <algorithm>
#include <memory>

extern std::vector<double> zbuffer; // the depth buffer

void accumulate_ao(
    std::vector<double> &ao_visible,
    std::vector<double> &ao_samples,
    const std::vector<double> &camera_zbuffer,
    const std::vector<double> &shadowbuffer,
    const TGAImage &normalbuffer,
    const mat<4, 4> &camera_screen_to_world,
    const mat<4, 4> &world_to_shadow_screen,
    const vec3 &sample_eye,
    const int width,
    const int height)
{
    constexpr double bias = 0.005;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int camera_idx = x + y * width;
            double camera_z = camera_zbuffer[camera_idx];

            // 没有模型覆盖的背景
            if (camera_z < -100.)
                continue;

            // 相机屏幕空间 -> 世界空间齐次坐标
            vec4 world_h = camera_screen_to_world * vec4{static_cast<double>(x), static_cast<double>(y), camera_z, 1.};
            if (std::abs(world_h.w) < 1e-12)
                continue;

            vec3 world_position{world_h.x / world_h.w, world_h.y / world_h.w, world_h.z / world_h.w};

            // 解码 normalbuffer：[0, 255] -> [-1, 1]
            const TGAColor encoded_normal =
                normalbuffer.get(x, y);

            vec3 world_normal{
                encoded_normal.bgra[0] / 255.0 * 2.0 - 1.0,
                encoded_normal.bgra[1] / 255.0 * 2.0 - 1.0,
                encoded_normal.bgra[2] / 255.0 * 2.0 - 1.0};

            if (norm(world_normal) < 1e-12)
                continue;

            world_normal = normalized(world_normal);

            const vec3 omega =
                normalized(sample_eye - world_position);

            // 当前采样相机位于表面的背面，不属于该点的正面半球
            const double weight =
                std::max(0.0, dot(world_normal, omega));

            if (weight <= 0.0)
                continue;

            // 世界空间 -> 屏幕坐标
            vec4 shadow_h = world_to_shadow_screen * world_h;

            if (std::abs(shadow_h.w) < 1e-12)
                continue;

            double shadow_x = shadow_h.x / shadow_h.w;
            double shadow_y = shadow_h.y / shadow_h.w;
            double current_light_z = shadow_h.z / shadow_h.w;

            // 不在 shadow map 的投影视锥内
            if (shadow_x < 0. || shadow_x >= width || shadow_y < 0. || shadow_y >= height)
                continue;

            int sx = static_cast<int>(shadow_x);
            int sy = static_cast<int>(shadow_y);
            int shadow_idx = sx + sy * width;
            double closest_light_z = shadowbuffer[shadow_idx];

            bool inshadow = current_light_z < closest_light_z - bias; // 避免自遮挡
            ao_samples[camera_idx] += weight;

            if (!inshadow)
                ao_visible[camera_idx] += weight;
        }
    }
}

int main(int argc, char **argv)

{
    const int width = 800; // output image size
    const int height = 800;
    const vec3 eye{-1, 0, 2};   // camera position
    const vec3 center{0, 0, 0}; // camera direction
    const vec3 up{0, 1, 0};     // camera up vector

    // const vec3 light_dir_world = normalized(vec3{1, 1, 1}); // 方向光
    const vec3 light_position_world{1, 1, 1}; // spotlight

    lookat(eye, center, up);
    init_perspective(norm(eye - center));
    init_viewport(width / 16., width / 16., width * 7. / 8., height * 7. / 8.);
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB);

    std::vector<const char *> model_files;
    if (argc == 1)
    {
        model_files.push_back("obj/diablo3_pose/diablo3_pose.obj");
    }
    else
    {
        for (int m = 1; m < argc; m++)
            model_files.push_back(argv[m]);
    }

    // 避免复制模型和纹理
    std::vector<std::unique_ptr<Model>> models;
    models.reserve(model_files.size());

    for (const char *filename : model_files)
    {
        std::cout
            << "Loading model: "
            << filename
            << std::endl;

        models.push_back(
            std::make_unique<Model>(filename));
    }

    // Normal Pass
    TGAImage normalbuffer(width, height, TGAImage::RGB);

    for (const auto &model_ptr : models)
    {
        Model &model = *model_ptr;
        NormalShader shader(model);

        for (int face = 0; face < model.nfaces(); face++)
        {
            Triangle clip = {
                shader.vertex(face, 0),
                shader.vertex(face, 1),
                shader.vertex(face, 2)};

            rasterize(
                clip,
                shader,
            normalbuffer);
        }
    }

    const std::vector<double> camera_zbuffer = zbuffer;
    const mat<4, 4> camera_screen_to_world =
        (Viewport * Perspective * ModelView).invert();

    // 环境光遮蔽AO
    std::vector<double> ao_visible(width * height, 0.0);
    std::vector<double> ao_samples(width * height, 0.0);

    constexpr int sample_count = 32;
    constexpr double pi = 3.14159265358979323846;
    const double golden_angle =
        pi * (3.0 - std::sqrt(5.0));

    const double sample_radius = 3.0;

    TGAImage shadow_image(width, height, TGAImage::RGB);
    for (int sample = 0; sample < sample_count; sample++)
    {
        double y = 1.0 - 2.0 * (sample + 0.5) / sample_count;
        double radius_at_y = std::sqrt(std::max(0.0, 1.0 - y * y));

        double phi = golden_angle * sample;
        vec3 sample_dir = {radius_at_y * std::cos(phi), y, radius_at_y * std::sin(phi)};
        vec3 sample_eye = center + sample_dir * sample_radius;
        // 避免观察方向和 up 几乎平行
        vec3 sample_up = std::abs(sample_dir.y) > 0.99 ? vec3{1, 0, 0} : vec3{0, 1, 0};

        lookat(sample_eye, center, sample_up);
        init_perspective(norm(sample_eye - center));
        init_viewport(
            width / 16.,
            height / 16.,
            width * 7. / 8.,
            height * 7. / 8.);
        init_zbuffer(width, height);

        for (const auto &model_ptr : models)
        {
            Model &model = *model_ptr;
            BlankShader shader(model);

            for (int face = 0; face < model.nfaces(); face++)
            {
                Triangle clip = {
                    shader.vertex(face, 0),
                    shader.vertex(face, 1),
                    shader.vertex(face, 2)};

                rasterize(clip, shader, shadow_image);
            }
        }

        const mat<4, 4> world_to_shadow_screen = Viewport * Perspective * ModelView;
        accumulate_ao(
            ao_visible,
            ao_samples,
            camera_zbuffer,
            zbuffer,
            normalbuffer,
            camera_screen_to_world,
            world_to_shadow_screen,
            sample_eye,
            width,
            height);
    }

    std::vector<double> ao_buffer(width * height, 1.0);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int idx = x + y * width;

            if (camera_zbuffer[idx] < -100.0)
                continue;

            ao_buffer[idx] = ao_samples[idx] > 0.0
                                 ? ao_visible[idx] / ao_samples[idx]
                                 : 1.0;
        }
    }

    // AO 已经计算完成，恢复最终观察相机并执行 Phong pass。
    lookat(eye, center, up);
    init_perspective(norm(eye - center));
    init_viewport(
        width / 16.,
        height / 16.,
        width * 7. / 8.,
        height * 7. / 8.);
    init_zbuffer(width, height);

    for (const auto &model_ptr : models)
    {
        Model &model = *model_ptr;
        PhongShader shader(
            model,
            light_position_world,
            eye,
            ao_buffer,
            width);

        for (int face = 0; face < model.nfaces(); face++)
        {
            Triangle clip = {
                shader.vertex(face, 0),
                shader.vertex(face, 1),
                shader.vertex(face, 2)};

            rasterize(clip, shader, framebuffer);
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

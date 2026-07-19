#include <cstdlib>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <random>
#include <vector>

#include "our_gl.h"
#include "model.h"
#include "shaders.h"

extern std::vector<double> zbuffer; // the depth buffer

void post_processing_shadow(TGAImage &framebuffer,
                            const std::vector<double> &camera_zbuffer,
                            const std::vector<double> &shadowbuffer,
                            const mat<4, 4> &camera_screen_to_world,
                            const mat<4, 4> &world_to_shadow_screen,
                            const int width,
                            const int height)
{
    constexpr double bias = 0.005;
    constexpr double visibility = 0.3;

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

            bool in_shadow = current_light_z < closest_light_z - bias; // 避免自遮挡
            if (!in_shadow)
                continue;

            TGAColor color = framebuffer.get(x, y);
            for (int channel : {0, 1, 2})
            {
                color[channel] = static_cast<std::uint8_t>(color[channel] * visibility);
            }
            framebuffer.set(x, y, color);
        }
    }
}

std::vector<vec3> make_ssao_kernel(int sample_count, double radius)
{
    std::vector<vec3> kernel;
    kernel.reserve(sample_count);
    std::mt19937 rng(42); // 固定种子
    std::uniform_real_distribution<double> random_xy(-1., 1.);
    std::uniform_real_distribution<double> random_z(0., 1.); // 从原本的整个球体改为+z半球

    while (static_cast<int>(kernel.size()) < sample_count)
    {
        vec3 sample{random_xy(rng), random_xy(rng), random_z(rng)}; // 可能落在整个立方体里
        const double length = norm(sample);
        if (length < 1e-12 || length > 1.) // 丢弃单位球体外的点，留下单位球内部的采样
            continue;
        /*
         * 让更多采样靠近当前像素。
         *
         * t 从接近 0 逐渐变成 1；
         * t² 会让前面的采样更加集中在球心附近。
         */
        const double t = (kernel.size() + 1.0) / static_cast<double>(sample_count);

        const double scale = 0.1 + 0.9 * t * t;

        kernel.push_back(sample * radius * scale);
    }
    return kernel;
}

vec3 decode_normal(const TGAImage &normalbuffer, const int x, const int y)
{
    const TGAColor encoded =
        normalbuffer.get(x, y);

    vec3 normal{
        encoded.bgra[0] / 255. * 2. - 1.,
        encoded.bgra[1] / 255. * 2. - 1.,
        encoded.bgra[2] / 255. * 2. - 1.};

    if (norm(normal) < 1e-12)
        return {0., 0., 1.};

    return normalized(normal);
}

std::vector<double> compute_ssao(const std::vector<double> &camera_zbuffer, const TGAImage &normalbuffer, int width, int height)
{
    std::vector<double> ao_buffer(static_cast<std::size_t>(width) * height, 1.);
    constexpr int sample_count = 128;
    constexpr double radius = 0.1;
    constexpr double bias = 0.005;
    constexpr double strength = 0.7;

    const std::vector<vec3> ssao_kernel = make_ssao_kernel(sample_count, radius);
    const mat<4, 4> screen_to_eye =
        (Viewport * Perspective).invert();

    const mat<4, 4> eye_to_screen =
        Viewport * Perspective;

    // 深度距离权重
    auto smoothstep = [](const double edge0, const double edge1, const double value)
    {
        const double t = std::clamp((value - edge0) / (edge1 - edge0), 0., 1.);
        return t * t * (3. - 2. * t);
    };

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            double camera_z = camera_zbuffer[x + y * width];

            if (camera_z < -100.)
                continue;
            vec4 fragment_eye_h = screen_to_eye * vec4{static_cast<double>(x), static_cast<double>(y), camera_z, 1.};
            if (std::abs(fragment_eye_h.w) < 1e-12)
                continue;

            const vec3 fragment_eye{
                fragment_eye_h.x / fragment_eye_h.w,
                fragment_eye_h.y / fragment_eye_h.w,
                fragment_eye_h.z / fragment_eye_h.w};

            double occlusion = 0.;
            double considered_samples = 0.;

            // 逐像素解码法线for+z半球采样
            vec3 normal = decode_normal(normalbuffer, x, y);

            // 根据法线构造当前像素的局部坐标系
            const vec3 helper =
                std::abs(normal.z) < 0.999
                    ? vec3{0., 0., 1.}
                    : vec3{0., 1., 0.};

            const vec3 tangent = normalized(cross(helper, normal));

            const vec3 bitangent = normalized(cross(normal, tangent));

            for (const vec3 &offset : ssao_kernel)
            {
                // 将局部半球旋转到法线方向
                const vec3 oriented_offset =
                    tangent * offset.x +
                    bitangent * offset.y +
                    normal * offset.z;

                const vec3 sample_eye = fragment_eye + oriented_offset;
                const vec4 sample_screen_h = eye_to_screen * vec4{sample_eye.x, sample_eye.y, sample_eye.z, 1.};

                if (std::abs(sample_screen_h.w) < 1e-12)
                    continue;

                const vec4 sample_screen = sample_screen_h / sample_screen_h.w;

                if (sample_screen.x < 0 || sample_screen.y < 0 || sample_screen.x >= width || sample_screen.y >= height)
                    continue;
                considered_samples += 1.;

                const int sample_screen_x = static_cast<int>(sample_screen.x);
                const int sample_screen_y = static_cast<int>(sample_screen.y);
                const double current_z = sample_screen.z;

                const double scene_z = camera_zbuffer[sample_screen_x + sample_screen_y * width];
                if (scene_z < -100.)
                    continue;

                const vec4 scene_eye_h =
                    screen_to_eye *
                    vec4{
                        sample_screen.x,
                        sample_screen.y,
                        scene_z,
                        1.};

                if (std::abs(scene_eye_h.w) < 1e-12)
                    continue;

                const vec3 scene_eye{
                    scene_eye_h.x / scene_eye_h.w,
                    scene_eye_h.y / scene_eye_h.w,
                    scene_eye_h.z / scene_eye_h.w};

                const double depth_difference =
                    std::abs(
                        scene_eye.z -
                        fragment_eye.z);
                const double range_weight = 1. - smoothstep(radius, radius * 5., depth_difference);

                bool is_occluded = scene_z > current_z + bias;
                if (is_occluded)
                    occlusion += range_weight;
            }

            const double occluded_ratio = considered_samples > 0. ? occlusion / considered_samples : 0.;
            double ssao_visibility = std::clamp(1 - strength * occluded_ratio, 0., 1.);
            ao_buffer[x + y * width] = ssao_visibility;
        }
    }
    return ao_buffer;
}

void write_ssao_image(const std::vector<double> &ao_buffer, const int width, const int height)
{
    TGAImage ao_image(width, height, TGAImage::GRAYSCALE);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const double ao = std::clamp(ao_buffer[x + y * width], 0., 1.);
            const std::uint8_t gray = static_cast<std::uint8_t>(ao * 255.);
            TGAColor color{};
            color[0] = gray;
            ao_image.set(x, y, color);
        }
    }
    ao_image.write_tga_file("ssao.tga");
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

    // Load models
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
    std::vector<std::unique_ptr<Model>> models;
    models.reserve(model_files.size());
    for (const char *filename : model_files)
    {
        std::cout << "Loading model: " << filename << std::endl;
        models.push_back(std::make_unique<Model>(filename));
    }

    // Normal Pass
    TGAImage normalbuffer(width, height, TGAImage::RGB);
    init_zbuffer(width, height);

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

            rasterize(clip, shader, normalbuffer);
        }
    }
    normalbuffer.write_tga_file(
        "normalbuffer.tga");

    const std::vector<double> camera_zbuffer = zbuffer;
    std::vector<double> ao_buffer = compute_ssao(camera_zbuffer, normalbuffer, width, height);
    init_zbuffer(width, height);

    // Camera pass (Phong shader render)
    for (const auto &model_ptr : models)
    {
        Model &model = *model_ptr;
        PhongShader shader(
            model,
            light_position_world,
            eye,
            ao_buffer,
            width);

        for (int idx = 0; idx < model.nfaces(); idx++)
        {
            Triangle clip = {
                shader.vertex(idx, 0),
                shader.vertex(idx, 1),
                shader.vertex(idx, 2)};

            rasterize(clip, shader, framebuffer);
        }
    }

    // Shadow pass (Shadow mapping as post-pocessing)
    const mat<4, 4> camera_screen_to_world = (Viewport * Perspective * ModelView).invert();

    write_ssao_image(ao_buffer, width, height);

    lookat(light_position_world, center, up);
    init_perspective(norm(light_position_world - center));
    init_viewport(width / 16., width / 16., width * 7. / 8., height * 7. / 8.);
    init_zbuffer(width, height);
    TGAImage shadow_image(width, height, TGAImage::RGB);

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
    const std::vector<double> shadowbuffer = zbuffer;
    post_processing_shadow(framebuffer, camera_zbuffer, shadowbuffer, camera_screen_to_world, world_to_shadow_screen, width, height);

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

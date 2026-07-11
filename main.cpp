#include <cstdlib>
#include <cstdint>
#include "our_gl.h"
#include "model.h"
#include <algorithm>

extern std::vector<double> zbuffer; // the depth buffer

struct RandomShader : IShader
{
    Model &model;
    TGAColor color = {};
    vec3 tri[3]; // triangle in eye coordinates

    RandomShader(Model &m) : model(m) {}

    vec4 vertex(const int face, const int vert)
    {
        // 先取得当前面的三个全局顶点索引
        const vec3 v = model.vert(face, vert); // current vertex in object coordinates
        const vec4 gl_Position = ModelView * vec4{v.x, v.y, v.z, 1.};
        // tri[vert] = gl_Position.xyz();                            // in eye coordinates
        return Perspective * gl_Position; // in clip coordinates
    }

    std::pair<bool, TGAColor> fragment(const vec3 bar) const override
    {
        return {false, color}; // do not discard the pixel
    }
};

struct PhongShader : IShader
{
    Model &model;
    vec3 light_dir;

    vec3 varying_normal[3];
    vec3 varying_position[3];
    vec2 varying_uv[3];

    PhongShader(Model &m, const vec3 &light_dir_world) : model(m)
    {
        // 光线是方向，w 必须是 0，不受平移影响
        const vec4 light4 =
            ModelView *
            vec4{
                light_dir_world.x,
                light_dir_world.y,
                light_dir_world.z,
                0.0};

        light_dir = normalized(
            vec3{light4.x, light4.y, light4.z});
    }

    vec4 vertex(const int face, const int vert)
    {
        // 先取得当前面的三个全局顶点索引
        const vec3 position = model.vert(face, vert); // current vertex in object coordinate
        const vec3 normal = model.normal(face, vert);
        const vec2 uv = model.texcoord(face, vert);

        const vec4 eye_position =
            ModelView *
            vec4{position.x, position.y, position.z, 1.};
        varying_position[vert] = {
            eye_position.x,
            eye_position.y,
            eye_position.z};

        const vec4 eye_normal =
            ModelView *
            vec4{normal.x, normal.y, normal.z, 0.};
        varying_normal[vert] = {
            eye_normal.x,
            eye_normal.y,
            eye_normal.z};

        return Perspective * eye_position; // in clip coordinates
    }

    std::pair<bool, TGAColor> fragment(const vec3 bar) const override
    {
        // 插值观察空间位置
        const vec3 position =
            varying_position[0] * bar.x +
            varying_position[1] * bar.y +
            varying_position[2] * bar.z;

        // 插值并重新归一化法线
        const vec3 n = normalized(
            varying_normal[0] * bar.x +
            varying_normal[1] * bar.y +
            varying_normal[2] * bar.z);

        // Light direction in clip coordinates
        const vec3 light = normalized(light_dir);

        // Ambient light
        const double ambient = 0.15;

        // Diffuse Reflection
        const double diffuse = std::max(0., dot(light, n));

        // Specular Reflection
        const double e = 32.;
        const vec3 view_dir = normalized(-1. * position);
        const vec3 reflect_dir = 2. * n * dot(n, light) - light;
        const double specular = std::pow(std::max(0., dot(view_dir, reflect_dir)), e);

        const double intensity = ambient + diffuse + specular;

        const std::uint8_t value = static_cast<std::uint8_t>(std::clamp(intensity, 0., 1.) * 255.);
        TGAColor fragment_color = {value, value, value, 255};
        return {false, fragment_color}; // do not discard the pixel
    }
};

int main(int argc, char **argv)

{
    const int width = 800; // output image size
    const int height = 800;
    const vec3 eye{-1, 0, 2};   // camera position
    const vec3 center{0, 0, 0}; // camera direction
    const vec3 up{0, 1, 0};     // camera up vector
    const vec3 light_dir_world = normalized(vec3{1, 1, 1});

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

    for (const char *filename : model_files)
    {
        std::cout << "Loading model: " << filename << std::endl;
        Model model(filename);
        // RandomShader shader(model);
        PhongShader shader(model, light_dir_world);

        for (int idx = 0; idx < model.nfaces(); idx++)
        {
            // shader.color = {
            //     static_cast<std::uint8_t>(std::rand() % 255),
            //     static_cast<std::uint8_t>(std::rand() % 255),
            //     static_cast<std::uint8_t>(std::rand() % 255),
            //     255};

            Triangle clip = {
                shader.vertex(idx, 0),
                shader.vertex(idx, 1),
                shader.vertex(idx, 2)};

            rasterize(clip, shader, framebuffer);
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

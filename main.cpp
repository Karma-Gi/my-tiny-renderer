#include <cstdlib>
#include "our_gl.h"
#include "model.h"

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
    TGAColor color = {};

    PhongShader(Model &m) : model(m) {}

    vec4 vertex(const int face, const int vert)
    {
        // 先取得当前面的三个全局顶点索引
        const vec3 position = model.vert(face, vert); // current vertex in object coordinate
        const vec3 normal = model.normal(face, vert);
        const vec2 uv = model.texcoord(face, vert);

        const vec4 gl_Position = ModelView * vec4{position.x, position.y, position.z, 1.};

        return Perspective * gl_Position; // in clip coordinates
    }

    std::pair<bool, TGAColor> fragment(const vec3 bar) const override
    {
        // Ambient light

        // Diffuse Reflection

        // Specular Reflection

        return {false, color}; // do not discard the pixel
    }
};

int main(int argc, char **argv)

{
    const int width = 800; // output image size
    const int height = 800;
    const vec3 eye{-1, 0, 2};   // camera position
    const vec3 center{0, 0, 0}; // camera direction
    const vec3 up{0, 1, 0};     // camera up vector

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
        RandomShader shader(model);

        for (int idx = 0; idx < model.nfaces(); idx++)
        {
            shader.color = {
                static_cast<std::uint8_t>(std::rand() % 255),
                static_cast<std::uint8_t>(std::rand() % 255),
                static_cast<std::uint8_t>(std::rand() % 255),
                255};

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

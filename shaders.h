#pragma once

#include "model.h"
#include "our_gl.h"

#include <vector>

struct BlankShader : IShader
{
    const Model &model;

    BlankShader(const Model &m);

    vec4 vertex(const int face, const int vert);
    std::pair<bool, TGAColor> fragment(const vec3, int, int) const override;
};

struct PhongShader : IShader
{
    Model &model;
    vec3 light_position_eye;
    vec3 camera_position_eye;
    mat<3, 3> normal_matrix;

    vec3 varying_normal[3];
    vec3 varying_position[3];
    vec2 varying_uv[3];

    vec3 triangle_tangent;
    vec3 triangle_bitangent;
    bool triangle_tbn_valid;
    const std::vector<double> &ao_buffer;
    int framebuffer_width;

    PhongShader(Model &m,
                const vec3 &light_position_world,
                const vec3 &camera_position_world,
                const std::vector<double> &ao,
                const int width);

    vec4 vertex(const int face, const int vert);

    std::pair<bool, TGAColor> fragment(const vec3 bar, const int x, const int y) const override;
};

struct NormalShader : IShader
{
    const Model &model;
    mat<3, 3> normal_matrix;
    vec3 varying_normal[3];

    NormalShader(const Model &m);

    vec4 vertex(const int face, const int vert);
    std::pair<bool, TGAColor> fragment(const vec3 bar, int, int) const override;
};

struct ToonShader : IShader
{
    const Model &model;
    vec4 color;
    vec3 light_position_eye;
    mat<3,3> normal_matrix;
    vec3 varying_normal[3];

    ToonShader(const Model &m, const vec4 color, const vec3 light);
    vec4 vertex(const int face, const int vert);
    std::pair<bool, TGAColor> fragment(const vec3, int, int) const override;
};

#include "shaders.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

BlankShader::BlankShader(const Model &m) : model(m) {}

vec4 BlankShader::vertex(const int face, const int vert)
{
    const vec3 v = model.vert(face, vert);
    const vec4 eye_position = ModelView * vec4{v.x, v.y, v.z, 1.};
    return Perspective * eye_position;
}

std::pair<bool, TGAColor> BlankShader::fragment(const vec3, int, int) const
{
    return {false, {255, 255, 255, 255}};
}

PhongShader::PhongShader(Model &m,
                         const vec3 &light_position_world,
                         const vec3 &camera_position_world,
                         const std::vector<double> &ao_buffer,
                         const int framebuffer_width)
    : model(m),
      ao_buffer(ao_buffer),
      framebuffer_width(framebuffer_width)
{
    const mat<3, 3> linear_modelview =
        {{{ModelView[0][0], ModelView[0][1], ModelView[0][2]},
          {ModelView[1][0], ModelView[1][1], ModelView[1][2]},
          {ModelView[2][0], ModelView[2][1], ModelView[2][2]}}};

    normal_matrix = linear_modelview.invert().transpose();

    const vec4 camera4 =
        ModelView *
        vec4{camera_position_world.x,
             camera_position_world.y,
             camera_position_world.z,
             1.0};

    camera_position_eye = {
        camera4.x,
        camera4.y,
        camera4.z};

    const vec4 light4 =
        ModelView *
        vec4{light_position_world.x,
             light_position_world.y,
             light_position_world.z,
             1.0};

    light_position_eye = {
        light4.x,
        light4.y,
        light4.z};
}

vec4 PhongShader::vertex(const int face, const int vert)
{
    const vec3 position = model.vert(face, vert);
    const vec3 normal = model.normal(face, vert);
    const vec2 uv = model.texcoord(face, vert);

    const vec4 eye_position =
        ModelView * vec4{position.x, position.y, position.z, 1.};

    varying_position[vert] = {
        eye_position.x,
        eye_position.y,
        eye_position.z};

    const vec3 eye_normal = normal_matrix * normal;
    varying_normal[vert] = {
        eye_normal.x,
        eye_normal.y,
        eye_normal.z};

    varying_uv[vert] = {uv.x, uv.y};

    if (vert == 2)
    {
        const vec3 e0 = varying_position[1] - varying_position[0];
        const vec3 e1 = varying_position[2] - varying_position[0];
        const vec2 duv0 = varying_uv[1] - varying_uv[0];
        const vec2 duv1 = varying_uv[2] - varying_uv[0];

        const double det =
            duv0.x * duv1.y -
            duv1.x * duv0.y;

        triangle_tbn_valid = std::abs(det) > 1e-12;

        if (triangle_tbn_valid)
        {
            const mat<3, 2> E =
                {{{e0.x, e1.x},
                  {e0.y, e1.y},
                  {e0.z, e1.z}}};

            const mat<2, 2> U =
                {{{duv0.x, duv1.x},
                  {duv0.y, duv1.y}}};

            const mat<3, 2> TB = E * U.invert();
            triangle_tangent =
                normalized(vec3{TB[0][0], TB[1][0], TB[2][0]});
            triangle_bitangent =
                normalized(vec3{TB[0][1], TB[1][1], TB[2][1]});
        }
    }

    return Perspective * eye_position;
}

std::pair<bool, TGAColor> PhongShader::fragment(
    const vec3 bar,
    const int x,
    const int y) const
{
    const vec3 position =
        varying_position[0] * bar.x +
        varying_position[1] * bar.y +
        varying_position[2] * bar.z;

    const vec3 N = normalized(
        varying_normal[0] * bar.x +
        varying_normal[1] * bar.y +
        varying_normal[2] * bar.z);

    const vec2 uv =
        varying_uv[0] * bar.x +
        varying_uv[1] * bar.y +
        varying_uv[2] * bar.z;

    const vec3 T =
        normalized(triangle_tangent - N * dot(N, triangle_tangent));
    const double handedness =
        dot(cross(triangle_tangent, triangle_bitangent), N) < 0.
            ? -1.
            : 1.;
    const vec3 B = normalized(cross(N, T)) * handedness;

    const mat<3, 3> TBN =
        {{{T.x, B.x, N.x},
          {T.y, B.y, N.y},
          {T.z, B.z, N.z}}};

    const vec3 uv_mapped_n = model.normal_from_map(uv);
    vec3 shading_normal = normalized(TBN * uv_mapped_n);

    if (model.has_normalmap() && triangle_tbn_valid)
    {
        const vec3 tangent_normal = model.normal_from_map(uv);
        shading_normal = normalized(TBN * tangent_normal);
    }

    const TGAColor diffuse_color = model.diffuse_from_map(uv);
    const double specular_weight = model.specular_from_map(uv);

    const vec3 to_light = light_position_eye - position;
    const double light_distance = norm(to_light);
    const vec3 light = normalized(to_light);

    const int pixel_index = x + y * framebuffer_width;
    const double ao = std::clamp(ao_buffer[pixel_index], 0.0, 1.0);
    constexpr double ao_strength = 0.7;
    const double adjusted_ao = 1.0 - ao_strength * (1.0 - ao);
    const double ambient = 0.4 * adjusted_ao;
    // const double ambient = 0.4 * ao;
    const double diffuse = std::max(0., dot(light, shading_normal));

    constexpr double specular_exponent = 35.;
    const vec3 view_dir = normalized(camera_position_eye - position);
    const vec3 reflect_dir =
        2. * shading_normal * dot(shading_normal, light) - light;

    double specular = 0.0;
    if (diffuse > 0.0)
    {
        specular = std::pow(
            std::max(0., dot(view_dir, reflect_dir)),
            specular_exponent);
    }

    const double distance_squared = dot(to_light, to_light);
    const double attenuation =
        1.0 /
        (1.0 + 0.1 * light_distance + 0.03 * distance_squared);

    const double intensity = std::min(
        1.0,
        ambient + attenuation *
                      (diffuse + 3. * specular_weight * specular));

    TGAColor fragment_color = diffuse_color;
    for (const int channel : {0, 1, 2})
    {
        fragment_color[channel] =
            static_cast<std::uint8_t>(
                std::clamp(
                    diffuse_color.bgra[channel] * intensity,
                    0.0,
                    255.0));
    }

    fragment_color.bgra[3] = 255;
    return {false, fragment_color};
}

NormalShader::NormalShader(const Model &m) : model(m) {}

vec4 NormalShader::vertex(const int face, const int vert)
{
    const vec3 position = model.vert(face, vert);
    varying_normal[vert] = normalized(model.normal(face, vert));

    const vec4 eye_position =
        ModelView * vec4{position.x, position.y, position.z, 1.0};

    return Perspective * eye_position;
}

std::pair<bool, TGAColor> NormalShader::fragment(
    const vec3 bar,
    int,
    int) const
{
    const vec3 normal = normalized(
        varying_normal[0] * bar.x +
        varying_normal[1] * bar.y +
        varying_normal[2] * bar.z);

    TGAColor color;
    color[0] = static_cast<std::uint8_t>(
        std::clamp(normal.x * 0.5 + 0.5, 0.0, 1.0) * 255.0);
    color[1] = static_cast<std::uint8_t>(
        std::clamp(normal.y * 0.5 + 0.5, 0.0, 1.0) * 255.0);
    color[2] = static_cast<std::uint8_t>(
        std::clamp(normal.z * 0.5 + 0.5, 0.0, 1.0) * 255.0);
    color[3] = 255;

    return {false, color};
}

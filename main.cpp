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
    vec3 light_position_eye;
    vec3 camera_position_eye;
    mat<3, 3> normal_matrix;

    vec3 varying_normal[3];
    vec3 varying_position[3];
    vec2 varying_uv[3];

    vec3 triangle_tangent;
    vec3 triangle_bitangent;
    bool triangle_tbn_valid;

    PhongShader(Model &m, const vec3 &light_position_world, const vec3 &camera_position_world) : model(m)
    {
        const mat<3, 3> linear_modelview = {{{ModelView[0][0],
                                              ModelView[0][1],
                                              ModelView[0][2]},
                                             {ModelView[1][0],
                                              ModelView[1][1],
                                              ModelView[1][2]},
                                             {ModelView[2][0],
                                              ModelView[2][1],
                                              ModelView[2][2]}}};

        // 法线矩阵：ModelView 线性部分的逆转置
        normal_matrix =
            linear_modelview.invert().transpose();

        const vec4 camera4 =
            ModelView *
            vec4{
                camera_position_world.x,
                camera_position_world.y,
                camera_position_world.z,
                1.0};

        camera_position_eye = {
            camera4.x,
            camera4.y,
            camera4.z};

        // 方向光：光线是方向，w 必须是 0，不受平移影响
        // const vec4 light4 =
        //     ModelView *
        //     vec4{
        //         light_dir_world.x,
        //         light_dir_world.y,
        //         light_dir_world.z,
        //         0.0};

        // light_dir = normalized(
        //     vec3{light4.x, light4.y, light4.z});

        // 点光源
        const vec4 light4 =
            ModelView *
            vec4{
                light_position_world.x,
                light_position_world.y,
                light_position_world.z,
                1.0}; // 点的位置，w 必须是 1

        light_position_eye = {
            light4.x,
            light4.y,
            light4.z};
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

        // const vec4 eye_normal =
        //     ModelView *
        //     vec4{normal.x, normal.y, normal.z, 0.};
        const vec3 eye_normal = normal_matrix * normal;
        varying_normal[vert] = {
            eye_normal.x,
            eye_normal.y,
            eye_normal.z};

        varying_uv[vert] = {
            uv.x,
            uv.y};

        // 计算TBN矩阵中所需的tangent和bitangent
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

                const mat<3, 2> E = {{{e0.x, e1.x}, {e0.y, e1.y}, {e0.z, e1.z}}};
                const mat<2, 2> U = {{{duv0.x, duv1.x}, {duv0.y, duv1.y}}};

                mat<3, 2> TB = E * U.invert();
                triangle_tangent = normalized(vec3{TB[0][0], TB[1][0], TB[2][0]});
                triangle_bitangent = normalized(vec3{TB[0][1], TB[1][1], TB[2][1]});
            }
        }

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
        const vec3 N = normalized(
            varying_normal[0] * bar.x +
            varying_normal[1] * bar.y +
            varying_normal[2] * bar.z);

        const vec2 uv = varying_uv[0] * bar.x +
                        varying_uv[1] * bar.y +
                        varying_uv[2] * bar.z;

        vec3 T = normalized(triangle_tangent - N * dot(N, triangle_tangent));
        double handedness = dot(cross(triangle_tangent, triangle_bitangent), N) < 0. ? -1. : 1.;
        vec3 B = normalized(cross(N, T)) * handedness;

        mat<3, 3> TBN = {{{T.x, B.x, N.x}, {T.y, B.y, N.y}, {T.z, B.z, N.z}}};

        const vec3 uv_mapped_n = model.normal_from_map(uv); // tangent normal

        vec3 shading_normal = normalized(TBN * uv_mapped_n);

        if (model.has_normalmap() && triangle_tbn_valid)
        {
            const vec3 tangent_normal =
                model.normal_from_map(uv);

            shading_normal = normalized(
                TBN * tangent_normal);
        }

        // 采样颜色和镜面权重
        const TGAColor diffuse_color = model.diffuse_from_map(uv);
        const double specular_weight = model.specular_from_map(uv);

        // 方向光：Light direction in clip coordinates
        // const vec3 light = normalized(light_dir);

        // 点光源：position 和 light_position_eye 都位于观察空间
        const vec3 to_light = light_position_eye - position;
        const double light_distance = norm(to_light);
        const vec3 light = normalized(to_light);

        // Ambient light
        const double ambient = 0.4;

        // Diffuse Reflection
        const double diffuse = std::max(0., dot(light, shading_normal));

        // Specular Reflection
        const double e = 35;
        const vec3 view_dir = normalized(camera_position_eye - position);
        const vec3 reflect_dir = 2. * shading_normal * dot(shading_normal, light) - light;

        double specular = 0.0;
        if (diffuse > 0.0)
        {
            specular = std::pow(std::max(0., dot(view_dir, reflect_dir)), e);
        }

        // 点光源距离衰减
        const double distance_squared = dot(to_light, to_light);
        const double attenuation = 1.0 / (1.0 + 0.1 * light_distance + 0.03 * distance_squared);

        const double intensity = std::min(1.0, ambient + attenuation * (1. * diffuse + 3. * specular_weight * specular));

        TGAColor fragment_color = diffuse_color;
        for (int channel : {0, 1, 2})
        {
            fragment_color[channel] =
                static_cast<std::uint8_t>(
                    std::clamp(
                        diffuse_color.bgra[channel] *
                            intensity,
                        0.0,
                        255.0));
        }

        fragment_color.bgra[3] = 255;

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

    // const vec3 light_dir_world = normalized(vec3{1, 1, 1}); // 方向光
    const vec3 light_position_world{1, 1, 1}; // 点光源

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

        // Pass 2: camera pass
        PhongShader shader(model, light_position_world, eye);

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

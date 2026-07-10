#include <algorithm>
#include "our_gl.h"

mat<4, 4> ModelView, Viewport, Perspective; // "OpenGL" state matrices
std::vector<double> zbuffer;                // depth buffer

void lookat(const vec3 eye, const vec3 center, const vec3 up)
{
    vec3 n = normalized(eye - center);
    vec3 l = normalized(cross(up, n));
    vec3 m = normalized(cross(n, l));
    ModelView = mat<4, 4>{{{l.x, l.y, l.z, 0},
                           {m.x, m.y, m.z, 0},
                           {n.x, n.y, n.z, 0},
                           {0, 0, 0, 1}}} *
                mat<4, 4>{{{1, 0, 0, -center.x},
                           {0, 1, 0, -center.y},
                           {0, 0, 1, -center.z},
                           {0, 0, 0, 1}}};
}

void init_perspective(const double f)
{
    Perspective = {{{1, 0, 0, 0},
                    {0, 1, 0, 0},
                    {0, 0, 1, 0},
                    {0, 0, -1 / f, 1}}};
}

void init_viewport(const int x, const int y, const int w, const int h)
{
    Viewport = {{{w / 2., 0, 0, x + w / 2.},
                 {0, h / 2., 0, y + h / 2.},
                 {0, 0, 1, 0},
                 {0, 0, 0, 1}}};
}

void init_zbuffer(const int width, const int height)
{
    zbuffer = std::vector(width * height, -1000.);
}

void rasterize(const Triangle &clip, const IShader &shader, TGAImage &framebuffer)
{
    vec4 ndc[3] = {clip[0] / clip[0].w, clip[1] / clip[1].w, clip[2] / clip[2].w}; // normalized device coordinates (3x4 matrix)
    vec4 screen4[3] = {
        Viewport * ndc[0],
        Viewport * ndc[1],
        Viewport * ndc[2]};

    vec2 screen[3] = {
        vec2{screen4[0].x, screen4[0].y},
        vec2{screen4[1].x, screen4[1].y},
        vec2{screen4[2].x, screen4[2].y}}; // screen coordinates

    int width = framebuffer.width();
    int height = framebuffer.height();

    int bbmin_x = std::max(0., std::min({screen[0].x, screen[1].x, screen[2].x}));
    int bbmax_x = std::min(width - 1., std::max({screen[0].x, screen[1].x, screen[2].x}));
    int bbmin_y = std::max(0., std::min({screen[0].y, screen[1].y, screen[2].y}));
    int bbmax_y = std::min(height - 1., std::max({screen[0].y, screen[1].y, screen[2].y}));

    mat<3, 3> ABC = {{{screen[0].x, screen[1].x, screen[2].x},
                      {screen[0].y, screen[1].y, screen[2].y},
                      {1., 1., 1.}}};
    if (ABC.det() < 1.)
        return; // backface culling + discarding triangles that cover less than a pixel

    for (int x = bbmin_x; x <= bbmax_x; x++)
    {
        for (int y = bbmin_y; y <= bbmax_y; y++)
        {
            vec3 bc = ABC.invert() * vec3{static_cast<double>(x), static_cast<double>(y), 1.}; // barycentric coordinates of {x,y} w.r.t the triangle
            double alpha = bc[0];
            double beta = bc[1];
            double gamma = bc[2];
            if (alpha < 0 || beta < 0 || gamma < 0)
                continue; // negative barycentric coordinate => the pixel is outside the triangle

            double z = alpha * ndc[0].z + beta * ndc[1].z + gamma * ndc[2].z; // linear interpolation of the depth
            int idx = x + y * width;

            if (zbuffer[idx] >= z)
                continue; // discard fragments that are too deep w.r.t the z-buffer

            auto [discard, color] = shader.fragment(bc);
            if (discard)
                continue;                 // fragment shader can discard current fragment
            zbuffer[idx] = z;             // update the z-buffer
            framebuffer.set(x, y, color); // update the framebuffer
        }
    }
}
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cassert>
#include <filesystem>

#include "model.h"

Model::Model(const char *filename)
    : verts_(),
      texcoords_(),
      normals_(),
      faces_(),
      normalmap_(),
      diffusemap_(),
      specularmap_(),
      normalmap_loaded_(false),
      diffusemap_loaded_(false),
      specularmap_loaded_(false)
{
    std::ifstream in;
    in.open(filename, std::ifstream::in);
    if (in.fail())
    {
        std::cerr << "Fail to open " << filename << std::endl;
        return;
    }

    namespace fs = std::filesystem;

    const fs::path obj_path(filename);

    const fs::path normalmap_path =
        obj_path.parent_path() /
        (obj_path.stem().string() + "_nm_tangent.tga");

    const fs::path diffusemap_path =
        obj_path.parent_path() /
        (obj_path.stem().string() + "_diffuse.tga");

    const fs::path specularmap_path =
        obj_path.parent_path() /
        (obj_path.stem().string() + "_spec.tga");

    normalmap_loaded_ =
        normalmap_.read_tga_file(
            normalmap_path.string());

    diffusemap_loaded_ =
        diffusemap_.read_tga_file(
            diffusemap_path.string());

    specularmap_loaded_ =
        specularmap_.read_tga_file(
            specularmap_path.string());

    if (normalmap_loaded_)
    {
        std::cerr
            << "normal map: "
            << (normalmap_loaded_ ? "loaded" : "missing")
            << '\n'
            << "diffuse map: "
            << (diffusemap_loaded_ ? "loaded" : "missing")
            << '\n'
            << "specular map: "
            << (specularmap_loaded_ ? "loaded" : "missing")
            << '\n';
    }
    else
    {
        std::cerr
            << "Warning: failed to load normal map: "
            << normalmap_path.string()
            << '\n';
    }

    std::string line;
    while (std::getline(in, line)) // 逐行读取
    {
        std::istringstream iss(line);
        char trash;
        std::string prefix;

        // 接下来我们要分析提取 line 里面的数据
        // 解析顶点坐标：以 "v " 开头
        if (line.compare(0, 2, "v ") == 0)
        {
            iss >> trash; // 吃掉字符 'v'
            vec3 v;
            iss >> v.x >> v.y >> v.z; // 读取坐标
            verts_.push_back(v);      // 存入顶点仓库
        }

        else if (line.compare(0, 3, "vt ") == 0)
        {
            iss >> prefix;
            vec2 vt;
            iss >> vt.x >> vt.y;
            texcoords_.push_back(vt);
        }

        else if (line.compare(0, 3, "vn ") == 0)
        {
            iss >> prefix;
            vec3 vn;
            iss >> vn.x >> vn.y >> vn.z;
            normals_.push_back(vn);
        }

        else if (line.compare(0, 2, "f ") == 0)
        {
            FaceVertex f;
            Face facet;
            int vert_idx, uv_idx, normal_idx;
            iss >> trash; // 吃掉 ‘f’

            // 循环读取每一组 "顶点/纹理/法线" (v/vt/vn)
            while (iss >> vert_idx >> trash >> uv_idx >> trash >> normal_idx)
            {
                f.vertex_index = vert_idx - 1;
                f.texcoord_index = uv_idx - 1;
                f.normal_index = normal_idx - 1;

                facet.push_back(f);
            }
            if (facet.size() >= 3)
                faces_.push_back(facet);
        }
    }
    // 读取完毕后打印一下结果，确保我们成功了
    std::cerr
        << "# vertices: " << verts_.size()
        << " # texcoords: " << texcoords_.size()
        << " # normals: " << normals_.size()
        << " # faces: " << faces_.size()
        << '\n';
}

// 返回多边形面的总数
int Model::nfaces() const
{
    return faces_.size();
}

// 返回第i个顶点的xyz坐标
vec3 Model::vert(int index) const
{
    return verts_[index];
}

vec2 Model::texcoord(int index) const
{
    return texcoords_[index];
}

vec3 Model::normal(int index) const
{
    return normals_[index];
}

// 返回第idx个面的顶点全局索引
const Face &Model::face(int face_idx) const
{
    return faces_[face_idx];
}

vec3 Model::vert(int face_index, int nthvert) const
{
    const FaceVertex &fv = faces_[face_index][nthvert];
    return verts_[fv.vertex_index];
}

vec2 Model::texcoord(int face_index, int nthvert) const
{
    const FaceVertex &fv = faces_[face_index][nthvert];
    return texcoords_[fv.texcoord_index];
}

vec3 Model::normal(int face_index, int nthvert) const
{
    const FaceVertex &fv = faces_[face_index][nthvert];
    return normals_[fv.normal_index];
}

bool Model::has_normalmap() const
{
    return normalmap_loaded_;
}

TGAColor Model::sample_texture(const TGAImage &texture, vec2 uv)
{
    assert(texture.width() > 0);
    assert(texture.height() > 0);

    uv.x = std::clamp(uv.x, 0.0, 1.0);
    uv.y = std::clamp(uv.y, 0.0, 1.0);

    const int x = std::clamp(
        static_cast<int>(
            uv.x * (texture.width() - 1)),
        0,
        texture.width() - 1);

    const int y = std::clamp(
        static_cast<int>(
            (1. - uv.y) * (texture.height() - 1)),
        0,
        texture.height() - 1);

    return texture.get(x, y);
}

vec3 Model::normal_from_map(vec2 uv) const
{
    assert(normalmap_loaded_);
    assert(normalmap_.width() > 0);
    assert(normalmap_.height() > 0);

    const TGAColor color = sample_texture(normalmap_, uv);

    // TGAColor 的存储顺序是 BGRA
    const double nx = 2. * color.bgra[2] / 255. - 1.;
    const double ny = 2. * color.bgra[1] / 255. - 1.;
    const double nz = 2. * color.bgra[0] / 255. - 1.;
    return normalized(vec3{nx, ny, nz});
}

TGAColor Model::diffuse_from_map(vec2 uv) const
{
    assert(diffusemap_loaded_);
    assert(diffusemap_.width() > 0);
    assert(diffusemap_.height() > 0);
    return sample_texture(diffusemap_, uv);
}

double Model::specular_from_map(vec2 uv) const
{
    assert(specularmap_loaded_);
    assert(specularmap_.width() > 0);
    assert(specularmap_.height() > 0);
    return sample_texture(specularmap_, uv)[0] / 255.;
}
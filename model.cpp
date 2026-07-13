#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "model.h"

Model::Model(const char *filename) : verts_(), texcoords_(), normals_(), faces_()
{
    std::ifstream in;
    in.open(filename, std::ifstream::in);
    if (in.fail())
    {
        std::cerr << "Fail to open " << filename << std::endl;
        return;
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
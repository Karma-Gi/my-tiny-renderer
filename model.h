// model.h
#pragma once // 防止头文件被重复包含
#include <vector>
#include "geometry.h"
#include "tgaimage.h"

struct FaceVertex
{
    int vertex_index = -1;
    int texcoord_index = -1;
    int normal_index = -1;
};

using Face = std::vector<FaceVertex>;

class Model
{
private:
    // OBJ 文件中的原始数据
    std::vector<vec3> verts_;
    std::vector<vec2> texcoords_;
    std::vector<vec3> normals_;

    // 每个面由若干 FaceVertex 组成
    std::vector<Face> faces_;

    // TGA for normal mapping
    TGAImage normalmap_;
    bool normalmap_loaded_ = false;

public:
    explicit Model(const char *filename); // 构造函数必须是 public，允许外部实例化

    // 数量
    int nfaces() const; // 返回多边形面的总数

    // 根据原始全局索引获取数据
    vec3 vert(int index) const;
    vec2 texcoord(int index) const;
    vec3 normal(int index) const;

    // 获取一个面的索引信息
    const Face &face(int face_index) const;

    // 供 shader 使用的便捷接口：
    // face_index 表示第几个面
    // nthvert 表示该面的第几个顶点
    vec3 vert(int face_index, int nthvert) const;
    vec2 texcoord(int face_index, int nthvert) const;
    vec3 normal(int face_index, int nthvert) const;

    bool has_normalmap() const;
    // 根据 UV 从 _nm.tga 中采样模型空间法线
    vec3 normal_from_map(const vec2 uv) const;
};
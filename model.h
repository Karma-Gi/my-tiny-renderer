// model.h
#pragma once // 这是一个好习惯，防止头文件被重复包含
#include <vector>
#include "geometry.h"

class Model
{
private:
    std::vector<vec3> verts_;
    std::vector<std::vector<int>> faces_;

public:
    Model(const char *filename); // 构造函数必须是 public，允许外部实例化

    int nfaces();                   // 返回多边形面的总数
    vec3 vert(int i);               // 返回第i个顶点的xyz坐标
    std::vector<int> face(int idx); // 别忘了我们在画线循环里需要获取面的索引！
};
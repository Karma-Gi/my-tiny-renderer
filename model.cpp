#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "model.h"

Model::Model(const char *filename) : verts_(), faces_()
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

        // 接下来我们要分析提取 line 里面的数据
        // 解析顶点坐标：以 "v " 开头
        if (!line.compare(0, 2, "v "))
        {
            iss >> trash; // 吃掉字符 'v'
            vec3 v;
            iss >> v.x >> v.y >> v.z; // 读取坐标
            verts_.push_back(v);      // 存入顶点仓库
        }

        else if (!line.compare(0, 2, "f "))
        {
            std::vector<int> f;
            int idx, itrash;
            iss >> trash; // 吃掉 ‘f’

            // 循环读取每一组 "顶点/纹理/法线" (v/vt/vn)
            while (iss >> idx >> trash >> itrash >> trash >> itrash)
            {
                idx--; // 关键：OBJ文件索引从1开始，C++数组从0开始
                f.push_back(idx);
            }
            faces_.push_back(f);
        }
    }
    // 读取完毕后打印一下结果，确保我们成功了
    std::cerr << "# v# " << verts_.size() << " f# " << faces_.size() << std::endl;
}

// 返回多边形面的总数
int Model::nfaces()
{
    return faces_.size();
}

// 返回第i个顶点的xyz坐标
vec3 Model::vert(int i)
{
    return verts_[i];
}

// 返回第idx个面的顶点全局索引
std::vector<int> Model::face(int idx)
{
    return faces_[idx];
}
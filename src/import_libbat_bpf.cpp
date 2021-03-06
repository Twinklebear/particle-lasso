#include <cassert>
#include <algorithm>
#include <map>
#include <iostream>
#include <array>
#include <limits>
#include <unordered_map>
#include <fstream>
#include "import_libbat_bpf.h"
#include "json.hpp"

using namespace pl;
using json = nlohmann::json;

enum LIBBAT_DTYPE {
    UNKNOWN,
    INT_8,
    UINT_8,
    INT_16,
    UINT_16,
    INT_32,
    UINT_32,
    INT_64,
    UINT_64,
    FLOAT_32,
    FLOAT_64,
    VEC2_I8,
    VEC2_U8,
    VEC2_I16,
    VEC2_U16,
    VEC2_I32,
    VEC2_U32,
    VEC2_FLOAT,
    VEC2_DOUBLE,
    VEC3_I8,
    VEC3_U8,
    VEC3_I16,
    VEC3_U16,
    VEC3_I32,
    VEC3_U32,
    VEC3_FLOAT,
    VEC3_DOUBLE,
    VEC4_I8,
    VEC4_U8,
    VEC4_I16,
    VEC4_U16,
    VEC4_I32,
    VEC4_U32,
    VEC4_FLOAT,
    VEC4_DOUBLE,
    MAT2_I8,
    MAT2_U8,
    MAT2_I16,
    MAT2_U16,
    MAT2_I32,
    MAT2_U32,
    MAT2_FLOAT,
    MAT2_DOUBLE,
    MAT3_I8,
    MAT3_U8,
    MAT3_I16,
    MAT3_U16,
    MAT3_I32,
    MAT3_U32,
    MAT3_FLOAT,
    MAT3_DOUBLE,
    MAT4_I8,
    MAT4_U8,
    MAT4_I16,
    MAT4_U16,
    MAT4_I32,
    MAT4_U32,
    MAT4_FLOAT,
    MAT4_DOUBLE,
};

void pl::import_libbat_bpf(const FileName &file_name, ParticleModel &model) {
    std::ifstream fin(file_name.c_str(), std::ios::binary);

    uint64_t json_header_size = 0;
    fin.read(reinterpret_cast<char *>(&json_header_size), sizeof(uint64_t));
    const uint64_t total_header_size = json_header_size + sizeof(uint64_t);

    std::vector<uint8_t> json_header(json_header_size, 0);
    fin.read(reinterpret_cast<char *>(json_header.data()), json_header.size());

    json header = json::parse(json_header);

	auto points = std::make_shared<DataT<float>>();
    points->data.resize(3 * header["num_points"].get<uint64_t>());
    fin.read(reinterpret_cast<char *>(points->data.data()), points->data.size() * sizeof(float));
    model["positions"] = points;

    for (size_t i = 0; i < header["attributes"].size(); ++i) {
        const std::string name = header["attributes"][i]["name"].get<std::string>();
        const LIBBAT_DTYPE dtype = static_cast<LIBBAT_DTYPE>(header["attributes"][i]["dtype"].get<int>());
        if (dtype != FLOAT_32 && dtype != FLOAT_64) {
            std::cout << "Skipping unhandled dtype for attribute " << name << "\n";
            continue;
        }

        const size_t offset = header["attributes"][i]["offset"].get<uint64_t>();
        const size_t size = header["attributes"][i]["size"].get<uint64_t>();
        fin.seekg(offset + total_header_size);

        if (dtype == FLOAT_32) {
            auto data = std::make_shared<DataT<float>>();
            fin.read(reinterpret_cast<char *>(data->data.data()), size);
            model[name] = data;
        } else {
            auto data = std::make_shared<DataT<double>>();
            fin.read(reinterpret_cast<char *>(data->data.data()), size);
            model[name] = data;
        }
    }
}


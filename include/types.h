#ifndef TYPES_H
#define TYPES_H

#include <vector>
#include <unordered_map>
#include <string>

struct vec3f {
	float x, y, z;

	vec3f(float x = 1.0, float y = 1.0, float z = 1.0);
};
vec3f operator+(const vec3f &a, const vec3f &b);
vec3f operator*(const vec3f &a, const vec3f &b);
std::ostream& operator<<(std::ostream &os, const vec3f &v);

struct FileName {
	std::string file_name;

	FileName(const std::string &file_name);
	FileName path() const;
	std::string extension() const;
	FileName join(const FileName &other) const;
};
std::ostream& operator<<(std::ostream &os, const FileName &f);

using ParticleModel = std::unordered_map<std::string, std::vector<float>>;

#endif


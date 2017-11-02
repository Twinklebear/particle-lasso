#ifndef TYPES_H
#define TYPES_H

#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>
#include <memory>
#include <iostream>
#include <typeinfo>

template<typename T>
T clamp(T x, T lo, T hi){
	if (x < lo){
		return lo;
	}
	if (x > hi){
		return hi;
	}
	return x;
}

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

struct Data {
	virtual const std::type_info& type() const = 0;
	// Dump the data in binary format to the output stream as raw data
	virtual void write(std::ofstream &os) const = 0;
	virtual ~Data(){}
};

template<typename T>
struct DataT : Data {
	std::vector<T> data;

	const std::type_info& type() const override {
		return typeid(T);
	}
	void write(std::ofstream &os) const override {
		std::cout << "Writing " << data.size() << " " << typeid(T).name()
			<< ", file is " << sizeof(T) * data.size() << " bytes\n";
		os.write(reinterpret_cast<const char*>(data.data()), sizeof(T) * data.size());
	}
};

using ParticleModel = std::unordered_map<std::string, std::unique_ptr<Data>>;

#endif


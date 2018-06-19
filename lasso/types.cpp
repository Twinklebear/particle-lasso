#include <ostream>
#include "types.h"

vec3f::vec3f(float x, float y, float z) : x(x), y(y), z(z){}
vec3f operator+(const vec3f &a, const vec3f &b){
	return vec3f(a.x + b.x, a.y + b.y, a.z + b.z);
}
vec3f operator*(const vec3f &a, const vec3f &b){
	return vec3f(a.x * b.x, a.y * b.y, a.z * b.z);
}
std::ostream& operator<<(std::ostream &os, const vec3f &v){
	os << "[ " << v.x << ", " << v.y << ", " << v.z << " ]";
	return os;
}

FileName::FileName(const std::string &file_name) : file_name(file_name){}
FileName& FileName::operator=(const std::string &name) {
	file_name = name;
	return *this;
}
FileName FileName::path() const {
	size_t fnd = file_name.rfind("/");
	if (fnd != std::string::npos){
		return FileName(file_name.substr(0, fnd + 1));
	}
	return FileName("");
}
std::string FileName::extension() const {
	size_t fnd = file_name.rfind(".");
	if (fnd != std::string::npos){
		return file_name.substr(fnd + 1);
	}
	return "";
}
FileName FileName::join(const FileName &other) const {
	return FileName(file_name + "/" + other.file_name);
}
std::ostream& operator<<(std::ostream &os, const FileName &f){
	os << f.file_name;
	return os;
}

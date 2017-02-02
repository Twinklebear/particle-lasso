#version 430 core

#include "view_info.glsl"

layout(location = 0) in vec3 pos;

void main(void) {
	gl_Position = proj * view * vec4(pos, 1);
	const float radius = 0.5;
	gl_PointSize = radius * 100.0 / gl_Position.z;
}


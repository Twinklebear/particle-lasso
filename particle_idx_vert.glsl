#version 430 core

#include <view_info.glsl>

uniform float particle_radius;

layout(location = 0) in vec3 pos;

void main(void) {
	gl_Position = proj * view * vec4(pos, 1);
	gl_PointSize = particle_radius * 100.0 / gl_Position.z;
}


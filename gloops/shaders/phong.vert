#version 420

layout(location = 0) in vec3 position;
layout(location = 2) in vec3 normal;

out vec3 out_pos;
out vec3 out_normal;

uniform mat4 mvp;

void main(){
	out_pos = position;
	out_normal = normal;
	gl_Position = mvp*vec4(position,1.0);
}
#version 420

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

out vec2 out_uv;

uniform mat4 mvp;

void main(){
	out_uv = uv;
	gl_Position = mvp*vec4(position,1.0);
}
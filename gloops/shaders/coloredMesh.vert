#version 420

layout(location = 0) in vec3 position;
layout(location = 3) in vec3 color;

out vec3 out_color;

uniform mat4 mvp;

void main(){
	out_color = color;
	gl_Position = mvp*vec4(position,1.0);
}
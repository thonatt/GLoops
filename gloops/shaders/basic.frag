#version 420

layout(location = 0) out vec4 outColor;

uniform vec4 color;

void main(){
	outColor = vec4(color);
}
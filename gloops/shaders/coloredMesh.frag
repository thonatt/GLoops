#version 420

layout(location = 0) out vec4 color;

in vec3 out_color;

void main(){
	color = vec4(out_color, 1.0);
}
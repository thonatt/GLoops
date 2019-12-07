#version 420

layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D tex;

in vec2 out_uv;
uniform float alpha = 1.0;

void main(){
	color = vec4(texture(tex, out_uv).rgb, alpha);
}
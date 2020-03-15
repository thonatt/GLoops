
#version 420
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler3D tex; 

in VertexData {
	vec3 position, normal, color;
	vec2 uv;
} frag_in;

uniform vec3 bmin, bmax;

void main(){
	float value = texture(tex, (frag_in.position - bmin)/(bmax-bmin)).x;
	value = round(5*value)/5.0;
	outColor = vec4(vec3(1), value);
}
#version 420

layout(location = 0) out vec4 color;

in vec3 out_pos;
in vec3 out_normal;

uniform vec3 light_pos;
uniform vec3 cam_pos;

void main(){

	const float kd = 0.3;
    const float ks = 0.2;
	const vec3 meshColor = vec3(0.7);

	vec3 L = normalize(light_pos - out_pos);
	vec3 N = normalize(out_normal);
	vec3 V = normalize(cam_pos - out_pos);
	vec3 R = reflect(-L,N);
	float diffuse = max(0.0, dot(L,N));
    float specular = max(0.0, dot(R,V));
	
	color = vec4( (1.0 - kd - kd)*meshColor + (kd*diffuse + ks*specular)*vec3(1.0) , 1.0);
}
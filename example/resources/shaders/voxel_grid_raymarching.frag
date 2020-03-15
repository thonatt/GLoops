
#version 420
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler3D tex; 

in VertexData {
	vec3 position, normal, color;
	vec2 uv;
} frag_in;

uniform vec3 eye_pos, bmin, bmax;
uniform ivec3 gridSize;
uniform float intensity;

vec4 sampleTex(ivec3 cell) {
	return texture(tex, (vec3(cell) + 0.5)/vec3(gridSize));
}

vec4 sampleTex(vec3 pos) {
	return texture(tex, (pos - bmin)/(bmax-bmin));
}

int getMinIndex(vec3 v) {
	return v.x <= v.y ? (v.x <= v.z ? 0 : 2 ) : (v.y <= v.z ? 1 : 2);
}

float maxCoef(vec3 v){
	return max(v.x,max(v.y,v.z));
}

float minCoef(vec3 v){
	return min(v.x,min(v.y,v.z));
}

ivec3 getCell(vec3 p){
	vec3 uv = (p - bmin)/(bmax - bmin);
	return ivec3(floor(vec3(gridSize) * uv));
}

float boxIntersection(vec3 rayDir) {
	vec3 minTs = (bmin - eye_pos)/rayDir;
	vec3 maxTs = (bmax - eye_pos)/rayDir;

	float nearT = maxCoef(min(minTs,maxTs)); 
	float farT = minCoef(max(minTs,maxTs)); 
	if( 0 <= nearT && nearT <= farT){
		return nearT;
	}
	return -1.0;
}

void main(){
	
	vec3 dir = normalize(frag_in.position - eye_pos);

	//setup start
	vec3 start = eye_pos;
			
	if(!(all(greaterThanEqual(eye_pos, bmin)) && all(greaterThanEqual(bmax, eye_pos)))){
		float distToBox = boxIntersection(dir);
		if(distToBox >= 0){
			start = eye_pos + distToBox*dir;
		} else {
			discard;
		}
	} 

	vec3 cellSize = (bmax - bmin)/vec3(gridSize); 
	start = clamp(start, bmin, bmax - 0.001*cellSize);
	ivec3 currentCell = getCell(start);

	//setup raymarching
	vec3 deltas = cellSize / abs(dir);
	vec3 fracs = fract((start - bmin)/cellSize);			
	vec3 ts;
	ivec3 finalCell, steps;
	for(int k=0; k<3; ++k){
		steps[k] = (dir[k] >= 0 ? 1 : -1);
		ts[k] = deltas[k] * (dir[k] >= 0 ? 1.0 - fracs[k] : fracs[k]);
		finalCell[k] = (dir[k] >= 0 ? gridSize[k] : -1);
	}

	float t = 0, alpha = 0;

	//actual raymarching
	do {
		int c = getMinIndex(ts);
		currentCell[c] += steps[c];

		float delta_t = ts[c] - t;
		t = ts[c];

		ts[c] += deltas[c];
		
		alpha += delta_t * sampleTex(currentCell).x;
		//delta_t * sampleTex(start + (t - 0.5*delta_t)*dir).x;
	
	} while (all(notEqual(currentCell, finalCell)));
			
	outColor = vec4(mix(vec3(1,1,0),vec3(1), intensity*alpha),  min(5*alpha, 1.0));
}
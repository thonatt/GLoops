
#version 420
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler3D tex; 

in VertexData {
	vec3 position, normal, color;
	vec2 uv;
} frag_in;

uniform vec3 eye_pos, bmin, bmax;
uniform ivec3 gridSize;
uniform float sdf_offset = 0.0, epsilon = 0.001;

float sampleTex(vec3 pos) {
	return texture(tex, (pos - bmin)/(bmax-bmin)).x;
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

int getMinIndex(vec3 v) {
	return v.x <= v.y ? (v.x <= v.z ? 0 : 2 ) : (v.y <= v.z ? 1 : 2);
}

bool insideBox(vec3 p){
	return all(greaterThanEqual(p, bmin)) && all(greaterThanEqual(bmax, p));
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


void sdf() {
	vec3 dir = normalize(frag_in.position - eye_pos);

	float t = 0;
	if(!insideBox(eye_pos)){
		t = boxIntersection(dir);
	} 
	
	vec3 p;
	for(int i=0; i<124; ++i){
		p = eye_pos + t*dir;
		if(!insideBox(p)){
			discard;
		}

		float d = 1.0 - sampleTex(p).x;
		if(d < 0.001) {
			outColor = vec4(1.0, 0.0, 0.0, 1.0);
			return;
		}

		t += 0.01*d;
	}
	discard;
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

	float t = 0;
	//actual raymarching
	do {
		int c = getMinIndex(ts);
		currentCell[c] += steps[c];

		float delta_t = ts[c] - t;
		t = ts[c];

		ts[c] += deltas[c];
		
		float d = sampleTex(start + t*dir);

		if(d >= sdf_offset) {
			float previous_d = sampleTex(start + (t - delta_t)*dir);
			float interpolated_t = t - d*delta_t / (d - previous_d);

			vec3 p = start + interpolated_t*dir;
			
//			const vec2 k = vec2(1,-1);
//			vec3 N = normalize( 
//				k.xyy*sampleTex( p + k.xyy*epsilon ) + 
//                k.yyx*sampleTex( p + k.yyx*epsilon ) + 
//                k.yxy*sampleTex( p + k.yxy*epsilon ) + 
//                k.xxx*sampleTex( p + k.xxx*epsilon ) 
//			);
			const vec2 h = vec2(epsilon, 0);
			vec3 N = normalize(vec3(
				sampleTex(p+h.xyy) - sampleTex(p-h.xyy),
				sampleTex(p+h.yxy) - sampleTex(p-h.yxy),
				sampleTex(p+h.yyx) - sampleTex(p-h.yyx)
			) );

			vec3 L = normalize(eye_pos - p);
			vec3 R = reflect(-L,N);
			float diffuse = max(0.0, dot(L,N));
			float specular = max(0.0, dot(R,L));
			
			outColor = vec4(0.5*vec3(0.7) + (0.3*diffuse + 0.2*specular)*vec3(1.0) , 1.0);
			outColor = vec4(vec3(0.5*N + 0.5), 1.0);
			outColor = vec4(vec3(interpolated_t), 1.0);
			
			return;
		}

		//delta_t * sampleTex(start + (t - 0.5*delta_t)*dir).x;
	
	} while (all(notEqual(currentCell, finalCell)));

	discard;

}
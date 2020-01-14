		#version 430
		layout(location = 0) out vec4 outColor;

		in vec3 world_position;
		uniform vec3 eye_pos;
		uniform vec3 bmin;
		uniform vec3 bmax;
		uniform int gridSize;

		int getMinIndex(vec3 v) {
			return v.x < v.y ? (v.x < v.z ? 0 : 2 ) : (v.y < v.z ? 1 : 2);
		}

		float maxCoef(vec3 v){
			return max(v.x,max(v.y,v.z));
		}

		float minCoef(vec3 v){
			return min(v.x,min(v.y,v.z));
		}

		ivec3 getCell(vec3 p){
			vec3 uv = (p - bmin)/(bmax - bmin);
			return ivec3(floor(float(gridSize) * uv));
		}

		bool check(vec3 v){
			return !(any(isnan(v)) || any(isinf(v)));
		}

		bool checkCell(ivec3 c){
			return all(greaterThanEqual(c, ivec3(0))) && all(greaterThanEqual(ivec3(gridSize) - 1, c));
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
	
			outColor = vec4(0,0,0,1);

			vec3 dir = normalize(world_position - eye_pos);

			//setup start
			vec3 start = eye_pos;
			
			if(!(all(greaterThanEqual(eye_pos, bmin)) && all(greaterThanEqual(bmax, eye_pos)))){
				float distToBox = boxIntersection(dir);
				//outColor.x = distToBox;
				//return;
				if(distToBox >= 0){
					start = eye_pos + distToBox*dir;
				} else {
					return;
				}
			} 

			vec3 cellSize = (bmax - bmin)/float(gridSize); 
			start = clamp(start, bmin + 0.1*cellSize, bmax - 0.1*cellSize);
			ivec3 currentCell = getCell(start);

			if(!checkCell(currentCell)){
				if(any(lessThan(currentCell, ivec3(0)))){
					outColor = vec4(1,1,0,1);
				} else if(any(lessThan(ivec3(gridSize) - 1, currentCell))){
					outColor = vec4(0,1,1,1);
				}	
				return;
			}

			//setup raymarching
			vec3 deltas = cellSize / abs(dir);
			vec3 fracs = fract((start - bmin)/cellSize);			
			vec3 ts;
			ivec3 finalCell, steps;
			for(int k=0; k<3; ++k){
				steps[k] = (dir[k] >= 0 ? 1 : -1);
				ts[k] = deltas[k] * (dir[k] >= 0 ? 1.0 - fracs[k] : fracs[k]);
				finalCell[k] = (dir[k] >= 0 ? gridSize : - 1);
			}
			
			float alpha = 0.0;

			//outColor.xyz = 0.5 + 0.8*((vec3(finalCell) - bmin)/(bmax - bmin) - 0.5);
			//return;

			//actual raymarching

			int c;
			float counter = 0;
			do {
				if(!checkCell(currentCell)){
					if(any(lessThan(currentCell, ivec3(0)))){
						outColor = (counter/float(gridSize))*vec4(1,1,0,1);
					} else if(any(lessThan(ivec3(gridSize) - 1, currentCell))){
						outColor =(counter/float(gridSize))* vec4(0,1,1,1);
					}	
					return;
				}

				alpha += 1.0;

				c = getMinIndex(ts);
				currentCell[c] += steps[c];
				ts[c] += deltas[c];
				counter += 1.0;

			} while (currentCell[c] != finalCell[c]);

			alpha /= float(2*gridSize);
			outColor.xyz = mix(vec3(0,0,1), vec3(1,0,0), alpha);
		}
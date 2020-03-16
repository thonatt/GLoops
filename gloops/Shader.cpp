#include "Shader.hpp"
#include "Debug.hpp"

#include <vector>
#include <filesystem>

#include "Texture.hpp"
#include "Mesh.hpp"
#include "Utils.hpp"

namespace gloops {

	GLuniformInternalBase::GLuniformInternalBase(const std::string& _name)
		: name(_name) {
	}

	bool GLuniformInternalBase::setupLocation(GLuint shaderID)
	{
		location = glGetUniformLocation(shaderID, name.c_str());
		if (location >= 0) {
			location_map[shaderID] = location;
		} else {
			std::cout << " Warning : cant find uniform \"" << name << "\" in program " << shaderID << std::endl;
		}
		return location >= 0;
	}

	void GLuniformInternalBase::switchShader(GLuint shaderID) const
	{
		const auto locIt = location_map.find(shaderID);
		if (locIt != location_map.end()) { 
			location = locIt->second;
		} else {
			std::cout << " cant find location of " << name << std::endl;
		}
	}

	Shader::Shader(GLuint shader_type, const std::string& shader_str)
		: str(shader_str), type(shader_type)
	{
		id = GLptr(
			[&](GLuint* ptr) { *ptr = glCreateShader(shader_type); },
			[](const GLuint* ptr) { glDeleteShader(*ptr); }
		);

		char const* c_str = str.c_str();
		glShaderSource(id, 1, &c_str, NULL);
		glCompileShader(id);

		checkCompilation();
	}

	void Shader::attachTo(GLuint program) const
	{
		if (!id || !program) {
			return;
		}
		glAttachShader(program, id);
	}

	void Shader::detachFrom(GLuint program) const
	{
		if (!id || !program) {
			return;
		}
		glDetachShader(program, id);
	}

	const std::string& Shader::getStr() const
	{
		return str;
	}

	GLenum Shader::getType() const
	{
		return type;
	}

	Shader::operator bool() const
	{
		return id && compileStatus;
	}

	void Shader::checkCompilation()
	{
		int infoLogLength;
		glGetShaderiv(id, GL_COMPILE_STATUS, &compileStatus);
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 1) {
			std::vector<char> errorMessage(static_cast<size_t>(infoLogLength) + 1);
			glGetShaderInfoLog(id, infoLogLength, NULL, errorMessage.data());
			std::cout << "shader " << (compileStatus ? "warning" : "error") << " : " << std::string(errorMessage.data()) << std::endl;
			std::cout << getStr() << std::endl;
		}
	}

	ShaderProgram::ShaderProgram()
	{
		id = GLptr(
			[](GLuint* ptr) { *ptr = glCreateProgram(); },
			[](const GLuint* ptr) { glDeleteProgram(*ptr); }
		);
	}

	void ShaderProgram::init(const std::string& vertex, const std::string& frag)
	{
		init(vertex, "", frag);
	}

	void ShaderProgram::init(const std::string & vert, const std::string& geom, const std::string & frag)
	{
		setupShader(ShaderType::VERTEX, vert);
		setupShader(ShaderType::FRAGMENT, frag);
		setupShader(ShaderType::GEOMETRY, geom);
	}

	void ShaderProgram::initFromPaths(const std::string& vertex, const std::string& frag)
	{
		initFromPaths(vertex, "", frag);
	}

	void ShaderProgram::initFromPaths(const std::string& vertex, const std::string& geom, const std::string& frag)
	{
		auto str_from_path = [](const std::string& path) {

			if (path == "") {
				return std::string("");
			}

			std::filesystem::path fspath(path);
			//std::cout << std::filesystem::absolute(std::filesystem::path(".")).string() << std::endl;
			//std::cout << std::filesystem::absolute(fspath).string() << std::endl;
			if (!std::filesystem::exists(fspath)) {
				return "cant load " + fspath.string();
			}
			return loadFile(fspath.string());
		};

		init(str_from_path(vertex), str_from_path(geom), str_from_path(frag));
	}

	void ShaderProgram::setupShader(ShaderType type, const std::string& str)
	{
		if (str != "") {
			setupShader(Shader(static_cast<GLuint>(type), str));
		} else {
			shaders.erase(type);
			linkStatus = GL_FALSE;
			uniformsStatus = GL_FALSE;
		}
	}

	void ShaderProgram::setupShader(const Shader& shader)
	{
		shaders[static_cast<ShaderType>(shader.getType())] = shader;
		linkStatus = GL_FALSE;
		uniformsStatus = GL_FALSE;
	}

	void ShaderProgram::use() const
	{
		if (!linkStatus) {
			linkProgram();
		}
		if (!uniformsStatus) {
			locateUniforms();
		}

		glUseProgram(id);
		for (const auto& uniform : uniforms) {
			uniform->switchShader(id);
			uniform->send();
		}
	}

	void ShaderProgram::addUniforms()
	{
		uniformsStatus = GL_FALSE;
	}

	void ShaderProgram::removeUniforms()
	{
		uniformsStatus = GL_FALSE;
	}

	void ShaderProgram::linkProgram() const
	{
		for (auto& shader : shaders) {
			shader.second.attachTo(id);
		}

		glLinkProgram(id);
		checkLink();

		for (auto& shader : shaders) {
			shader.second.detachFrom(id);
		}
	}

	void ShaderProgram::checkLink() const
	{
		int infoLogLength;
		glGetProgramiv(id, GL_LINK_STATUS, &linkStatus);
		glGetProgramiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 1) {
			std::vector<char> errorMessage(static_cast<size_t>(infoLogLength) + 1);
			glGetProgramInfoLog(id, infoLogLength, NULL, errorMessage.data());
			std::cout << "shader program " << (linkStatus ? "warning" : "error") << " : " << std::string(errorMessage.data()) << std::endl;
		}
	}

	void ShaderProgram::locateUniforms() const
	{
		for (auto& uniform : uniforms) {
			uniform->setupLocation(id);
		}
		uniformsStatus = GL_TRUE;
	}

	ShaderCollection::ShaderCollection()
	{
		initBasic();
		initPhong();
		initColoredMesh();
		initTexturedMesh();
		initNormals();
		initCubemap();
		initUVs();
	}

	void ShaderCollection::renderBasicMesh(const Cameraf& eye, const MeshGL& mesh, const v3f& c)
	{
		renderBasicMesh(eye, mesh, v4f(c[0], c[1], c[2], 1.0));
	}

	void ShaderCollection::renderBasicMesh(const Cameraf& eye, const MeshGL& mesh, const v4f& _color)
	{
		setMVP(eye, mesh);
		color = _color;
		shaderPrograms.at(Name::BASIC).use();
		mesh.draw();
	}

	void ShaderCollection::renderPhongMesh(const Cameraf& eye, const v3f& light_position, const MeshGL& mesh)
	{
		setMVP(eye, mesh);
		cam_pos = eye.position();
		light_pos = light_position;
		shaderPrograms.at(Name::PHONG).use();
		mesh.draw();
	}

	void ShaderCollection::renderPhongMesh(const Cameraf& eye, const MeshGL& mesh)
	{
		renderPhongMesh(eye, eye.position(), mesh);
	}

	void ShaderCollection::renderColoredMesh(const Cameraf& eye, const MeshGL& mesh)
	{
		setMVP(eye, mesh);
		shaderPrograms.at(Name::COLORED_MESH).use();
		mesh.draw();
	}

	void ShaderCollection::renderTexturedMesh(const Cameraf& eye, const MeshGL& mesh, const Texture& tex, float _alpha, float _lod)
	{
		setMVP(eye, mesh);
		alpha = _alpha;
		lod = _lod;
		tex.bindSlot(GL_TEXTURE0);
		shaderPrograms.at(Name::TEXTURED_MESH).use();
		mesh.draw();
	}

	void ShaderCollection::renderTexturedMesh(const MeshGL& mesh, const Texture& tex, float _alpha, float _lod)
	{
		vp = m4f::Identity();
		model = mesh.model();
		alpha = _alpha;
		lod = _lod;
		tex.bindSlot(GL_TEXTURE0);
		shaderPrograms.at(Name::TEXTURED_MESH).use();
		mesh.draw();
	}

	void ShaderCollection::renderUVs(const Cameraf& eye, const MeshGL& mesh)
	{
		setMVP(eye, mesh);
		shaderPrograms.at(Name::UVS).use();
		mesh.draw();
	}

	void ShaderCollection::renderNormals(const Cameraf& eye, const MeshGL& mesh, float _size, const v4f& _color)
	{
		setMVP(eye, mesh);
		size = _size;
		color = _color;
		shaderPrograms.at(Name::NORMALS).use();
		mesh.draw();
	}

	void ShaderCollection::renderCubemap(const Cameraf& eye, const v3f& position, float size, const Texture& cubemap)
	{
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

		MeshGL cube = Mesh::getCube().setTranslation(position).setScaling(size).invertFaces();
		setMVP(eye, cube);
		cubemap.bindSlot(GL_TEXTURE0);
		shaderPrograms.at(Name::CUBEMAP).use();
		cube.draw();
		glEnable(GL_DEPTH_TEST);
	}

	void ShaderCollection::setMVP(const Cameraf& eye, const MeshGL& mesh)
	{
		vp = eye.viewProj();
		model = mesh.model();
	}

	const std::string& ShaderCollection::fragLodTexUValpha()
	{
		static const std::string s = R"(
				#version 420
				layout(location = 0) out vec4 color;
				layout(binding = 0) uniform sampler2D tex;

				uniform float alpha = 1.0, lod = -1;

				in VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} frag_in;
		
				void main(){
					if(lod < 0) {
						color = vec4(texture(tex, frag_in.uv).rgb, alpha);
					} else {
						color = vec4(textureLod(tex, frag_in.uv, lod).rgb, alpha);
					}				
				}
			)";
		return s;
	}

	const std::string& ShaderCollection::tevTriDisplacement()
	{
		static const std::string s = R"(
				#version 420
				layout(triangles, equal_spacing, ccw) in;
				layout(binding = 6) uniform sampler2D displacementTex;
				
				uniform mat4 vp, model;
				
				in VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} tev_in[];

				out VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} tev_out;
				
				void main(){ 				
					tev_out.normal = normalize(gl_TessCoord[0]*tev_in[0].normal + gl_TessCoord[1]*tev_in[1].normal + gl_TessCoord[2]*tev_in[2].normal);
					tev_out.color = gl_TessCoord[0]*tev_in[0].color + gl_TessCoord[1]*tev_in[1].color + gl_TessCoord[2]*tev_in[2].color;
					tev_out.uv = gl_TessCoord[0]*tev_in[0].uv + gl_TessCoord[1]*tev_in[1].uv + gl_TessCoord[2]*tev_in[2].uv;
					
					vec3 pos = gl_TessCoord[0]*tev_in[0].position + gl_TessCoord[1]*tev_in[1].position + gl_TessCoord[2]*tev_in[2].position;
					float displacement = texture(displacementTex, tev_out.uv).x;
					vec4 delta_pos = model * vec4(displacement*tev_out.normal, 0.0);
					tev_out.position = pos + delta_pos.xyz;
					gl_Position = vp * vec4(tev_out.position , 1.0);
				}
			)";
		return s;
	}

	const std::string& ShaderCollection::tcsTriInTerface()
	{
		static const std::string s = R"(
				#version 420
				layout(vertices = 3) out;
			
				uniform mat4 vp;
				//uniform vec2 viewport_diagonal;
				uniform float tesselation_size;

				in VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} tcs_in[];

				out VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} tcs_out[];

				void main(){		

					//mat3 screen_positions = mat3(1.0);
					//for(int i=0; i<3; ++i){
					//	vec4 proj = vp*vec4(tcs_in[i].position , 1.0);
					//	screen_positions[i].xy = viewport_diagonal * (proj.xy / proj.w);
					//}
					//float screen_triangle_size = abs(determinant(screen_positions));
					//float ratio = max(1.0, 0.5*sqrt(screen_triangle_size / tesselation_size));

					if (gl_InvocationID == 0){
						gl_TessLevelInner[0] = tesselation_size;
						gl_TessLevelOuter[0] = tesselation_size;
						gl_TessLevelOuter[1] = tesselation_size;
						gl_TessLevelOuter[2] = tesselation_size;
					}

					tcs_out[gl_InvocationID].position = tcs_in[gl_InvocationID].position;  
					tcs_out[gl_InvocationID].normal = tcs_in[gl_InvocationID].normal;  
					tcs_out[gl_InvocationID].color = tcs_in[gl_InvocationID].color;  
					tcs_out[gl_InvocationID].uv = tcs_in[gl_InvocationID].uv;  
					gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
				}		
			)";
		return s;
	}

	const std::string& ShaderCollection::vertexMeshInterface()
	{
		static const std::string s = R"(
				#version 420
				layout(location = 0) in vec3 position;
				layout(location = 1) in vec2 uv;	
				layout(location = 2) in vec3 normal;
				layout(location = 3) in vec3 color;
				
				uniform mat4 vp, model;
				
				out VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} vs_out;

				void main(){
					vec4 pos = model * vec4(position,1.0);
					vs_out.position = pos.xyz;
					vs_out.uv = uv;
					vs_out.normal = transpose(inverse(mat3(model))) * normal;
					vs_out.color = color;
					gl_Position = vp * pos;
				}
			)";
		return s;
	}

	const std::string& ShaderCollection::fragCubemap()
	{
		static const std::string s = R"(
				#version 420
				layout(location = 0) out vec4 outColor;
				layout(binding = 0) uniform samplerCube cubeMap;
				
				in VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} frag_in;

				void main(){
					outColor = texture(cubeMap, frag_in.position);
				}
			)";
		return s;
	}

	const std::string& ShaderCollection::geomNormalTriangle()
	{
		static const std::string s = R"(
				#version 420
				layout(triangles) in;
				layout(line_strip, max_vertices = 2) out;
		
				in VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} vs_in[];

				uniform mat4 model, vp;    
				uniform float size;
				
				void main(void) {
					vec3 a = vs_in[0].position;
					vec3 b = vs_in[1].position;
					vec3 c = vs_in[2].position;

					vec3 tri_normal = normalize(cross(b-a,c-b));
					vec3 tri_center = (a+b+c)/3.0;

					gl_Position = vp*vec4(tri_center, 1.0);
					EmitVertex();
					gl_Position = vp*vec4(tri_center + size*tri_normal, 1.0);
					EmitVertex();
					EndPrimitive();
				}
			)";
		return s;
	}

	const std::string& ShaderCollection::fragUVs()
	{
		static const std::string s = R"(
				#version 420
				layout(location = 0) out vec4 color;

				in VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} frag_in;

				void main(){
					color = vec4(frag_in.uv, 0.0, 1.0);
				}
			)";
		return s;
	}

	const std::string& ShaderCollection::fragColor()
	{
		static const std::string s = R"(
				#version 420
				layout(location = 0) out vec4 color;

				in VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} frag_in;

				void main(){
					color = vec4(frag_in.color, 1.0);
				}
			)";
		return s;
	}

	const std::string& ShaderCollection::fragPhong()
	{
		static const std::string s = R"(
				#version 420

				layout(location = 0) out vec4 color;
				
				in VertexData {
					vec3 position, normal, color;
					vec2 uv;
				} frag_in;

				uniform vec3 light_pos;
				uniform vec3 cam_pos;

				void main(){
					const float kd = 0.3;
					const float ks = 0.2;
					const vec3 meshColor = vec3(0.7);

					vec3 L = normalize(light_pos - frag_in.position);
					vec3 N = normalize(frag_in.normal);
					vec3 V = normalize(cam_pos - frag_in.position);
					vec3 R = reflect(-L,N);
					float diffuse = max(0.0, dot(L,N));
					float specular = max(0.0, dot(R,V));
	
					color = vec4( (1.0 - kd - kd)*meshColor + (kd*diffuse + ks*specular)*vec3(1.0) , 1.0);
				}
			)";
		return s;
	}

	const std::string& ShaderCollection::fragUniformColor()
	{
		static const std::string s = R"(
				#version 420
				layout(location = 0) out vec4 out_color;
				uniform vec4 color;
				void main(){
					out_color = color;
				}
			)";
		return s;
	}

	void ShaderCollection::initBasic()
	{
		ShaderProgram shader;
		shader.init(vertexMeshInterface(), fragUniformColor());
		shader.addUniforms(model, vp, color);
		shaderPrograms[Name::BASIC] = shader;
	}

	void ShaderCollection::initPhong()
	{
		ShaderProgram shader;
		shader.init(vertexMeshInterface(), fragPhong());
		shader.addUniforms(model, vp, light_pos, cam_pos);
		shaderPrograms[Name::PHONG] = shader;
	}

	void ShaderCollection::initColoredMesh()
	{
		ShaderProgram shader;
		shader.init(vertexMeshInterface(), fragColor());
		shader.addUniforms(model, vp);
		shaderPrograms[Name::COLORED_MESH] = shader;
	}

	void ShaderCollection::initTexturedMesh()
	{
		ShaderProgram shader;
		shader.init(vertexMeshInterface(), fragLodTexUValpha());
		shader.addUniforms(model, vp, alpha, lod);
		shaderPrograms[Name::TEXTURED_MESH] = shader;
	}

	void ShaderCollection::initNormals()
	{
		ShaderProgram shader;
		shader.init(vertexMeshInterface(), geomNormalTriangle(), fragUniformColor());
		shader.addUniforms(model, vp, color, size);
		shaderPrograms[Name::NORMALS] = shader;
	}

	void ShaderCollection::initCubemap()
	{
		ShaderProgram shader;
		shader.init(vertexMeshInterface(), fragCubemap());
		shader.addUniforms(model, vp);
		shaderPrograms[Name::CUBEMAP] = shader;
	}

	void ShaderCollection::initUVs()
	{
		ShaderProgram shader;
		shader.init(vertexMeshInterface(), fragUVs());
		shader.addUniforms(model, vp);
		shaderPrograms[Name::UVS] = shader;
	}

}
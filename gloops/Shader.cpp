#include "Shader.hpp"
#include "Debug.hpp"

#include <vector>
#include <filesystem>

#include "Texture.hpp"
#include "Mesh.hpp"

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
			std::cout << " cant find uniform : " << name << std::endl;
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

	ShaderData::ShaderData(GLuint shader_type, const std::string & shader_str)
		: str(shader_str), type(shader_type)
	{
		if (str != "") {
			gl_check();

			id = GLptr(
				[&](GLuint* ptr) { *ptr = glCreateShader(type); },
				[](const GLuint* ptr) { glDeleteShader(*ptr); }
			);

			char const * c_str = str.c_str();
			glShaderSource(id, 1, &c_str, NULL);
			glCompileShader(id);

			GLint Result = GL_FALSE;
			int InfoLogLength;
			glGetShaderiv(id, GL_COMPILE_STATUS, &Result);
			glGetShaderiv(id, GL_INFO_LOG_LENGTH, &InfoLogLength);
			if (InfoLogLength > 1) {
				std::cout << InfoLogLength << std::endl;
				std::vector<char> FragmentShaderErrorMessage(static_cast<size_t>(InfoLogLength) + 1);
				glGetShaderInfoLog(id, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
				std::cout << "shader error : " << std::string(FragmentShaderErrorMessage.data()) << std::endl;
			}

			gl_check();
		}
	}

	void ShaderData::detach()
	{
		if (!id || !program) {
			return;
		}
		glDetachShader(program, id);
	}

	void ShaderData::attachTo(GLptr _program)
	{
		if (!id) {
			return;
		}
		program = _program;
		glAttachShader(program, id);

		gl_check();
	}

	void ShaderProgram::init(const std::string & vert, const std::string & frag, const std::string & geom)
	{
		id = GLptr(
			[](GLuint* ptr) { *ptr = glCreateProgram(); },
			[](const GLuint* ptr) { glDeleteProgram(*ptr); }
		);

		gl_check();

		vertex = ShaderData(GL_VERTEX_SHADER, vert);
		fragment = ShaderData(GL_FRAGMENT_SHADER, frag);
		geometry = ShaderData(GL_GEOMETRY_SHADER, geom);

		gl_check();

		vertex.attachTo(id);
		fragment.attachTo(id);
		geometry.attachTo(id);

		gl_check();

		glLinkProgram(id);

		gl_check();

		GLint Result = GL_FALSE;
		int InfoLogLength;
		glGetProgramiv(id, GL_LINK_STATUS, &Result);
		glGetProgramiv(id, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if (InfoLogLength > 1) {
			std::vector<char> ProgramErrorMessage(static_cast<size_t>(InfoLogLength) + 1);
			glGetProgramInfoLog(id, InfoLogLength, NULL, &ProgramErrorMessage[0]);
			std::cout << "shader program error : " << std::string(ProgramErrorMessage.data()) << std::endl;
			std::cout << std::endl << vert << std::endl << std::endl << frag << std::endl;
		}
	
		vertex.detach();
		geometry.detach();
		fragment.detach();	

		gl_check();
	}

	void ShaderProgram::initFromPaths(const std::string& vertex, const std::string& frag, const std::string& geom)
	{
		auto str_from_path = [](const std::string& path) {

			if (path == "") {
				return std::string("");
			}

			std::filesystem::path fspath(path);
			std::cout << std::filesystem::absolute(std::filesystem::path(".")).string() << std::endl;
			std::cout << std::filesystem::absolute(fspath).string() << std::endl;
			if (!std::filesystem::exists(fspath)) {
				return "cant load " + fspath.string();
			}
			return loadFile(fspath.string());
		};

		init(str_from_path(vertex), str_from_path(frag), str_from_path(geom));
	}

	void ShaderProgram::use() const
	{
		glUseProgram(id);
		for (const auto& uniform : uniforms) {
			uniform->switchShader(id);
			uniform->send();
		}
	}

	ShaderCollection::ShaderCollection()
	{
		initBasic();
		initPhong();
		initColoredMesh();
		initTexturedMesh();
		initNormals();
		initCubemap();
	}

	void ShaderCollection::renderBasicMesh(const Cameraf& eye, const MeshGL& mesh, const v4f& _color)
	{
		mvp = eye.viewProj() * mesh.model();
		color = _color;
		shader_map.at(Name::BASIC).use();
		mesh.draw();
	}

	void ShaderCollection::renderPhongMesh(const Cameraf& eye, const v3f& light_position, const MeshGL& mesh)
	{
		mvp = eye.viewProj() * mesh.model();
		model = mesh.model();
		cam_pos = eye.position();
		light_pos = light_position;
		shader_map.at(Name::PHONG).use();
		mesh.draw();
	}

	void ShaderCollection::renderPhongMesh(const Cameraf& eye, const MeshGL& mesh)
	{
		renderPhongMesh(eye, eye.position(), mesh);
	}

	void ShaderCollection::renderColoredMesh(const Cameraf& eye, const MeshGL& mesh)
	{
		mvp = eye.viewProj() * mesh.model();
		shader_map.at(Name::COLORED_MESH).use();
		mesh.draw();
	}

	void ShaderCollection::renderTexturedMesh(const Cameraf& eye, const MeshGL& mesh, const Texture& tex, float _alpha, float _lod)
	{
		mvp = eye.viewProj() * mesh.model();
		alpha = _alpha;
		lod = _lod;
		tex.bindSlot(GL_TEXTURE0);
		shader_map.at(Name::TEXTURED_MESH).use();
		mesh.draw();
	}

	void ShaderCollection::renderTexturedMesh(const MeshGL& mesh, const Texture& tex, float _alpha, float _lod )
	{
		mvp = m4f::Identity() * mesh.model();
		alpha = _alpha;
		lod = _lod;
		tex.bindSlot(GL_TEXTURE0);
		shader_map.at(Name::TEXTURED_MESH).use();
		mesh.draw();
	}

	void ShaderCollection::renderNormals(const Cameraf& eye, const MeshGL& mesh, float _size, const v4f& _color)
	{
		mvp = eye.viewProj() * mesh.model();
		color = _color;
		size = _size;
		shader_map.at(Name::NORMALS).use();
		mesh.draw();
	}

	void ShaderCollection::renderCubemap(const Cameraf& eye, const v3f& position, float size, const Texture& cubemap)
	{
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

		MeshGL cube = Mesh::getCube().setTranslation(position).setScaling(size).invertFaces();
		vp = eye.viewProj();
		model = cube.model();
		cubemap.bindSlot(GL_TEXTURE0);
		shader_map.at(Name::CUBEMAP).use();
		cube.draw();
		glEnable(GL_DEPTH_TEST);
	}

	void ShaderCollection::initBasic()
	{
		ShaderProgram shader;
		shader.init(
			R"(
				#version 420
				layout(location = 0) in vec3 position;
				uniform mat4 mvp;
				void main() {
					gl_Position = mvp * vec4(position, 1.0);
				}
			)",
			R"(
				#version 420
				layout(location = 0) out vec4 outColor;
				uniform vec4 color;
				void main(){
					outColor = vec4(color);
				}
			)"
		);
		//shader.initFromPaths(shader_folder + "basic.vert", shader_folder + "basic.frag");
		shader.addUniforms(mvp, color);
		shader_map[Name::BASIC] = shader;
	}

	void ShaderCollection::initPhong()
	{
		ShaderProgram shader;
		shader.init(
			R"(
				#version 420

				layout(location = 0) in vec3 position;
				layout(location = 2) in vec3 normal;

				out vec3 out_pos;
				out vec3 out_normal;

				uniform mat4 mvp, model;
	
				void main(){
					out_pos = (model*vec4(position,1.0)).xyz;
					out_normal = transpose(inverse(mat3(model)))*normal;
					gl_Position = mvp*vec4(position,1.0);
				}
			)",
			R"(
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
			)"
		);
		//shader.initFromPaths(shader_folder + "phong.vert", shader_folder + "phong.frag");
		shader.addUniforms(mvp, model, light_pos, cam_pos);
		shader_map[Name::PHONG] = shader;
	}

	void ShaderCollection::initColoredMesh()
	{
		ShaderProgram shader;
		shader.init(
			R"(
				#version 420

				layout(location = 0) in vec3 position;
				layout(location = 3) in vec3 color;

				out vec3 out_color;

				uniform mat4 mvp;

				void main(){
					out_color = color;
					gl_Position = mvp*vec4(position,1.0);
				}
			)",
			R"(
				#version 420

				layout(location = 0) out vec4 color;

				in vec3 out_color;

				void main(){
					color = vec4(out_color, 1.0);
				}
			)"
		);
		//shader.initFromPaths(shader_folder + "coloredMesh.vert", shader_folder + "coloredMesh.frag");
		shader.addUniforms(mvp);
		shader_map[Name::COLORED_MESH] = shader;
	}

	void ShaderCollection::initTexturedMesh()
	{
		ShaderProgram shader;
		shader.init(
			R"(
				#version 420

				layout(location = 0) in vec3 position;
				layout(location = 1) in vec2 uv;

				out vec2 out_uv;

				uniform mat4 mvp;

				void main(){
					out_uv = uv;
					gl_Position = mvp*vec4(position,1.0);
				}
			)",
			R"(
				#version 420
				layout(location = 0) out vec4 color;
				layout(binding = 0) uniform sampler2D tex;

				in vec2 out_uv;
				uniform float alpha = 1.0, lod = -1;

				void main(){
					if(lod < 0) {
						color = vec4(texture(tex, out_uv).rgb, alpha);
					} else {
						color = vec4(textureLod(tex, out_uv, lod).rgb, alpha);
					}				
				}
			)"
		);
		//shader.initFromPaths(shader_folder + "texturedMesh.vert", shader_folder + "texturedMesh.frag");
		shader.addUniforms(mvp, alpha, lod);
		shader_map[Name::TEXTURED_MESH] = shader;
	}

	void ShaderCollection::initNormals()
	{
		ShaderProgram shader;
		shader.init(
			R"(
				#version 420
				layout(location = 0) in vec3 position;
				void main(){
					gl_Position = vec4(position,1.0);
				}
			)",
			R"(
				#version 420
				layout(location = 0) out vec4 outColor;
				uniform vec4 color;
				void main(){
					outColor = vec4(color);
				}
			)",
			R"(
				#version 420
				layout(triangles) in;
				layout(line_strip, max_vertices = 2) out;
				uniform mat4 mvp;    
				uniform float size;

				void main(void) {
					vec3 a = gl_in[0].gl_Position.xyz;
					vec3 b = gl_in[1].gl_Position.xyz;
					vec3 c = gl_in[2].gl_Position.xyz;

					vec3 tri_normal = normalize(cross(b-a,c-b));
					vec3 tri_center = (a+b+c)/3.0;

					gl_Position = mvp*vec4(tri_center,1.0);
					EmitVertex();
					gl_Position = mvp*vec4(tri_center + size*tri_normal, 1.0);
					EmitVertex();
					EndPrimitive();
				}
			)"
		);

		shader.addUniforms(mvp, color, size);
		shader_map[Name::NORMALS] = shader;
	}

	void ShaderCollection::initCubemap()
	{
		ShaderProgram shader;
		shader.init(
			R"(
				#version 420
				layout(location = 0) in vec3 position;
				uniform mat4 vp, model;
				out vec4 pos;
				void main() {
					pos = model * vec4(position, 1.0);
					gl_Position = vp * pos;
				}
			)",
			R"(
				#version 420
				layout(location = 0) out vec4 outColor;
				layout(binding = 0) uniform samplerCube cubeMap;
				in vec4 pos;
				void main(){
					outColor = texture(cubeMap, pos.xyz);
				}
			)"
		);
		shader.addUniforms(vp, model);
		shader_map[Name::CUBEMAP] = shader;
	}

}
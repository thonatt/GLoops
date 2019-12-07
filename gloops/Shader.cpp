#include "Shader.hpp"
#include "Debug.hpp"

#include <vector>
#include <filesystem>

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

	void GLuniformInternalBase::switchShader(GLuint shaderID)
	{
		if (location_map.count(shaderID) > 0) {
			location = location_map[shaderID];
		} else {
			std::cout << " cant find location of " << name << std::endl;
		}
	}

	ShaderData::ShaderData(GLuint shader_type, const std::string & shader_str)
		: type(shader_type), str(shader_str)
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
		for (auto & uniform : uniforms) {
			uniform->switchShader(id);
			uniform->send();
		}
	}

	ShaderCollection::ShaderCollection(const std::string& folder_path)
		: shader_folder(folder_path)
	{
		initBasic();
		initPhong();
		initColoredMesh();
		initTexturedMesh();
	}

	const ShaderProgram& ShaderCollection::get(Name name) 
	{
		return shader_map.at(name);
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

				uniform mat4 mvp;

				void main(){
					out_pos = position;
					out_normal = normal;
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
		shader.addUniforms(mvp, light_pos, cam_pos);
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
				uniform float alpha = 1.0;

				void main(){
					color = vec4(texture(tex, out_uv).rgb, alpha);
				}
			)"
		);
		//shader.initFromPaths(shader_folder + "texturedMesh.vert", shader_folder + "texturedMesh.frag");
		shader.addUniforms(mvp, alpha);
		shader_map[Name::TEXTURED_MESH] = shader;
	}

}
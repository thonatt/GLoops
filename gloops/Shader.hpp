#pragma once

#include "config.hpp"

#include <vector>
#include <memory>
#include <map>

#include "Camera.hpp"

namespace gloops {

	class GLuniformInternalBase {
	public:

		GLuniformInternalBase(const std::string& _name);

		virtual void send() const { }

		bool setupLocation(GLuint shaderID);

		void switchShader(GLuint shaderID) const;

	protected:		
		std::string name;
		std::map<GLuint, GLint> location_map;
		mutable GLint location = 0;
	};

	template<typename T>
	class GLuniformInternal : public GLuniformInternalBase {

		using GLuniformInternalBase::location;

	public:
		GLuniformInternal(const std::string & _name, const T & _t = {})
			: GLuniformInternalBase(_name), t(_t) {}

		void operator=(const T & _t) { 
			t = _t;
		}
		
		T & get() { 
			return t;
		}

		void send() const override;

		virtual ~GLuniformInternal() = default;

	protected:
		T t;
	};

	template<> inline void GLuniformInternal<bool>::send() const {
		glUniform1i(location, t);
	}
	template<> inline void GLuniformInternal<int>::send() const {
		glUniform1i(location, t);
	}
	template<> inline void GLuniformInternal<float>::send() const {
		glUniform1f(location, t);
	}
	template<> inline void GLuniformInternal<v2i>::send() const {
		glUniform2i(location, t[0], t[1]);
	}
	template<> inline void GLuniformInternal<v2f>::send() const {
		glUniform2f(location, t[0], t[1]);
	}
	template<> inline void GLuniformInternal<v3f>::send() const {
		glUniform3f(location, t[0], t[1], t[2]);
	}
	template<> inline void GLuniformInternal<v4f>::send() const {
		glUniform4f(location, t[0], t[1], t[2], t[3]);
	}
	template<> inline void GLuniformInternal<m3f>::send() const {
		glUniformMatrix3fv(location, 1, GL_FALSE, t.data());
	}
	template<> inline void GLuniformInternal<m4f>::send() const {
		glUniformMatrix4fv(location, 1, GL_FALSE, t.data());
	}

	class ShaderProgram;

	template<typename T>
	class Uniform {
	public:

		friend ShaderProgram;

		Uniform(const std::string & _name, const T & _t = {}) {
			data = std::make_shared<GLuniformInternal<T>>(_name, _t);
		}

		void operator=(const Uniform& other) {
			data->get() = other.get();
		}

		template<typename U>
		void operator=(const U & _t) {
			data->get() = static_cast<const T&>(_t); 
		}
		
		operator const T & () const { 
			return data->get();
		}

		T & get() { 
			return data->get();
		}
		
		const T & get() const { 
			return data->get(); 
		}

	protected:
		
		bool setupLocation(GLuint shaderID) {
			return data->setupLocation(shaderID);
		}

		const std::shared_ptr<GLuniformInternal<T>>& getUniformData() const {
			return data;
		}

		std::shared_ptr<GLuniformInternal<T>> data;
	};


	class ShaderData {

	public:
		ShaderData() = default;
		ShaderData(GLuint shader_type, const std::string & shader_str);

		void attachTo(GLptr program);
		void detach();

	protected:
		std::string str;
		GLptr id, program;
		GLuint type = GL_VERTEX_SHADER;
	};

	class ShaderProgram {

	public:
		void init(const std::string & vertex, const std::string & frag, const std::string & geom = "");
		void initFromPaths(const std::string& vertex, const std::string& frag, const std::string& geom = "");

		template<typename T, typename ...Ts>
		void addUniforms(Uniform<T> & unif, Uniform<Ts> & ...unifs) {
			if (unif.setupLocation(id)) {
				uniforms.push_back(unif.getUniformData());
			}
			addUniforms(unifs...);
		}
		
		void use() const;

	private:
		void addUniforms() {}

		std::vector<std::shared_ptr<GLuniformInternalBase>> uniforms;
		ShaderData vertex, fragment, geometry;
		GLptr id;
	};

	class MeshGL;
	class Texture;

	class ShaderCollection {
	public:

		enum class Name {
			BASIC, PHONG, COLORED_MESH, TEXTURED_MESH, NORMALS
		};

		ShaderCollection();
		

		void renderBasicMesh(const Cameraf& eye, const MeshGL& mesh, const v4f& color);
		void renderPhongMesh(const Cameraf& eye, const v3f& light_position, const MeshGL& mesh);
		void renderPhongMesh(const Cameraf& eye, const MeshGL& mesh);
		void renderColoredMesh(const Cameraf& eye, const MeshGL& mesh);
		void renderTexturedMesh(const Cameraf& eye, const MeshGL& mesh, const Texture& tex, float alpha = 1.0f);
		void renderNormals(const Cameraf& eye, const MeshGL& mesh, float size = 1.0f, const v4f& color = { 1,0,1,1 });

		//const ShaderProgram& get(Name name);


	public:
		Uniform<m4f> mvp = { "mvp" }, model = { "model" };
		Uniform<m3f> rotation = { "rotation" };
		Uniform<v4f> color = { "color" };
		Uniform<v3f> light_pos = { "light_pos" }, cam_pos{ "cam_pos" };
		Uniform<float> alpha = { "alpha" }, size = { "size" };

	protected:
		std::map<Name, ShaderProgram> shader_map;

		void initBasic();
		void initPhong();
		void initColoredMesh();
		void initTexturedMesh();
		void initNormals();
	};

}
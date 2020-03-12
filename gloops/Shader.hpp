#pragma once

#include "config.hpp"

#include <memory>
#include <map>
#include <set>

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
	template<> inline void GLuniformInternal<v3i>::send() const {
		glUniform3i(location, t[0], t[1], t[2]);
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


	enum class ShaderType : GLenum {
		VERTEX = GL_VERTEX_SHADER,
		FRAGMENT = GL_FRAGMENT_SHADER,
		GEOMETRY = GL_GEOMETRY_SHADER,
		TESSELATION_CONTROL = GL_TESS_CONTROL_SHADER,
		TESSELATION_EVALUATION = GL_TESS_EVALUATION_SHADER
	};

	class Shader {

	public:
		Shader() = default;
		Shader(GLuint shader_type, const std::string & shader_str);

		void attachTo(GLuint program) const;
		void detachFrom(GLuint program) const;

		const std::string& getStr() const;
		GLenum getType() const;

		operator bool() const;

	protected:
		void checkCompilation();

		std::string str;
		GLptr id;
		GLint compileStatus = GL_FALSE;
		GLenum type = GL_FRAGMENT_SHADER;
	};

	class ShaderProgram {

	public:
		ShaderProgram();

		void init(const std::string& vertex, const std::string& frag);
		void init(const std::string& vertex, const std::string& geom, const std::string& frag);

		void initFromPaths(const std::string& vertex, const std::string& frag);
		void initFromPaths(const std::string& vertex, const std::string& geom, const std::string& frag);

		void setupShader(ShaderType type, const std::string& str);
		void setupShader(const Shader& shader);

		template<typename T, typename ...Ts>
		void addUniforms(Uniform<T>& uniform, Uniform<Ts>& ...unifs);

		template<typename T, typename ...Ts>
		void removeUniforms(Uniform<T>& uniform, Uniform<Ts>& ...unifs);

		void use() const;

	private:
		void addUniforms();
		void removeUniforms();

		void linkProgram() const;
		void checkLink() const;
		void locateUniforms() const;

		std::set<std::shared_ptr<GLuniformInternalBase>> uniforms;
		std::map<ShaderType, Shader> shaders;
		GLptr id;
		mutable GLint linkStatus = GL_FALSE, uniformsStatus = GL_FALSE;
	};

	class MeshGL;
	class Texture;

	class ShaderCollection {
	public:

		enum class Name {
			BASIC, PHONG, COLORED_MESH, TEXTURED_MESH, NORMALS, CUBEMAP, UVS
		};

		ShaderCollection();
		
		void renderBasicMesh(const Cameraf& eye, const MeshGL& mesh, const v3f& color);
		void renderBasicMesh(const Cameraf& eye, const MeshGL& mesh, const v4f& color);
		void renderPhongMesh(const Cameraf& eye, const v3f& light_position, const MeshGL& mesh);
		void renderPhongMesh(const Cameraf& eye, const MeshGL& mesh);
		void renderColoredMesh(const Cameraf& eye, const MeshGL& mesh);
		
		void renderTexturedMesh(const Cameraf& eye, const MeshGL& mesh, const Texture& tex, float alpha = 1.0f, float lod = - 1);
		void renderTexturedMesh(const MeshGL& mesh, const Texture& tex, float alpha = 1.0f, float lod = -1); 
		
		void renderUVs(const Cameraf& eye, const MeshGL& mesh);

		void renderNormals(const Cameraf& eye, const MeshGL& mesh, float size = 1.0f, const v4f& color = { 1,0,1,1 });

		void renderCubemap(const Cameraf& eye, const v3f& position, float size, const Texture& cubemap);
	public:
		Uniform<m4f> mvp = { "mvp" }, model = { "model" }, vp = { "vp" };
		Uniform<v4f> color = { "color" };
		Uniform<v3f> light_pos = { "light_pos" }, cam_pos = { "cam_pos" };
		Uniform<v2f> viewport_diagonal = { "viewport_diagonal" };
		Uniform<float> alpha = { "alpha" }, size = { "size" }, lod = { "lod" }, tesselation_size = { "tesselation_size", 1 };

		std::map<Name, ShaderProgram> shaderPrograms;

	public:
		static const std::string
			//& vertexPosition(), & vertexPositionNormalUV(), & vertexMVP(), & vertexMVPPosition(), & vertexMVPColor(), & vertexMVPUV(), & vertexMVPPositionNormal(),			
			& vertexMeshInterface(), & geomNormalTriangle(), 
			& fragUniformColor(), & fragColor(), & fragPhong(), & fragLodTexUValpha(), & fragCubemap(), &fragUVs(),
			& tcsTriInTerface(), &tevTriDisplacement();

	private:
		void setMVP(const Cameraf& eye, const MeshGL& mesh);

		void initBasic();
		void initPhong();
		void initColoredMesh();
		void initTexturedMesh();
		void initNormals();
		void initCubemap();
		void initUVs();
	};

	template<typename T, typename ...Ts>
	inline void ShaderProgram::addUniforms(Uniform<T>& uniform, Uniform<Ts>& ...unifs)
	{
		uniforms.insert(uniform.getUniformData());
		addUniforms(unifs...);
	}

	template<typename T, typename ...Ts>
	inline void ShaderProgram::removeUniforms(Uniform<T>& uniform, Uniform<Ts>& ...unifs)
	{
		uniforms.erase(uniform.getUniformData());
		removeUniforms(unifs...);
	}

}
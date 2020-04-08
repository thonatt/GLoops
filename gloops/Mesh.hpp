#pragma once

#include "config.hpp"
#include <vector>
#include <map>
#include <any>

namespace gloops {

	class Transform4 {

	public:

		const m4f& model() const;

		m3f rotation() const;

		v3f eulerAngles() const;
		void setEulerAngles(const v3f& angles);

		const v3f& scaling() const;
		const v3f& translation() const;

		void setTranslation(const v3f& translation);
		void setRotation(const Qf& rotation);
		void setRotation(const Eigen::AngleAxisf& aa);
		void setScaling(const v3f& scaling);
		void setScaling(float s);

		bool dirty() const;

	private:
		v3f _translation = { 0,0,0 }, _scaling = { 1,1,1 };
		Qf _rotation = Qf::Identity();
		mutable m4f _model;

		mutable bool dirtyModel = true;
	};

	class Mesh {
	public:
		using Tri = v3u;
		using Vert = v3f;

		using Triangles = std::vector<Tri>;
		using Vertices = std::vector<Vert>;
		using Normals = std::vector<v3f>;
		using Colors = std::vector<v3f>;
		using UVs = std::vector<v2f>;
		using Box = Eigen::AlignedBox<float, 3>;

		using Callback = std::function<void(void)>;

		Mesh();

		const Vertices& getVertices() const;
		const Triangles& getTriangles() const;
		const Normals& getNormals() const;
		const UVs& getUVs() const;
		const Colors& getColors() const;

		const m4f& model() const;

		virtual void setTriangles(const Triangles& tris);
		void setVertices(const Vertices& tris);
		void setUVs(const UVs& tris);
		void setNormals(const Normals& tris);
		void setColors(const Colors& tris);

		Mesh& invertFaces();

		static std::vector<Mesh> loadMeshes(const std::string& path);
		
		std::vector<Mesh> extractComponents() const;

		Mesh merge(const Mesh& other) const;

		//virtual bool load(const std::string& path);

		void computeVertexNormalsFromVertices();

		const Box& getBoundingBox() const;

		const Transform4& transform() const;

		operator bool() const;

		Mesh& setTranslation(const v3f& translation);
		Mesh& setRotation(const Qf& rotation);
		Mesh& setRotation(const Eigen::AngleAxisf& aa);
		Mesh& setRotation(const v3f& eulerAngles);

		Mesh& setScaling(const v3f& scaling);
		Mesh& setScaling(float s);
		
		Mesh& setTransform(const Transform4& t);

		static Mesh getSphere(uint precision = 50);
		static Mesh getTorus(float outerRadius, float innerRadius, uint precision = 50);
		static Mesh getCube(const Box& box = Box(v3f(-1, -1, -1), v3f(1, 1, 1)));
		static Mesh getCube(const v3f& center, const v3f& halfDiag);

		template<typename T>
		void setCPUattribute(const std::string& name, const std::vector<T>& data);

		template<typename T>
		const std::vector<T>& getAttribute(const std::string& name) const;

		//callback will be called whenever model is modified
		template<typename F>
		size_t addModelCallback( F&& f) const;

		//callback will be called whenever triangles or vertices are modified
		template<typename F>
		size_t addGeometryCallback( F&& f) const;

		void removeModelCallback(const size_t id);
		void removeGeometryCallback(const size_t id);

	protected:

		mutable Box box;
		mutable bool dirtyBox = true;

		void invalidateModel();
		void invalidateGeometry();
	
		using Callbacks = std::map<size_t, Callback>;

		mutable std::shared_ptr<Callbacks> modelCallbacks, geometryCallbacks;

		//v3f _translation = { 0,0,0 }, _scaling = { 1,1,1 };
		//Qf _rotation = Qf::Identity();
		//mutable m4f _model;

		std::shared_ptr<Transform4> _transform;

		std::shared_ptr<std::map<std::string, std::any>> custom_attributes;

		std::shared_ptr<Triangles> triangles;
		std::shared_ptr<Vertices> vertices;
		std::shared_ptr<Normals> normals;
		std::shared_ptr<Colors> colors;
		std::shared_ptr<UVs> uvs;
	};


	template<typename T>
	struct VertexAttributeInfos;

	template<>
	struct VertexAttributeInfos<float> {
		static const GLenum type = GL_FLOAT;
		static const int channels = 1;
	};

	template<typename T, int N>
	struct VertexAttributeInfos<Eigen::Matrix<T, N, 1>> {
		static const GLenum type = VertexAttributeInfos<T>::type;
		static const int channels = N;
	};

	struct VertexAttribute {

		VertexAttribute() = default;

		template<typename T>
		VertexAttribute(const std::vector<T>& v, GLuint index) :
			index(index), 
			total_num_bytes(v.size() * sizeof(T)), 
			num_channels(VertexAttributeInfos<T>::channels),
			type(VertexAttributeInfos<T>::type),
			pointer(v.data())
		{
		}

		size_t copyDataToBuffer(char* dst) const;

		size_t setupAttributePtr(char* ptr) const;

		GLuint index = 0;
		size_t total_num_bytes = 0;
		GLint num_channels = 0;
		GLenum type = GL_FLOAT;
		GLboolean normalized = GL_FALSE;
		GLsizei stride = 0;
		const GLvoid* pointer = 0;

	};

	class MeshGL : public Mesh {

	public:

		enum VertexAttributeLocationDefault : GLuint {
			PositionDefaultLocation = 0,
			NormalDefaultLocation = 2,
			ColorDefaultLocation = 3,
			UVDefaultLocation = 1,

		};

		using Ptr = std::shared_ptr<MeshGL>;

		MeshGL();
		MeshGL(const Mesh& mesh);

		virtual void setTriangles(const Triangles& tris) override;
		void setVertices(const Vertices& verts, GLuint location = PositionDefaultLocation);
		void setUVs(const UVs& uvs, GLuint location = UVDefaultLocation);
		void setNormals(const Normals& normals, GLuint location = NormalDefaultLocation);
		void setColors(const Colors& colors, GLuint location = ColorDefaultLocation);

		template<typename T>
		void setGLattribute(const std::string& name, const std::vector<T>& data, GLuint location);

		//void modifyAttributeLocation(GLuint currentLocation, GLuint newLocation);

		static std::vector<MeshGL> loadMeshes(const std::string& path);

		//virtual bool load(const std::string& path) override;

		void computeVertexNormalsFromVertices(GLuint location = NormalDefaultLocation);

		void draw() const;

		GLenum mode = GL_FILL;
		mutable GLenum primitive = GL_TRIANGLES;

		bool backface_culling = true;
		bool depth_test = true;

		const std::map<std::string, VertexAttribute>& getAttributes() const;

		static MeshGL getCubeLines(const Box& box);
		static MeshGL getAxis();
		static MeshGL fromEndPoints(const std::vector<v3f>& pts);
		static MeshGL fromPoints(const std::vector<v3f>& pts);
		static MeshGL quad(const v3f& center, const v3f& semiDiagonalA, const v3f& semiDiagonalB, const v2f& uvs_tl = { 0,0 }, const v2f uvs_br = { 1,1 });

	private:
		size_t size_of_vertex_data() const;

		void updateBuffers() const;
		void updateLocations() const;

		std::map<std::string, VertexAttribute> attributes_mapping;
		//std::shared_ptr<std::map<GLuint, std::any>> custom_attributes;

		GLptr vao, triangleBuffer, vertexBuffer;
		GLsizei numElements = 0;
		mutable bool dirtyBuffers = true, dirtyLocations = true;
	};

	template<typename T>
	inline void Mesh::setCPUattribute(const std::string& name, const std::vector<T>& data)
	{
		(*custom_attributes)[name] = std::make_any<std::vector<T>>(data);
	}

	template<typename T>
	inline const std::vector<T>& Mesh::getAttribute(const std::string& name) const
	{
		return std::any_cast<const std::vector<T>&>(custom_attributes->at(name));
	}

	template<typename F>
	inline size_t Mesh::addModelCallback(F&& f) const
	{
		static size_t id = 0;
		++id;
		modelCallbacks->emplace(id, std::forward<F>(f));
		return id;
	}

	template<typename F>
	inline size_t Mesh::addGeometryCallback(F&& f) const
	{
		static size_t id = 0;
		++id;
		geometryCallbacks->emplace(id, std::forward<F>(f));
		return id;
	}

	template<typename T>
	inline void MeshGL::setGLattribute(const std::string& name, const std::vector<T>& data, GLuint location)
	{
		Mesh::setCPUattribute(name, data);

		attributes_mapping[name] = VertexAttribute(getAttribute<T>(name), location);

		if (numElements == 0) {
			numElements = static_cast<GLsizei>(data.size());
		}

		dirtyBuffers = true;
	}


}
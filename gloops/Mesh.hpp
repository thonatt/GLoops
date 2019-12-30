#pragma once

#include "config.hpp"
#include <vector>
#include <map>
#include <any>

namespace gloops {

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

		Mesh();

		const Vertices& getVertices() const;
		const Triangles& getTriangles() const;
		const Normals& getNormals() const;
		const UVs& getUVs() const;
		const Colors& getColors() const;

		virtual void setTriangles(const Triangles& tris);
		void setVertices(const Vertices& tris);
		void setUVs(const UVs& tris);
		void setNormals(const Normals& tris);
		void setColors(const Colors& tris);

		Mesh& invertFaces();

		virtual bool load(const std::string& path);

		void computeVertexNormalsFromVertices();

		virtual const Box& getBoundingBox() const;

		operator bool() const;

		Mesh& setTranslation(const v3f& translation);
		Mesh& setRotation(const Qf& rotation);
		Mesh& setRotation(const Eigen::AngleAxisf& aa);
		Mesh& setScaling(const v3f& scaling);
		Mesh& setScaling(float s);

		const m4f& model() const;
		m3f rotation() const;
		const Qf& quat() const;
		const v3f& scaling() const;
		const v3f& translation() const;

		static Mesh getSphere(uint precision = 50);
		static Mesh getTorus(float outerRadius, float innerRadius, uint precision = 50);
		static Mesh getCube(const Box& box = Box(v3f(-1, -1, -1), v3f(1, 1, 1)));
		static Mesh getCube(const v3f& center, const v3f& halfDiag);

	protected:

		mutable Box box;
		mutable bool dirtyBox = true, dirtyModel = true;

		v3f _translation = { 0,0,0 }, _scaling = { 1,1,1 };
		Qf _rotation = Qf::Identity();
		mutable m4f _model;

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
		void setAttribute(const std::vector<T>& data, GLuint location);

		void modifyAttributeLocation(GLuint currentLocation, GLuint newLocation);

		virtual bool load(const std::string& path) override;

		void computeVertexNormalsFromVertices(GLuint location = NormalDefaultLocation);

		void draw() const;

		GLenum mode = GL_FILL;
		mutable GLenum primitive = GL_TRIANGLES;

		bool backface_culling = true;
		bool depth_test = true;

		const std::map<GLuint, VertexAttribute>& getAttributes() const;

		virtual const Box& getBoundingBox() const override;

		static MeshGL getCubeLines(const Box& box);
		static MeshGL getAxis();
		static MeshGL fromEndPoints(const std::vector<v3f>& pts);

	private:
		size_t size_of_vertex_data() const;

		void updateBuffers() const;
		void updateLocations() const;

		std::map<GLuint, VertexAttribute> attributes_mapping;
		std::shared_ptr<std::map<GLuint, std::any>> custom_attributes;

		GLptr vao, triangleBuffer, vertexBuffer;
		GLsizei numElements = 0;
		mutable bool dirtyBuffers = true, dirtyLocations = true;
	};

	template<typename T>
	inline void MeshGL::setAttribute(const std::vector<T>& data, GLuint location)
	{
		custom_attributes[location] = std::make_any<std::vector<T>>(data);

		attributes_mapping[location] = VertexAttribute(
			std::any_cast<const std::vector<T>&>(custom_attributes.at(location)), location
		);

		if (numElements == 0) {
			numElements = static_cast<GLsizei>(data.size());
		}

		dirtyBuffers = true;
	}

}
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

		const Vertices& getVertices() const;
		const Triangles& getTriangles() const;
		const Colors& getColors() const;

		void setTriangles(const Triangles& tris);
		void setVertices(const Vertices& tris);
		void setUVs(const UVs& tris);
		void setNormals(const Normals& tris);
		void setColors(const Colors& tris);

		virtual bool load(const std::string& path);

		void computeVertexNormalsFromVertices();

		const Box& getBoundingBox() const;

		operator bool() const;

	protected:

		mutable Box box;
		mutable bool dirtyBox = true;

		Triangles triangles;
		Vertices vertices;
		Normals normals;
		Colors colors;
		UVs uvs;
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

		void setTriangles(const Triangles& tris);
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

		static MeshGL getSphere(float radius = 1.0f, const v3f& center = { 0,0,0 }, uint precision = 50);
		static MeshGL getCube(const Box& box);
		static MeshGL getCubeLines(const Box& box);
		static MeshGL getAxis(const m4f& transformation = m4f::Identity());

		GLenum mode = GL_FILL;
		mutable GLenum primitive = GL_TRIANGLES;

		bool backface_culling = true;
		bool depth_test = true;

	private:
		size_t size_of_vertex_data() const;

		void updateBuffers() const;
		void updateLocations() const;

		std::map<GLuint, VertexAttribute> attributes_mapping;
		std::map<GLuint, std::any> custom_attributes;

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
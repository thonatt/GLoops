#include "Mesh.hpp"
#include "Debug.hpp"

#include <cstring>
#include <cmath>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace gloops {

	const Mesh::Vertices& Mesh::getVertices() const
	{
		return vertices;
	}

	const Mesh::Triangles& Mesh::getTriangles() const
	{
		return triangles;
	}

	const Mesh::Colors& Mesh::getColors() const
	{
		return colors;
	}

	void Mesh::setTriangles(const Triangles& tris)
	{
		triangles = tris;
		dirtyBox = true;
	}

	void Mesh::setVertices(const Vertices& verts)
	{
		vertices = verts;
		dirtyBox = true;
	}

	void Mesh::setUVs(const UVs& texCoords)
	{
		uvs = texCoords;
	}

	void Mesh::setNormals(const Normals& norms)
	{
		normals = norms;
	}

	void Mesh::setColors(const Colors& cols)
	{
		colors = cols;
	}

	MeshGL::MeshGL()
	{
		vao = GLptr(
			[](GLuint* ptr) { glGenVertexArrays(1, ptr); },
			[](const GLuint* ptr) { glDeleteVertexArrays(1, ptr); }
		);

		triangleBuffer = GLptr(
			[](GLuint* ptr) { glGenBuffers(1, ptr); },
			[](const GLuint* ptr) { glDeleteBuffers(1, ptr); }
		);

		vertexBuffer = GLptr(
			[](GLuint* ptr) { glGenBuffers(1, ptr); },
			[](const GLuint* ptr) { glDeleteBuffers(1, ptr); }
		);

	}

	void MeshGL::setTriangles(const Triangles& tris)
	{
		Mesh::setTriangles(tris);
		dirtyBuffers = true;
	}

	void MeshGL::setVertices(const Vertices& verts, GLuint location)
	{
		Mesh::setVertices(verts);
		dirtyBuffers = true;
		setAttribute(vertices, location);
		//setAttributeLocation(Position, location);
	}

	void MeshGL::setNormals(const Normals& norms, GLuint location)
	{
		Mesh::setNormals(norms);
		dirtyBuffers = true;
		setAttribute(normals, location);
		//setAttributeLocation(Normal, location);		
	}

	void MeshGL::setColors(const Colors& cols, GLuint location)
	{
		Mesh::setColors(cols);
		dirtyBuffers = true;
		setAttribute(colors, location);
		//setAttributeLocation(Color, location);
	}

	void MeshGL::setUVs(const UVs& texCoords, GLuint location)
	{
		Mesh::setUVs(texCoords);
		dirtyBuffers = true;
		setAttribute(uvs, location);
		//setAttributeLocation(UV, location);
	}

	void MeshGL::modifyAttributeLocation(GLuint currentLocation, GLuint newLocation)
	{
		auto attribute = attributes_mapping.find(currentLocation);
		if (attribute != attributes_mapping.end()) {
			attributes_mapping[currentLocation].index = newLocation;
			dirtyLocations = true;
		}
	}

	bool MeshGL::load(const std::string& path)
	{
		bool out = Mesh::load(path);
		if (out) {
			setVertices(vertices);
		}
		return out;
	}

	void MeshGL::computeVertexNormalsFromVertices(GLuint location)
	{
		Mesh::computeVertexNormalsFromVertices();
		setNormals(normals, location);
	}

	void MeshGL::draw() const
	{
		if (dirtyBuffers) {
			updateBuffers();
		}

		if (dirtyLocations) {
			updateLocations();
		}

		glPolygonMode(GL_FRONT_AND_BACK, mode);

		if (mode == GL_FILL && backface_culling) {
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}
		if (depth_test) {
			glEnable(GL_DEPTH_TEST);
		}

		glBindVertexArray(vao);

		if (mode == GL_POINT) {
			primitive = GL_POINTS;
		}

		switch (primitive)
		{
		case GL_TRIANGLES: {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleBuffer);
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(3 * triangles.size()), GL_UNSIGNED_INT, 0);
			break;
		}
		case GL_POINTS: {
			glDrawArrays(GL_POINTS, 0, numElements);
			break;
		}
		default:
			break;
		}

		glBindVertexArray(0);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		if (depth_test) {
			glDisable(GL_DEPTH_TEST);
		}
		if (mode == GL_FILL && backface_culling) {
			glDisable(GL_CULL_FACE);
		}
	
		//glUseProgram(0);
	}

	void MeshGL::updateBuffers() const
	{
		glBindVertexArray(vao);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(v3u) * triangles.size(), triangles.data(), GL_STATIC_DRAW);

		std::vector<char> vertexData(size_of_vertex_data());

		char* dst = vertexData.data();
		for (const auto& attribute : attributes_mapping) {
			dst += attribute.second.copyDataToBuffer(dst);
		}

		gl_check();

		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, vertexData.size(), vertexData.data(), GL_STATIC_DRAW);

		updateLocations();

		glBindVertexArray(0);

		dirtyBuffers = false;
	}

	void MeshGL::updateLocations() const
	{

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

		char* ptr = 0;
		for (const auto& attribute : attributes_mapping) {
			ptr += attribute.second.setupAttributePtr(ptr);
		}

		glBindVertexArray(0);

		dirtyLocations = false;

	}

	bool Mesh::load(const std::string& path)
	{
		using namespace Assimp;
		Importer importer;

		const aiScene* scene;

		try {
			scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
		}
		catch (const std::exception & e) {
			std::cout << e.what() << std::endl;
			return false;
		}

		if (!scene) {
			std::cout << " cant load " << path << std::endl;
			return false;
		}
		if (!scene->mNumMeshes) {
			std::cout << " no meshes at " << path << std::endl;
			return false;
		}

		//aiMatrix4x4 t = scene->mRootNode->mTransformation;

		//auto coutT = [](const aiMatrix4x4& t) {
		//	std::cout <<
		//		t.a1 << " " << t.a2 << " " << t.a3 << " " << t.a4 << " " << std::endl <<
		//		t.b1 << " " << t.b2 << " " << t.b3 << " " << t.b4 << " " << std::endl <<
		//		t.c1 << " " << t.c2 << " " << t.c3 << " " << t.c4 << " " << std::endl <<
		//		t.d1 << " " << t.d2 << " " << t.d3 << " " << t.d4 << " " << std::endl;
		//};

		//coutT(t);
			

		auto vConverter = [](const aiVector3D& v) { return v3f(v.x, v.y, v.z); };
		auto tConverter = [](const aiFace& f) { return v3u(f.mIndices[0], f.mIndices[1], f.mIndices[2]); };
		auto uvConverter = [](const aiVector3D& v) { return v2f(v.x, v.y); };

		size_t num_vertices = 0, num_triangles = 0;

		//std::cout << scene->mNumMeshes << " meshes" << std::endl;
		for (size_t m_id = 0; m_id < scene->mNumMeshes; ++m_id) {
			num_vertices += scene->mMeshes[m_id]->mNumVertices;
			num_triangles += scene->mMeshes[m_id]->mNumFaces;
		}
		//std::cout << num_vertices << " verts, " << num_triangles << " tris loaded" << std::endl;

		vertices.resize(num_vertices);
		triangles.resize(num_triangles);
		uvs.resize(0);

		size_t offset_t = 0, offset_v = 0;
		for (size_t m_id = 0; m_id < scene->mNumMeshes; ++m_id) {

			const aiMesh& mesh = *scene->mMeshes[m_id];

			const bool hasTexCoords = mesh.GetNumUVChannels() > 0;

			if (hasTexCoords) {
				uvs.resize(uvs.size() + mesh.mNumVertices);
			}

			for (size_t v_id = 0; v_id < mesh.mNumVertices; ++v_id) {
				size_t global_v_id = offset_v + v_id;
				vertices[global_v_id] = vConverter(mesh.mVertices[v_id]);

				if (hasTexCoords) {
					uvs[global_v_id] = uvConverter(mesh.mTextureCoords[0][v_id]);
				}
			
			}

			offset_v += mesh.mNumVertices;

			const uint offset_tu = static_cast<uint>(offset_t);
			const v3u offset_t3 =  v3u(offset_tu, offset_tu, offset_tu);

			for (size_t t_id = 0; t_id < mesh.mNumFaces; ++t_id) {
				if (mesh.mFaces[t_id].mNumIndices != 3) {
					std::cout << "wrong num indices : " << t_id << std::endl;
				}
				triangles[offset_t + t_id] = offset_t3 + tConverter(mesh.mFaces[t_id]);
				//std::cout << triangles[offset_t + t_id].transpose() << "\n";
			}
			offset_t += mesh.mNumFaces;
		}

		std::cout << path << " : " << triangles.size() << " tris, " << vertices.size() << " verts loaded" << std::endl;

		return true;
	}

	void Mesh::computeVertexNormalsFromVertices()
	{
		struct Vdata {
			v3f sum_normals = { 0,0,0 };
			float sum_w = 0;
		};

		std::vector<Vdata> vData(vertices.size());

		for (const auto& tri : triangles) {
			const Vert& a = vertices[tri[0]], b = vertices[tri[1]], c = vertices[tri[2]];
			v3f tri_normal = (b - a).cross(c - a);
			for (uint i : {tri[0], tri[1], tri[2]}) {
				vData[i].sum_w += tri_normal.norm();
				vData[i].sum_normals += tri_normal;
			}
		}

		std::vector<v3f> newNormals(vertices.size());
		for (uint i = 0; i < vertices.size(); ++i) {
			newNormals[i] = (vData[i].sum_normals / vData[i].sum_w).normalized();
			if (newNormals[i].array().isNaN().any()) {
				std::cout << " normals contains NaN" << std::endl;
				return;
			}
		}

		setNormals(newNormals);
	}

	const Mesh::Box& Mesh::getBoundingBox() const
	{
		if(dirtyBox){
			box.setEmpty();
			for (const auto& v : vertices) {
				box.extend(v);
			}
			dirtyBox = false;
		}
		return box;
	}

	Mesh::operator bool() const
	{
		return triangles.size() > 0 && vertices.size() > 0;
	}

	const std::map<GLuint, VertexAttribute>& MeshGL::getAttributes() const
	{
		return attributes_mapping;
	}

	size_t MeshGL::size_of_vertex_data() const
	{
		size_t out = 0;
		for (const auto& attribute : attributes_mapping) {
			out += attribute.second.total_num_bytes;
		}
		return out;
	}


	MeshGL MeshGL::getSphere(float radius, const v3f& center, uint precision)
	{
		precision = std::max(precision, 2u);

		Vertices vertices(static_cast<size_t>(precision) * precision);
		Normals normals(vertices.size());
		Triangles triangles(2u * static_cast<size_t>(precision - 1) * precision);
		UVs uvs(vertices.size());

		double frac_p = 1 / (double)precision;
		double frac_t = 1 / ((double)precision - 1.0);
		for (size_t t = 0; t < precision; ++t) {
			double theta = t * frac_t * pi();
			double cost = std::cos(theta), sint = std::sin(theta);
			for (size_t p = 0; p < precision; ++p) {
				double phi = p * frac_p * 2 * pi();
				double cosp = std::cos(phi), sinp = std::sin(phi);
				normals[p + precision * t] = v3d(sint * cosp, sint * sinp, cost).template cast<float>();
				vertices[p + precision * t] = (radius * normals[p + precision * t] + center);
				uvs[p + precision * t] = v2f(frac_p, frac_t);
			}
		}

		uint tri_id = 0;
		for (uint t = 0; t < precision - 1; ++t) {
			for (uint p = 0; p < precision; ++p, tri_id += 2) {
				uint current_id = p + precision * t;
				uint offset_row = 1 - (p == precision - 1 ? precision : 0);
				uint next_in_row = current_id + offset_row;
				uint next_in_col = current_id + precision;
				uint next_next = next_in_col + offset_row;
				triangles[tri_id] = { current_id, next_in_col, next_in_row };
				triangles[tri_id + 1] = { next_in_row, next_in_col, next_next };
			}
		}

		MeshGL mesh;
		mesh.setVertices(vertices);
		mesh.setTriangles(triangles);
		mesh.setNormals(normals);
		mesh.setUVs(uvs);
		return mesh;
	}

	MeshGL MeshGL::getCube(const Box& box)
	{
		static const Mesh::Triangles tris = {
			{ 0, 3, 1 }, { 0, 2, 3 }, 
			{ 4, 5, 7 }, {7, 6, 4 },
			{ 8, 11, 9 }, { 11, 8, 10 },
			{ 12, 13, 15 }, { 12, 15, 14 },
			{ 16, 19, 17 }, { 19, 16, 18 },
			{ 20, 21, 23 }, { 20, 23, 22 }
		};

		static const std::vector<std::vector<int>> corners = {
			//top bottom left right floor ceil
			{ 2, 3, 6, 7 },
			{ 0, 1, 4, 5 },
			{ 0, 2, 4, 6 },
			{ 1, 3, 5, 7 },
			{ 0, 1, 2, 3 },
			{ 4, 5, 6, 7 }
		};

		Mesh::Vertices vertices(4 * 6);
		for (int f = 0; f < 6; ++f) {
			for (int v = 0; v < 4; ++v) {
				vertices[4 * f + v] = box.corner((Box::CornerType)corners[f][v]);
			}
		}
		
		MeshGL out;
		out.setTriangles(tris);
		out.setVertices(vertices);
		out.computeVertexNormalsFromVertices();

		return out;
	}

	MeshGL MeshGL::getCubeLines(const Box& box)
	{
		static const Mesh::Triangles tris = {
			{ 0,4, 4 }, { 5,1, 1 }, { 4,5, 5 }, { 0,1, 1 },
			{ 2,6, 6 }, { 7,3, 3 }, { 6,7, 7 }, { 2,3, 3 },
			{ 0,2, 2 }, { 1,3, 3 }, { 4,6, 6 }, { 5,7, 7 }
		};

		Mesh::Vertices vertices(8);
		for (int i = 0; i < 8; ++i) {
			vertices[i] = box.corner((Box::CornerType)i);
		}

		MeshGL out;
		out.setTriangles(tris);
		out.setVertices(vertices);
		out.mode = GL_LINE;
		return out;
	}
	
	MeshGL MeshGL::getAxis(const m4f& transformation)
	{
		static const Mesh::Triangles tris = {
			{ 0,0,1 }, { 0,0,2 }, { 0,0,3 }
		};

		static const Mesh::Colors cols = {
			v3f::Ones(), v3f::UnitX(), v3f::UnitY(), v3f::UnitZ()
		};

		static const Mesh::Vertices verts = {
			v3f::Zero(), v3f::UnitX(), v3f::UnitY(), v3f::UnitZ()
		};

		Mesh::Vertices vs(verts.size());
		for (uint i = 0; i < verts.size(); ++i) {
			vs[i] = applyTransformation(transformation, verts[i]);
		}

		MeshGL out;
		out.setTriangles(tris);
		out.setVertices(vs);
		out.setColors(cols);
		out.mode = GL_LINE;

		return out;
	}

	size_t VertexAttribute::copyDataToBuffer(char* dst) const
	{
		std::memcpy(dst, pointer, total_num_bytes);
		return total_num_bytes;
	}

	size_t VertexAttribute::setupAttributePtr(char* ptr) const
	{
		glVertexAttribPointer(index, num_channels, type, normalized, stride, ptr);
		glEnableVertexAttribArray(index);
		return total_num_bytes;
	}

}
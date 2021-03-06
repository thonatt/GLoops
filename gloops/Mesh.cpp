#include "Mesh.hpp"
#include "Debug.hpp"
#include "Utils.hpp"

#include <cstring>
#include <cmath>
#include <queue>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

//#include <assimp/Importer.hpp>
//#include <assimp/scene.h>
//#include <assimp/postprocess.h>

namespace gloops {

	const m4f& Transform4::model() const
	{
		if (dirtyModel) {
			_model = transformationMatrix(_translation, rotation() , _scaling);
			dirtyModel = false;
		}
		return _model;
	}

	m3f Transform4::rotation() const
	{
		return _rotation.toRotationMatrix();
	}

	v3f Transform4::eulerAngles() const
	{
		return rotation().eulerAngles(0, 1, 2);
	}

	void Transform4::setEulerAngles(const v3f& angles)
	{
		_rotation =
			Eigen::AngleAxisf(angles[0], v3f::UnitX()) *
			Eigen::AngleAxisf(angles[1], v3f::UnitY()) *
			Eigen::AngleAxisf(angles[2], v3f::UnitZ());
		dirtyModel = true;
	}

	const v3f& Transform4::scaling() const
	{
		return _scaling;
	}

	const v3f& Transform4::translation() const
	{
		return _translation;
	}

	void Transform4::setTranslation(const v3f& translation)
	{
		_translation = translation;
		dirtyModel = true;
	}

	void Transform4::setRotation(const Qf& rotation)
	{
		_rotation = rotation;
		dirtyModel = true;
	}

	void Transform4::setRotation(const Eigen::AngleAxisf& aa)
	{
		_rotation = aa;
		dirtyModel = true;
	}

	void Transform4::setScaling(const v3f& scaling)
	{
		_scaling = scaling;
		dirtyModel = true;
	}

	void Transform4::setScaling(float s)
	{
		setScaling(v3f(s, s, s));
	}

	bool Transform4::dirty() const
	{
		return dirtyModel;
	}

	Mesh::Mesh()
	{
		triangles = std::make_shared<Triangles>();
		vertices = std::make_shared<Vertices>();
		normals = std::make_shared<Normals>();
		colors = std::make_shared<Colors>();
		uvs = std::make_shared<UVs>();

		_transform = std::make_shared<Transform4>();
		modelCallbacks = std::make_shared<Callbacks>();
		geometryCallbacks = std::make_shared<Callbacks>();
	}

	const Mesh::Vertices& Mesh::getVertices() const
	{
		return *vertices;
	}

	const Mesh::Triangles& Mesh::getTriangles() const
	{
		return *triangles;
	}

	const Mesh::Normals& Mesh::getNormals() const
	{
		return *normals;
	}

	const Mesh::UVs& Mesh::getUVs() const
	{
		return *uvs;
	}

	const Mesh::Colors& Mesh::getColors() const
	{
		return *colors;
	}

	const m4f& Mesh::model() const
	{
		return _transform->model();
	}

	void Mesh::setTriangles(const Triangles& tris)
	{
		*triangles = tris;
		invalidateGeometry();
	}

	void Mesh::setVertices(const Vertices& verts)
	{
		*vertices = verts;
		invalidateGeometry();
	}

	void Mesh::setUVs(const UVs& texCoords)
	{
		*uvs = texCoords;
	}

	void Mesh::setNormals(const Normals& norms)
	{
		*normals = norms;
	}

	void Mesh::setColors(const Colors& cols)
	{
		*colors = cols;
	}

	Mesh& Mesh::invertFaces()
	{
		for (Tri& tri : *triangles) {
			tri = v3u(tri[0], tri[2], tri[1]);
		}
		for (v3f& normal : *normals) {
			normal = -normal;
		}
		return *this;
	}

	MeshGL::MeshGL() : Mesh()
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

		custom_attributes = std::make_shared< std::map<std::string, std::any>>();
	}

	MeshGL::MeshGL(const Mesh& mesh) : MeshGL()
	{
		setTriangles(mesh.getTriangles());
		setVertices(mesh.getVertices());
		
		if (mesh.getUVs().size() > 0) {
			setUVs(mesh.getUVs());
		}
		if (mesh.getNormals().size() > 0) {
			setNormals(mesh.getNormals());
		}
		if (mesh.getColors().size() > 0) {
			setColors(mesh.getColors());
		}
	
		*_transform = mesh.transform();

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
		attributes_mapping["positions"] = VertexAttribute(getVertices(), location);
		if (numElements == 0) {
			numElements = static_cast<GLsizei>(getVertices().size());
		}
	}

	void MeshGL::setNormals(const Normals& norms, GLuint location)
	{
		Mesh::setNormals(norms);
		dirtyBuffers = true;
		attributes_mapping["normals"] = VertexAttribute(getNormals(), location);
	}

	void MeshGL::setColors(const Colors& cols, GLuint location)
	{
		Mesh::setColors(cols);
		dirtyBuffers = true;
		attributes_mapping["colors"] = VertexAttribute(getColors(), location);
	}

	void MeshGL::setUVs(const UVs& texCoords, GLuint location)
	{
		Mesh::setUVs(texCoords);
		dirtyBuffers = true;
		attributes_mapping["uvs"] = VertexAttribute(getUVs(), location);
	}

	//void MeshGL::modifyAttributeLocation(GLuint currentLocation, GLuint newLocation)
	//{
	//	auto attribute = attributes_mapping.find(currentLocation);
	//	if (attribute != attributes_mapping.end()) {
	//		attributes_mapping[currentLocation].index = newLocation;
	//		dirtyLocations = true;
	//	}
	//}

	//bool MeshGL::load(const std::string& path)
	//{
	//	bool out = Mesh::load(path);
	//	if (out) {
	//		setVertices(getVertices());
	//	}
	//	return out;
	//}

	std::vector<MeshGL> MeshGL::loadMeshes(const std::string& path)
	{
		std::vector<Mesh> meshes = Mesh::loadMeshes(path);

		std::vector<MeshGL> out;
		for (const auto& mesh : meshes) {
			out.push_back(MeshGL(mesh));
		}

		return out;
	}

	void MeshGL::computeVertexNormalsFromVertices(GLuint location)
	{
		Mesh::computeVertexNormalsFromVertices();
		setNormals(getNormals(), location);
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
	
		//if (mode == GL_POINT) {
		//	primitive = GL_POINTS;
		//} else {
		//	primitive = GL_TRIANGLES;
		//}

		glBindVertexArray(vao);

		switch (primitive)
		{
		case GL_PATCHES: {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleBuffer);
			glDrawElements(GL_PATCHES, static_cast<GLsizei>(3 * getTriangles().size()), GL_UNSIGNED_INT, 0);
			break;
		}
		case GL_TRIANGLES: {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleBuffer);
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(3 * getTriangles().size()), GL_UNSIGNED_INT, 0);
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
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(v3u) * getTriangles().size(), getTriangles().data(), GL_STATIC_DRAW);

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

	std::vector<Mesh> Mesh::loadMeshes(const std::string& path)
	{
		auto assign3f = [](float& dst, const float& src) {
			std::memcpy(&dst, &src, 3 * sizeof(float));
		};

		auto assign2f = [](float& dst, const float& src) {
			std::memcpy(&dst, &src, 2 * sizeof(float));
		};

		struct IndexSort {
			bool operator()(const tinyobj::index_t& a, const tinyobj::index_t& b) const {
				return a.vertex_index < b.vertex_index;
			}
		};

		tinyobj::ObjReader reader;
		tinyobj::ObjReaderConfig config;
		config.triangulate = true;
		config.vertex_color = false;
	
		std::cout << "loading " << path << std::flush;
		if (!reader.ParseFromFile(path, config)) {
			addToLogs(LogType::ERROR, reader.Error());
		}
		if (!reader.Warning().empty()) {
			addToLogs(LogType::WARNING, reader.Warning());
		}

		std::vector<Mesh> out;
		
		const auto& attributes = reader.GetAttrib();
		const auto& shapes = reader.GetShapes();

		const bool hasColor = !attributes.colors.empty(),
			hasNormals = !attributes.normals.empty(),
			hasTexCoords = !attributes.texcoords.empty();

		for (const auto& shape : shapes) {
			
			Mesh mesh;

			const size_t numIndices = shape.mesh.indices.size();

			std::map<tinyobj::index_t, int, IndexSort> tinyobj_to_unique = {};

			Mesh::Triangles triangles(numIndices / 3);
			for (size_t i = 0; i < numIndices; ++i) {
				
				const tinyobj::index_t& v_index = shape.mesh.indices[i];

				if (tinyobj_to_unique.find(v_index) == tinyobj_to_unique.end()) {
					tinyobj_to_unique[v_index] = (int)tinyobj_to_unique.size();
				}

				triangles[i / 3][i % 3] = tinyobj_to_unique[v_index];

			}
			mesh.setTriangles(triangles);
		
			const size_t numVertices = tinyobj_to_unique.size();

			std::cout << ", " << numVertices << " vertices " << std::flush;

			std::vector<v3f> vertices(numVertices);
			for (const auto& indice : tinyobj_to_unique) {
				assign3f(vertices[indice.second][0], attributes.vertices[3*indice.first.vertex_index]);
			}
			mesh.setVertices(vertices);

			if (hasColor) {
				std::cout << ", colors" << std::flush;
				std::vector<v3f> colors(numVertices);
				for (const auto& indice : tinyobj_to_unique) {
					assign3f(colors[indice.second][0], attributes.colors[3*indice.first.vertex_index]);
				}
				mesh.setColors(colors);
			}

			if (hasNormals) {
				std::cout << ", normals" << std::flush;
				std::vector<v3f> normals(numVertices);
				for (const auto& indice : tinyobj_to_unique) {
					assign3f(normals[indice.second][0], attributes.normals[3 * indice.first.normal_index]);
				}
			
				mesh.setNormals(normals);
			}

			if (hasTexCoords) {
				std::cout << ", texCoords" << std::flush;
				std::vector<v2f> texCoords(numVertices);
				for (const auto& indice : tinyobj_to_unique) {
					assign2f(texCoords[indice.second][0], attributes.texcoords[2*indice.first.texcoord_index]);
				}
				mesh.setUVs(texCoords);
			}

			std::cout << std::endl;

			out.push_back(std::move(mesh));
		}

		return out;
	}

	std::vector<Mesh> Mesh::extractComponents() const
	{
		std::vector<Mesh> out;

		if (getTriangles().empty() || getVertices().empty()) {
			return out;
		}

		std::map<size_t, std::vector<size_t>> triangles_map;

		for (size_t t_id = 0; t_id < getTriangles().size(); ++t_id) {
			for (int i = 0; i < 3; ++i) {
				triangles_map[getTriangles()[t_id][i]].push_back(t_id);
			}
		}

		std::vector<int> v_visited(getVertices().size(), false), t_visited(getTriangles().size(), false);
		std::queue<size_t> queue;
		for (size_t v_id = 0; v_id < getVertices().size(); ++v_id) {
			if (v_visited[v_id]) {
				continue;
			}
			queue.push(v_id);
			v_visited[v_id] = true;

			std::vector<size_t> currentVertices, currentTriangles;

			while (!queue.empty()) {
				const size_t current_v_id = queue.front();
				queue.pop();	
				currentVertices.push_back(current_v_id);

				for (const auto& tri_id : triangles_map[current_v_id]) {
					if (!t_visited[tri_id]) {
						currentTriangles.push_back(tri_id);
						t_visited[tri_id] = true;
					}

					for (int i = 0; i < 3; ++i) {
						const size_t next_v_id = getTriangles()[tri_id][i];
						if (!v_visited[next_v_id]) {
							queue.push(next_v_id);
							v_visited[next_v_id] = true;
						}
					}
				}
			}

			Mesh mesh;

			std::vector<size_t> vs_mapping(getVertices().size());

			Vertices vs(currentVertices.size());
			for (size_t new_v_id = 0; new_v_id < currentVertices.size(); ++new_v_id) {
				vs[new_v_id] = getVertices()[currentVertices[new_v_id]];
				vs_mapping[currentVertices[new_v_id]] = new_v_id;
			}
			mesh.setVertices(vs);

			Triangles ts(currentTriangles.size());
			for (size_t new_t_id = 0; new_t_id < currentTriangles.size(); ++new_t_id) {
				const auto& prev_triangle = getTriangles()[currentTriangles[new_t_id]];
				for (int i = 0; i < 3; ++i) {
					ts[new_t_id][i] = (GLuint)vs_mapping[prev_triangle[i]];
				}
			}
			mesh.setTriangles(ts);
			
			mesh.setTransform(transform());

			if (!getNormals().empty()) {
				Normals ns(currentVertices.size());
				for (size_t new_v_id = 0; new_v_id < currentVertices.size(); ++new_v_id) {
					ns[new_v_id] = getNormals()[currentVertices[new_v_id]];
				}
				mesh.setNormals(ns);
			}

			if (!getUVs().empty()) {
				UVs uvs(currentVertices.size());
				for (size_t new_v_id = 0; new_v_id < currentVertices.size(); ++new_v_id) {
					uvs[new_v_id] = getUVs()[currentVertices[new_v_id]];
				}
				mesh.setUVs(uvs);
			}

			if (!getColors().empty()) {
				Colors cs(currentVertices.size());
				for (size_t new_v_id = 0; new_v_id < currentVertices.size(); ++new_v_id) {
					cs[new_v_id] = getColors()[currentVertices[new_v_id]];
				}
				mesh.setColors(cs);
			}

			out.push_back(mesh);
		}

		return out;
	}

	Mesh Mesh::merge(const Mesh& other) const
	{
		if (getTriangles().size() < other.getTriangles().size()) {
			return other.merge(*this);
		}

		Mesh out;

		const size_t v_size = getVertices().size(), t_size = getTriangles().size();

		Vertices vs(getVertices());
		vs.insert(vs.end(), other.getVertices().begin(), other.getVertices().end());
		out.setVertices(vs);

		Triangles ts(getTriangles());
		ts.insert(ts.end(), other.getTriangles().begin(), other.getTriangles().end());

		const v3u offset((GLuint)v_size, (GLuint)v_size, (GLuint)v_size);
		for (size_t v_id = 0; v_id < other.getTriangles().size(); ++v_id) {
			ts[t_size + v_id] += offset;
		}
		out.setTriangles(ts);

		out.setTransform(transform());

		if (!getNormals().empty() && !other.getNormals().empty()) {
			Normals ns(getNormals());
			ns.insert(ns.end(), other.getNormals().begin(), other.getNormals().end());
			out.setNormals(ns);
		}

		if (!getUVs().empty() && !other.getUVs().empty()) {
			UVs uvs(getUVs());
			uvs.insert(uvs.end(), other.getUVs().begin(), other.getUVs().end());
			out.setUVs(uvs);
		}

		if (!getColors().empty() && !other.getColors().empty()) {
			Colors cs(getColors());
			cs.insert(cs.end(), other.getColors().begin(), other.getColors().end());
			out.setColors(cs);
		}

		return out;
	}

	//bool Mesh::load(const std::string& path)
	//{
	//	using namespace Assimp;
	//	Importer importer;

	//	const aiScene* scene;

	//	try {
	//		scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
	//	}
	//	catch (const std::exception & e) {
	//		std::cout << e.what() << std::endl;
	//		return false;
	//	}

	//	if (!scene) {
	//		std::cout << " cant load " << path << std::endl;
	//		return false;
	//	}
	//	if (!scene->mNumMeshes) {
	//		std::cout << " no meshes at " << path << std::endl;
	//		return false;
	//	}

	//	auto vConverter = [](const aiVector3D& v) { return v3f(v.x, v.y, v.z); };
	//	auto tConverter = [](const aiFace& f) { return v3u(f.mIndices[0], f.mIndices[1], f.mIndices[2]); };
	//	auto uvConverter = [](const aiVector3D& v) { return v2f(v.x, v.y); };

	//	size_t num_vertices = 0, num_triangles = 0;

	//	//std::cout << scene->mNumMeshes << " meshes" << std::endl;
	//	for (size_t m_id = 0; m_id < scene->mNumMeshes; ++m_id) {
	//		num_vertices += scene->mMeshes[m_id]->mNumVertices;
	//		num_triangles += scene->mMeshes[m_id]->mNumFaces;
	//	}
	//	//std::cout << num_vertices << " verts, " << num_triangles << " tris loaded" << std::endl;

	//	auto& vs = *vertices;
	//	auto& ts = *triangles;
	//	auto& tcs = *uvs;

	//	vs.resize(num_vertices);
	//	ts.resize(num_triangles);
	//	tcs.resize(0);

	//	size_t offset_t = 0, offset_v = 0;
	//	for (size_t m_id = 0; m_id < scene->mNumMeshes; ++m_id) {

	//		const aiMesh& mesh = *scene->mMeshes[m_id];

	//		const bool hasTexCoords = mesh.GetNumUVChannels() > 0;

	//		if (hasTexCoords) {
	//			tcs.resize(tcs.size() + mesh.mNumVertices);
	//		}

	//		for (size_t v_id = 0; v_id < mesh.mNumVertices; ++v_id) {
	//			size_t global_v_id = offset_v + v_id;
	//			vs[global_v_id] = vConverter(mesh.mVertices[v_id]);

	//			if (hasTexCoords) {
	//				tcs[global_v_id] = uvConverter(mesh.mTextureCoords[0][v_id]);
	//			}
	//		
	//		}

	//		offset_v += mesh.mNumVertices;

	//		const uint offset_tu = static_cast<uint>(offset_t);
	//		const v3u offset_t3 =  v3u(offset_tu, offset_tu, offset_tu);

	//		for (size_t t_id = 0; t_id < mesh.mNumFaces; ++t_id) {
	//			if (mesh.mFaces[t_id].mNumIndices != 3) {
	//				std::cout << "wrong num indices : " << t_id << std::endl;
	//			}
	//			ts[offset_t + t_id] = offset_t3 + tConverter(mesh.mFaces[t_id]);
	//			//std::cout << triangles[offset_t + t_id].transpose() << "\n";
	//		}
	//		offset_t += mesh.mNumFaces;
	//	}

	//	std::cout << path << " : " << ts.size() << " tris, " << vs.size() << " verts loaded" << std::endl;

	//	return true;
	//}

	void Mesh::computeVertexNormalsFromVertices()
	{
		struct Vdata {
			v3f sum_normals = { 0,0,0 };
			float sum_w = 0;
		};

		const auto& vs = getVertices();
		std::vector<Vdata> vData(vs.size());

		for (const auto& tri : getTriangles()) {
			const Vert& a = vs[tri[0]], b = vs[tri[1]], c = vs[tri[2]];
			v3f tri_normal = (b - a).cross(c - a);
			for (uint i : {tri[0], tri[1], tri[2]}) {
				vData[i].sum_w += tri_normal.norm();
				vData[i].sum_normals += tri_normal;
			}
		}

		std::vector<v3f> newNormals(vs.size());
		for (uint i = 0; i < vs.size(); ++i) {
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
		if(dirtyBox || _transform->dirty()){
			box.setEmpty();
			for (const auto& v : getVertices()) {
				box.extend(applyTransformationMatrix(model(), v));
			}
			dirtyBox = false;
		}
		return box;
	}

	const Transform4& Mesh::transform() const
	{
		return *_transform;
	}

	Mesh::operator bool() const
	{
		return getVertices().size() > 0;
	}

	const std::map<std::string, VertexAttribute>& MeshGL::getAttributes() const
	{
		return attributes_mapping;
	}

	Mesh& Mesh::setTranslation(const v3f& translation)
	{
		_transform->setTranslation(translation);
		invalidateModel();
		return *this;
	}

	Mesh& Mesh::setRotation(const Qf& rotation)
	{
		_transform->setRotation(rotation);
		invalidateModel();
		return *this;
	}

	Mesh& Mesh::setRotation(const Eigen::AngleAxisf& aa)
	{
		_transform->setRotation(aa);
		invalidateModel();
		return *this;
	}

	Mesh& Mesh::setRotation(const v3f& eulerAngles)
	{
		_transform->setEulerAngles(eulerAngles);
		invalidateModel();
		return *this;
	}

	Mesh& Mesh::setScaling(const v3f& scaling)
	{
		_transform->setScaling(scaling);
		invalidateModel();
		return *this;
	}

	Mesh& Mesh::setScaling(float s)
	{
		return setScaling(v3f(s, s, s));
	}

	Mesh& Mesh::setTransform(const Transform4& t)
	{
		*_transform = t;
		invalidateModel();
		return *this;
	}

	size_t MeshGL::size_of_vertex_data() const
	{
		size_t out = 0;
		for (const auto& attribute : attributes_mapping) {
			out += attribute.second.total_num_bytes;
		}
		return out;
	}


	Mesh Mesh::getSphere(uint _precision)
	{
		const size_t precision = std::max(_precision, 2u);
		
		Vertices vertices((precision + 1) * precision);
		Normals normals(vertices.size());
		UVs uvs(vertices.size());

		Triangles triangles(2 * precision * (precision - 1));
		
		const float frac_p = 1.0f / (float)precision;
		const float frac_t = 1.0f / ((float)precision - 1.0f);;

		for (size_t t = 0; t < precision; ++t) {
			const float theta = t * frac_t * pi<float>();
			const float cost = std::cos(theta), sint = std::sin(theta);

			for (size_t p = 0; p < precision + 1; ++p) {
				const float phi = p * frac_p * 2 * pi<float>();
				const float cosp = std::cos(phi), sinp = std::sin(phi);
				vertices[p + (precision + 1) * t] = v3f(sint * cosp, sint * sinp, cost);
				normals[p + (precision + 1) * t] = vertices[p + (precision + 1) * t];
				uvs[p + (precision + 1) * t] = v2f(t * frac_t, p * frac_p);
			}
		}

		size_t tri_id = 0;
		for (size_t t = 0; t < precision - 1; ++t) {
			for (size_t p = 0; p < precision; ++p, tri_id += 2) {
				size_t current_id = p + (precision + 1) * t;
				size_t next_in_row = current_id + 1;
				size_t next_in_col = current_id + precision + 1;
				size_t next_next = next_in_col + 1;
				triangles[tri_id] = v3u((uint)current_id, (uint)next_in_col, (uint)next_in_row);
				triangles[tri_id + 1] = v3u((uint)next_in_row, (uint)next_in_col, (uint)next_next);
			}
		}

		Mesh mesh;
		mesh.setVertices(vertices);
		mesh.setTriangles(triangles);
		mesh.setNormals(normals);
		mesh.setUVs(uvs);
		return mesh;
	}

	Mesh Mesh::getTorus(float R, float r, uint _precision)
	{
		const size_t precision = std::max(_precision, 2u);

		Vertices vertices((precision + 1) * precision);
		Normals normals(vertices.size());
		UVs uvs(vertices.size());
		Triangles triangles(2 * precision  * (precision - 1));
	
		const float frac = 1.0f / (float)(precision - 1.0f);
		const float frac_uv = 1.0f / (float)precision;

		for (size_t t = 0; t < precision; ++t) {
			const float theta = (t * frac * 2 + 1) * pi<float>();
			const float cost = std::cos(theta), sint = std::sin(theta);
			const v2f pos = v2f(R + r * cost, r * sint);

			for (size_t p = 0; p < precision + 1; ++p) {
				const float phi = p * frac_uv * 2 * pi<float>();
				const float cosp = std::cos(phi), sinp = std::sin(phi);
				vertices[p + (precision + 1) * t] = v3f(pos[0] * cosp, pos[0] * sinp, pos[1]);
				normals[p + (precision + 1) * t] = v3f(cost * cosp, cost * sinp, sint);
				uvs[p + (precision + 1) * t] = v2f(t * frac_uv, p * frac_uv);
			}
		}

		size_t tri_id = 0;
		for (size_t t = 0; t < precision - 1; ++t) {
			for (size_t p = 0; p < precision; ++p, tri_id += 2) {
				size_t current_id = p + (precision + 1) * t;
				size_t next_in_row = current_id + 1;
				size_t next_in_col = current_id + precision + 1;
				size_t next_next = next_in_col + 1;
				triangles[tri_id] = v3u((uint)current_id, (uint)next_in_row, (uint)next_in_col);
				triangles[tri_id + 1] = v3u((uint)next_in_row, (uint)next_next, (uint)next_in_col);
			}
		}

		Mesh mesh;
		mesh.setVertices(vertices);
		mesh.setTriangles(triangles);
		mesh.setNormals(normals);
		mesh.setUVs(uvs);
		return mesh;
	}

	Mesh Mesh::getCube(const Box& box)
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
		Mesh::UVs uvs(4 * 6);
		for (size_t f = 0; f < 6; ++f) {
			for (size_t v = 0; v < 4; ++v) {
				vertices[4 * f + v] = box.corner((Box::CornerType)corners[f][v]);
				uvs[4 * f + v] = v2f(v / 2, v % 2);
			}
		}
		
		Mesh out;
		out.setTriangles(tris);
		out.setVertices(vertices);
		out.computeVertexNormalsFromVertices();
		out.setUVs(uvs);

		return out;
	}

	Mesh Mesh::getCube(const v3f& center, const v3f& halfDiag)
	{
		return getCube(Box(center - halfDiag, center + halfDiag));
	}

	void Mesh::removeModelCallback(const size_t id)
	{
		if (id) {
			modelCallbacks->erase(id);
		}
	}

	void Mesh::removeGeometryCallback(const size_t id)
	{
		if (id) {
			geometryCallbacks->erase(id);
		}
	}

	void Mesh::invalidateModel()
	{
		dirtyBox = true;
		for (const auto& callback : *modelCallbacks) {
			callback.second();
		}
	}

	void Mesh::invalidateGeometry()
	{
		dirtyBox = true;
		for (const auto& callback : *geometryCallbacks) {
			callback.second();
		}
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
	
	MeshGL MeshGL::getAxis()
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

		MeshGL out;
		out.setTriangles(tris);
		out.setVertices(verts);
		out.setColors(cols);
		out.mode = GL_LINE;

		return out;
	}

	MeshGL MeshGL::fromEndPoints(const std::vector<v3f>& pts)
	{
		MeshGL out;
		out.setVertices(pts);

		Mesh::Triangles tris(pts.size() / 2);
		for (uint i = 0, j = 0; i < tris.size(); ++i, j += 2) {
			tris[i] = v3u(j, j, j + 1);
		}

		out.setTriangles(tris);
		out.mode = GL_LINE;
		return out;
	}

	MeshGL MeshGL::fromPoints(const std::vector<v3f>& pts)
	{
		MeshGL out;
		out.setVertices(pts);
		out.mode = GL_POINT;
		return out;
	}

	MeshGL MeshGL::quad(const v3f& center, const v3f& semiDiagonalA, const v3f& semiDiagonalB, const v2f& uvs_tl, const v2f uvs_br)
	{
		static const Mesh::Triangles quadTriangles = {
			{ 0,2,1 }, { 0,3,2 }
		};

		static std::vector<v3f> vertices(4);
		static Mesh::UVs uvs(4);

		vertices = {
			center - semiDiagonalA, center - semiDiagonalB,
			center + semiDiagonalA, center + semiDiagonalB,
		};

		uvs = {
			uvs_tl, { uvs_br[0], uvs_tl[1] },
			uvs_br, { uvs_tl[0], uvs_br[1] },
		};

		MeshGL out;
		out.setVertices(vertices);
		out.setTriangles(quadTriangles);
		out.setUVs(uvs);

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
#pragma once

#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <imgui-master/imgui.h>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <iostream>
#include <string>
#include <fstream>

#define SHADER_STR(version, shader)  std::string("#version " #version "\n" #shader)

namespace gloops_types {

	using Time = long long int;
	using uint = unsigned int;
	using uchar = unsigned char;


	using v3b = Eigen::Matrix<uchar, 3, 1>;

	using v2i = Eigen::Vector2i;
	using v3i = Eigen::Vector3i;
	using v4i = Eigen::Vector4i;

	using v3u = Eigen::Matrix<uint32_t, 3, 1>;

	using v2f = Eigen::Vector2f;
	using v3f = Eigen::Vector3f;
	using v4f = Eigen::Vector4f;

	using v3d = Eigen::Vector3d;
	using v2d = Eigen::Vector2d;

	using m3f = Eigen::Matrix3f;
	using m4f = Eigen::Matrix4f;

	using m3d = Eigen::Matrix3d;
	using m4d = Eigen::Matrix4d;

	using Diag4f = Eigen::DiagonalMatrix<float, 4>;

	using Qf = Eigen::Quaternion<float>;
	using Qd = Eigen::Quaternion<double>;

	using Plane3f = Eigen::Hyperplane<float, 3>;
	using Line3f = Eigen::ParametrizedLine<float, 3>;

	using Rf = Eigen::AngleAxis<float>;
	using Rd = Eigen::AngleAxis<double>;

	using BBox3f = Eigen::AlignedBox<float, 3>;
	using BBox2f = Eigen::AlignedBox<float, 2>;

	using BBox2d = Eigen::AlignedBox<double, 2>;

	template<typename T>
	using Ray = Eigen::ParametrizedLine<T, 3>;
}

namespace ImGui {

	inline void Text(const std::string& s) {
		ImGui::Text("%s", s.c_str());
	}

	inline void TextColored(const std::string& s, const gloops_types::v4f& c) {
		ImGui::TextColored({ c[0],c[1],c[2],c[3] }, "%s", s.c_str());
	}
}

namespace gloops {

	using namespace gloops_types;

	template<typename FunType>
	void renderGroup(const std::string& s, FunType&& f) {
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, s.c_str());
		f();
		glPopDebugGroup();
	}

	class GLptr {

	public:
		using Generator = std::function<void(GLuint*)>;
		using Destructor = std::function<void(const GLuint*)>;
	
		GLptr() = default;

		GLptr(Generator&& generator, Destructor&& destructor) {
			_id = std::shared_ptr<GLuint>(new GLuint(), destructor);
			generator(_id.get());
		}

		operator GLuint() const {
			return *_id;
		}

		operator bool() const {
			return _id.operator bool();
		}

	protected:
		std::shared_ptr<GLuint> _id;
	};

	class GLptrArray {
		using Generator = std::function<void(GLsizei, GLuint*)>;
		using Destructor = std::function<void(GLsizei, const GLuint*)>;

		using GLuints = std::vector<GLuint>;

		GLptrArray() = default;

		GLptrArray(Generator&& generator, Destructor&& destructor, size_t n) {
			_ids = std::shared_ptr<GLuints>(new GLuints(n), 
				[&](const GLuints* ptr) { destructor(static_cast<GLsizei>(ptr->size()), ptr->data()); });
			generator(static_cast<GLsizei>(_ids->size()), _ids->data());
		}

		GLuint operator[](size_t idx) const {
			return (*_ids)[idx];
		}

	protected:
		std::shared_ptr<GLuints> _ids;
	};


	inline std::string loadFile(const std::string& path) {
		std::ifstream ifs(path);
		std::stringstream sst;
		sst << ifs.rdbuf();
		return sst.str();
	}

	inline void v3fgui(const v3f& v) {
		ImGui::Text(std::to_string(v[0]) + " " + std::to_string(v[1]) + " " + std::to_string(v[2]));
	}

	template<typename U = double>
	constexpr U pi() {
		return static_cast<U>(3.14159265358979323846);
	}

	template<typename U, typename T = U>
	Eigen::Matrix<T, 3, 1> sphericalDir(const U& phi, const U& theta) {
		U cosp = std::cos(phi), sinp = std::sin(phi),
			cost = std::cos(theta), sint = std::sin(theta);
		return Eigen::Matrix<U, 3, 1>(sint * cosp, sint * sinp, cost). template cast<T>();
	}

	template<typename T>
	Eigen::Matrix<T, 2, 1> uvToRad(const Eigen::Matrix<T, 2, 1>& uv) {
		return Eigen::Matrix<T, 2, 1>(2 * pi() * std::fmod(uv[0], 1.0), pi() * std::fmod(uv[1], 1.0));
	}
	template<typename T>
	Eigen::Matrix<T, 2, 1> radsToUV(const Eigen::Matrix<T, 2, 1>& rads) {
		return Eigen::Matrix<T, 2, 1>(std::fmod(rads[0] / (2 * pi()), 1.0), std::fmod(rads[1] / pi(), 1.0));
	}

	// maps [0,1]x[0,1] to [0, 2pi]x[0,pi] to dir
	template<typename U, typename T = U>
	Eigen::Matrix<T, 3, 1> sphericalDirUV(const Eigen::Matrix<U, 2, 1>& uv) {
		Eigen::Matrix<U, 2, 1> rads = uvToRad(uv);
		return sphericalDir<double, T>(rads[0], rads[1]);
	}

	template<typename T>
	Eigen::Matrix<T, 2, 1> dirToRads(const Eigen::Matrix<T, 3, 1>& dir) {
		return Eigen::Matrix<T, 2, 1>(std::atan2(dir[1], dir[0]), std::acos(dir[2]));
	}

	template<typename U, typename T = U>
	T degToRad(U angle_deg)
	{
		return static_cast<T>(static_cast<double>(angle_deg) * 2.0 * pi<double>() / 360.0);
	}

	template<typename U, typename T = U>
	T radToDeg(U angle_rad)
	{
		return static_cast<T>(static_cast<double>(angle_rad) * 360.0 / (2 * pi<double>()));
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> rotation(const Eigen::Matrix<T, 3, 3>& rot) {
		Eigen::Matrix<T, 4, 4> out = Eigen::Matrix<T, 4, 4> ::Identity();
		out.template block<3, 3>(0, 0) = rot;
		return out;
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> translation(const Eigen::Matrix<T, 3, 1>& position) {
		Eigen::Matrix<T, 4, 4> out = Eigen::Matrix<T, 4, 4> ::Identity();
		out.template block<3, 1>(0, 3) = position;
		return out;
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> scaling(T scale) {
		return Eigen::DiagonalMatrix<T, 4>(Eigen::Matrix<T, 4, 1>(scale, scale, scale, static_cast<T>(1)));
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> scaling(const Eigen::Matrix<T, 3, 1>& scale) {
		return Eigen::DiagonalMatrix<T, 4>(Eigen::Matrix<T, 4, 1>(scale[0], scale[1], scale[2], static_cast<T>(1)));
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> transformation(const Eigen::Matrix<T, 3, 1>& position) {
		return translation(position);
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> transformation(const Eigen::Matrix<T, 3, 1>& position, const Eigen::Matrix<T, 3, 3>& rot) {
		return translation(position) * rotation(rot);
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> transformation(const Eigen::Matrix<T, 3, 1>& position, const Eigen::Matrix<T, 3, 3>& rot, const Eigen::Matrix<T, 3, 1>& scale) {
		return translation(position) * rotation(rot) * scaling(scale);
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> transformation(const Eigen::Matrix<T, 3, 1>& position, const Eigen::Matrix<T, 3, 3>& rot, T scale) {
		return translation(position) * rotation(rot) * scaling(scale);
	}

	template<typename T>
	Eigen::Matrix<T, 3, 1> applyTransformation(const Eigen::Matrix<T, 4, 4>& t, const Eigen::Matrix<T, 3, 1>& p) {
		Eigen::Matrix<T, 4, 1> x = t * Eigen::Matrix<T, 4, 1>(p[0], p[1], p[2], 1.0);
		return Eigen::Matrix<T, 3, 1>(x[0], x[1], x[2]);
	}

}

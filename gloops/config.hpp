#pragma once

#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "config_imgui.hpp"
#include <imgui.h>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>
#include <thread>
#include <random>

#define GLOOPS_SHADER_STR(version, shader)  std::string("#version " #version "\n" #shader)

namespace gloops {

	using Time = long long int;
	using uint = unsigned int;
	using uchar = unsigned char;

	template<typename T, int N>
	using Vec = Eigen::Matrix<T, N, 1>;

	using v3b = Vec<uchar, 3>;
	using v4b = Vec<uchar, 4>;

	using v2i = Vec<int, 2>;
	using v3i = Vec<int, 3>;
	using v4i = Vec<int, 4>;

	using v3u = Vec<uint32_t, 3>;

	using v2f = Vec<float, 2>;
	using v3f = Vec<float, 3>;
	using v4f = Vec<float, 4>;

	using v2d = Vec<double, 2>;
	using v3d = Vec<double, 3>;

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
	using RayT = Eigen::ParametrizedLine<T, 3>;

	template<typename T>
	T saturate_cast(double d) {
		return static_cast<T>(d);
	}

	template<>
	inline uchar saturate_cast<uchar>(double d) {
		return static_cast<uchar>(std::clamp(d, 0.0, 255.0));
	}

	template<typename T, int N>
	std::string str(const gloops::Vec<T, N>& v) {
		std::stringstream s;
		s << v[0] << " " << v[1] << " " << v[2];
		return s.str();
	}

}

namespace ImGui {

	inline void Text(const ::std::string& s) {
		ImGui::Text("%s", s.c_str());
	}

	inline void Text(const ::std::stringstream& s) {
		ImGui::Text(s.str());
	}

	inline void TextColored(const ::std::string& s, const gloops::v3f& c) {
		ImGui::TextColored(ImVec4(c[0], c[1], c[2], 1.0f), "%s", s.c_str());
	}

	inline void TextColored(const ::std::string& s, const gloops::v4f& c) {
		ImGui::TextColored(ImVec4(c[0], c[1], c[2], c[3]), "%s", s.c_str());
	}

	inline float TitleHeight() {
		return ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
	}

	template<typename F>
	void ItemWithSize(float size, F&& f) {
		ImGui::PushItemWidth(size);
		f();
		ImGui::PopItemWidth();
	}
	
	inline bool colPicker(const std::string& s, gloops::v3f& color, unsigned int flag = 0) {
		ImGui::Text(s);
		ImGui::SameLine();
		return ImGui::ColorEdit3(s.c_str(), &color[0], flag | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
	};

	inline bool colPicker(const std::string& s, gloops::v4f& color, unsigned int flag = 0) {
		ImGui::Text(s);
		ImGui::SameLine();
		return ImGui::ColorEdit4(s.c_str(), &color[0], flag | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
	};

}

namespace gloops {

	template<typename FunType>
	void renderGroup(const std::string& s, FunType&& f) {
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, s.c_str());
		f();
		glPopDebugGroup();
	}

	class GLptr {

	public:
		//using Generator = std::function<void(GLuint*)>;
		//using Destructor = std::function<void(const GLuint*)>;
	
		GLptr() = default;

		template<typename Generator, typename Destructor>
		GLptr(Generator&& generator, Destructor&& destructor) {
			_id = std::shared_ptr<GLuint>(new GLuint(), std::forward<Destructor>(destructor));
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
		//using Generator = std::function<void(GLsizei, GLuint*)>;
		//using Destructor = std::function<void(GLsizei, const GLuint*)>;

		using GLuints = std::vector<GLuint>;

		GLptrArray() = default;

		template<typename Generator, typename Destructor>
		GLptrArray(Generator&& generator, Destructor&& destructor, size_t n) {
			_ids = std::shared_ptr<GLuints>(new GLuints(n), 
				[&](const GLuints* ptr) {
				destructor(static_cast<GLsizei>(ptr->size()), ptr->data());
			});
			generator(static_cast<GLsizei>(_ids->size()), _ids->data());
		}

		GLuint operator[](size_t idx) const {
			return (*_ids)[idx];
		}

		operator bool() const {
			return _ids.operator bool();
		}

	protected:
		std::shared_ptr<GLuints> _ids;
	};

}

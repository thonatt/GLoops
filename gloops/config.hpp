#pragma once

#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <imgui-master/imgui.h>

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

}

namespace ImGui {

	inline void Text(const ::std::string& s) {
		ImGui::Text("%s", s.c_str());
	}

	inline void Text(const ::std::stringstream& s) {
		ImGui::Text(s.str());
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


	inline std::string loadFile(const std::string& path) {
		std::ifstream ifs(path);
		std::stringstream sst;
		sst << ifs.rdbuf();
		return sst.str();
	}

	inline void v3fgui(const v3f& v) {
		ImGui::Text(std::to_string(v[0]) + " " + std::to_string(v[1]) + " " + std::to_string(v[2]));
	}

	template<typename F>
	void parallelForEach(int from_incl, int to_excl, F&& f, int max_num_threads)
	{
		const int numJobs = to_excl - from_incl;
		if (numJobs <= 0) {
			return;
		}

		const int numCores = std::thread::hardware_concurrency();
		const int numThreads = std::clamp(numCores - 1, 1, std::min(max_num_threads, numJobs));
		const int numJobsPerThread = numJobs / numThreads + 1;

		std::cout << "parallel for each : " << numCores << " cores availables, " << numJobs << " jobs divided into " << numThreads << " threads" << std::endl;

		std::vector<std::thread> threads(numThreads);
		for (int t = 0; t < numThreads; ++t) {
			const int t_start = from_incl + t * numJobsPerThread;
			const int t_end = std::min(from_incl + (t + 1) * numJobsPerThread, to_excl);
			//std::cout << "\t" << t_start << " " << t_end << std::endl;
			threads[t] = std::thread([](F&& fun, const int from, const int to) {
				for (int i = from; i < to; ++i) {
					fun(i);
				}
			}, std::forward<F>(f), t_start, t_end);
		}
		std::for_each(threads.begin(), threads.end(), [](std::thread& t) { t.join(); });
	}
	
	template<typename F>
	void parallelForEach(int from_incl, int to_excl, F&& f) {
		parallelForEach(from_incl, to_excl, std::forward<F>(f), 256);
	}

	template<typename T, int N>
	Vec<T, N> randomVec() 
	{
		static std::random_device device;
		static std::mt19937 generator(device());
		std::uniform_real_distribution<T> distribution(-1, 1);

		Vec<T, N> out;
		for (int i = 0; i < N; ++i) {
			out[i] = distribution(generator);
		}
		return out;
	}

	template<typename T, int N>
	Vec<T, N> randomVec(T min, T max)
	{
		static std::random_device device;
		static std::mt19937 generator(device());
		std::uniform_real_distribution<T> distribution(min, max);

		Vec<T, N> out;
		for (int i = 0; i < N; ++i) {
			out[i] = distribution(generator);
		}
		return out;
	}

	template<typename T, int N>
	Vec<T, N> randomUnit()
	{
		Vec<T, N> out;
		do {
			out = randomVec<T, N>();
		} while (out.squaredNorm() > T(1));
		return out.normalized();
	}

	template<typename T, typename U>
	auto lerp(const T& a1, const T& a2, U u) 
	{
		return a1 + u * (a2 - a1);
	};
	
	template<typename T, typename U>
	auto smoothstep3(const T& a1, const T& a2, U x)
	{
		const U u = x * x * (T(3) - T(2) * x);
		return lerp(a1, a2, u);
	}

	template<typename T, typename U>
	auto smoothstep5(const T& a1, const T& a2, U x)
	{
		const U u = x* x* x* (x * (x * U(6) - U(15)) + U(10));
		return lerp(a1, a2, u);
	}
	
	template<typename U = float>
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
	Eigen::Matrix<T, 4, 4> rotationMatrix(const Eigen::Matrix<T, 3, 3>& rot) {
		Eigen::Matrix<T, 4, 4> out = Eigen::Matrix<T, 4, 4>::Identity();
		out.template block<3, 3>(0, 0) = rot;
		return out;
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> translationMatrix(const Eigen::Matrix<T, 3, 1>& position) {
		Eigen::Matrix<T, 4, 4> out = Eigen::Matrix<T, 4, 4>::Identity();
		out.template block<3, 1>(0, 3) = position;
		return out;
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> scalingMatrix(T scale) {
		return Eigen::DiagonalMatrix<T, 4>(Eigen::Matrix<T, 4, 1>(scale, scale, scale, T(1)));
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> scalingMatrix(const Eigen::Matrix<T, 3, 1>& scale) {
		return Eigen::DiagonalMatrix<T, 4>(Eigen::Matrix<T, 4, 1>(scale[0], scale[1], scale[2], T(1)));
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> transformationMatrix(const Eigen::Matrix<T, 3, 1>& position) {
		return translationMatrix(position);
	}

	template<typename T>
	Eigen::Matrix<T,4,4> transformationMatrix(const Eigen::Matrix<T, 3, 1>& position, const Eigen::Matrix<T, 3, 3>& rot) {
		return translationMatrix(position) * rotationMatrix(rot);
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> transformationMatrix(const Eigen::Matrix<T, 3, 1>& position, const Eigen::Matrix<T, 3, 3>& rot, const Eigen::Matrix<T, 3, 1>& scale) {
		return translationMatrix(position) * rotationMatrix(rot) * scalingMatrix(scale);
	}

	template<typename T>
	Eigen::Matrix<T, 4, 4> transformationMatrix(const Eigen::Matrix<T, 3, 1>& position, const Eigen::Matrix<T, 3, 3>& rot, T scale) {
		return translationMatrix(position) * rotationMatrix(rot) * scalingMatrix(scale);
	}

	template<typename T>
	Eigen::Matrix<T, 3, 1> applyTransformationMatrix(const Eigen::Matrix<T, 4, 4>& t, const Eigen::Matrix<T, 3, 1>& p) {
		Eigen::Matrix<T, 4, 1> x = t * Eigen::Matrix<T, 4, 1>(p[0], p[1], p[2], 1.0);
		return Eigen::Matrix<T, 3, 1>(x[0], x[1], x[2]);
	}

	inline BBox3f mergeBoundingBoxes() { return BBox3f(); }

	template<typename ... Boxes>
	BBox3f mergeBoundingBoxes(const BBox3f& box, const Boxes& ...boxes) {
		if (sizeof...(Boxes) == 0) {
			return box;
		} 
		BBox3f tmp = box;
		return tmp.extend(mergeBoundingBoxes(boxes...));
	}


}

#pragma once

#include "config.hpp"

namespace gloops {

	std::string loadFile(const std::string& path);

	void v3fgui(const v3f& v);

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

		std::stringstream s;
		s << "parallel for each : " << numCores << " cores availables, " << numJobs << " jobs divided into " << numThreads << " threads" << std::endl;
		addToLogs(LogType::LOG, s.str());

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
		const U u = x * x * x * (x * (x * U(6) - U(15)) + U(10));
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

	template<typename T_out, typename T_in, int N>
	Vec<T_out, N> ceil(const Vec<T_in, N>& vec) {
		return vec.unaryExpr([](T_in t) { return std::ceil(t); }).template cast<T_out>();
	}

	template<typename T_out, typename T_in, int N>
	Vec<T_out, N> floor(const Vec<T_in, N>& vec) {
		return vec.unaryExpr([](T_in t) { return std::floor(t); }).template cast<T_out>();
	}

	template<typename T_out, typename T_in, int N>
	Vec<T_out, N> round(const Vec<T_in, N>& vec) {
		return vec.unaryExpr([](T_in t) { return std::round(t); }).template cast<T_out>();
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
	Eigen::Matrix<T, 4, 4> transformationMatrix(const Eigen::Matrix<T, 3, 1>& position, const Eigen::Matrix<T, 3, 3>& rot) {
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

	template<typename ... Boxes>
	BBox3f mergeBoundingBoxes(const BBox3f& box, const Boxes& ...boxes) {
		if (sizeof...(Boxes) == 0) {
			return box;
		}
		BBox3f tmp = box;
		return tmp.extend(mergeBoundingBoxes(boxes...));
	}
	inline BBox3f mergeBoundingBoxes() { return BBox3f(); }


}
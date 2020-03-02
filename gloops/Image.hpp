#pragma once

#include "config.hpp"

namespace gloops {

	uchar* stbiImageLoad(const std::string& path, int& w, int& h, int& n);
	void stbiImageFree(uchar* ptr);

	template<typename T, int N>
	class Image {
	public:

		using Pixel = Vec<T, N>; //std::conditional_t<N == 1, T, Eigen::Matrix<T, N, 1>>;

		Image() = default;

		Image(int width, int height) {
			resize(width, height);
		}

		int w() const
		{
			return _w;
		}

		int h() const
		{
			return _h;
		}

		constexpr int n() const
		{
			return N;
		}

		const Pixel& pixel(int x, int y) const
		{
			return _pixels[static_cast<size_t>(y)* w() + x];
		}

		Pixel& pixel(int x, int y)
		{
			return _pixels[static_cast<size_t>(y)* w() + x];
		}

		T& at(int x, int y, int c = 0) {
			return pixel(x, y)[c];
		}

		const T& at(int x, int y, int c = 0) const {
			return pixel(x, y)[c];
		}

		v4f pixel4f(int x, int y) const {
			const Pixel& p = pixel(x, y);

			switch (N)
			{
			case 1: return v4f((float)p[0], 0, 0, 0);
			case 2: return v4f((float)p[0], (float)p[1], 0, 0);
			case 3: return v4f((float)p[0], (float)p[1], (float)p[2], 0);
			case 4: return v4f((float)p[0], (float)p[1], (float)p[2], (float)p[3]);
			default:
				return v4f::Zero();
			}
		}

		bool boundsCheck(int x, int y) const
		{
			return (x >= 0 && y >= 0 && x < w() && y < h());
		}

		void load(const std::string& path)
		{
			static_assert(std::is_same_v<T, uchar>, "GLoops only support uchar sbti image loads");

			int  w, h, n = N;
			uchar* data_ptr = stbiImageLoad(path, w, h, n);

			if (n != N) {
				std::cout << " wrong number of channels when loading " <<
					std::filesystem::absolute(std::filesystem::path(path)).string() << "\n" <<
					" expecting " << N << " channels but received " << n << std::endl;
				//return;
			}

			if (data_ptr) {
				_path = path;
				resize(w, h);
				std::memcpy(_pixels.data(), data_ptr, _pixels.size() * sizeof(T));
				stbiImageFree(data_ptr);
				std::cout << path << " : " << _w << " x " << _h << " x " << N << std::endl;
			} else {
				std::cout << " cant load " << path << std::endl;
			}
		}

		const uchar* data() const
		{
			return reinterpret_cast<const uchar*>(_pixels.data());
		}

		uchar* data()
		{
			return reinterpret_cast<uchar*>(_pixels.data());
		}

		const std::string& getPath() const
		{
			return _path;
		}

		void resize(int w, int h) {
			if (w == _w && h == _h) {
				return;
			}
			_pixels.resize(w * static_cast<size_t>(h)* N);
			_w = w;
			_h = h;
		}

		void setTo(const Pixel& pix) {
			for (int i = 0; i < h(); ++i) {
				for (int j = 0; j < w(); ++j) {
					pixel(j, i) = pix;
				}
			}
		}

		Image subImage(int offsetX, int offsetY, int width, int height) {
			constexpr size_t SizeOfPixel = sizeof(Pixel);
			Image out(width, height);		
			for (size_t row = 0; row < (size_t)height; ++row) {
				std::memcpy(
					out.data() + SizeOfPixel * width * row,
					data() + SizeOfPixel * (offsetX + w() * (offsetY + row)),
					SizeOfPixel * width
				);
			}
			return out;
		}

		Image<float, N> operator+(const Image& other) const {
			Image out(w(), h());
			for (int i = 0; i < h(); ++i) {
				for (int j = 0; j < w(); ++j) {
					for (int c = 0; c < N; ++c) {
						out.pixel(j, i)[c] = pixel(j, i)[c] + other.pixel(j, i)[c];
					}
				}
			}
			return out;
		}

		Image<float, N> operator-(const Image& other) const {
			Image out(w(), h());
			for (int i = 0; i < h(); ++i) {
				for (int j = 0; j < w(); ++j) {
					for (int c = 0; c < N; ++c) {
						out.pixel(j, i)[c] = pixel(j, i)[c] - other.pixel(j, i)[c];
					}
				}
			}
			return out;
		}

		Image<float, N> operator*(const Image& other) const {
			Image out(w(), h());
			for (int i = 0; i < h(); ++i) {
				for (int j = 0; j < w(); ++j) {
					for (int c = 0; c < N; ++c) {
						out.pixel(j, i)[c] = pixel(j, i)[c] * other.pixel(j, i)[c];
					}
				}
			}
			return out;
		}

		template<typename U, int M>
		Image<float, (N > M ? N : M)> operator*(const Vec<U, M>& other) const {
			constexpr int C = (N > M ? N : M);
			Image<float, C> out(w(), h());
			for (int i = 0; i < h(); ++i) {
				for (int j = 0; j < w(); ++j) {
					for (int c = 0; c < C; ++c) {
						out.pixel(j, i)[c] = pixel(j, i)[std::min(c, N - 1)] * other[std::min(c, M - 1)];
					}
				}
			}
			return out;
		}

		template<typename U = T, int M = N>
		Image<U, M> convert(
			const Vec<double, M>& scaling, const Vec<double, M>& offset,
			const std::function<bool(int x, int y)>& mask = [](int i, int j) { return true; },
			const Vec<double, M>& defValue = 0
		) const {
			Image<U, M> out(w(), h());
			for (int n = 0; n < M; ++n) {
				const int c = std::min(n, N - 1);
				for (int i = 0; i < h(); ++i) {
					for (int j = 0; j < w(); ++j) {
						out.at(j, i, n) = saturate_cast<U>(mask(j, i) ? (scaling[c] * at(j, i, c) + offset[c]) : defValue[c]);
					}
				}

			}
			return out;
		}

		template<typename U = T, int M = N>
		Image<U, M> convert(
			double scaling = 1, double offset = 0,
			const std::function<bool(int x, int y)>& mask = [](int i, int j) { return true; },
			double defValue = 0
		) const {
			Vec<double, M> ones = Vec<double, M>::Ones();
			return convert<U, M>(scaling * ones, offset * ones, mask, defValue * ones);
		}

		template<typename U = T, int M = N>
		Image<U, M> normalized(
			double min, double max,
			const std::function<bool(int x, int y)>& mask = [](int i, int j) { return true; },
			double defValue = 0
		) const {

			Image<U, M> out(w(), h());

			Vec<double, M> scaling, offset, defVal;
			for (int n = 0; n < M; ++n) {
				const int c = std::min(n, N - 1);

				T pmin = std::numeric_limits<T>::infinity(),
					pmax = -std::numeric_limits<T>::infinity();

				for (int i = 0; i < h(); ++i) {
					for (int j = 0; j < w(); ++j) {
						if (!mask(j, i)) {
							continue;
						}
						const T& val = at(j, i, c);
						pmin = val < pmin ? val : pmin;
						pmax = val > pmax ? val : pmax;
					}
				}

				if (pmax == pmin) {
					scaling[c] = 1;
				} else {
					scaling[c] = (max - min) / (double(pmax) - double(pmin));
				}
				offset[c] = min - pmin * scaling[c];
				defVal[c] = defValue;
			}

			return convert<U, M>(scaling, offset, mask, defVal);
		}

	private:

		std::string _path;
		std::vector<Pixel> _pixels;
		int _w = 0, _h = 0;
	};

	template<typename T, int N>
	Image<float, N> operator-(double s, const Image<T, N>& image) {
		Image<float, N> out(image.w(), image.h());
		for (int i = 0; i < out.h(); ++i) {
			for (int j = 0; j < out.w(); ++j) {
				for (int c = 0; c < N; ++c) {
					out.pixel(j, i)[c] = (float)s - (float)image.pixel(j, i)[c];
				}
			}
		}
		return out;
	}

	template<typename T, int N>
	Image<float, N> operator+(double s, const Image<T, N>& image) {
		Image<float, N> out(image.w(), image.h());
		for (int i = 0; i < out.h(); ++i) {
			for (int j = 0; j < out.w(); ++j) {
				for (int c = 0; c < N; ++c) {
					out.pixel(j, i)[c] = (float)s + (float)image.pixel(j, i)[c];
				}
			}
		}
		return out;
	}

	template<typename T, int N>
	Image<float, N> operator+(const Image<T, N>& image, double s) {
		return s + image;
	}

	template<typename T, int N>
	Image<float, N> operator*(double s, const Image<T, N>& image) {
		Image<float, N> out(image.w(), image.h());
		for (int i = 0; i < out.h(); ++i) {
			for (int j = 0; j < out.w(); ++j) {
				for (int c = 0; c < N; ++c) {
					out.pixel(j, i)[c] = (float)s * (float)image.pixel(j, i)[c];
				}
			}
		}
		return out;
	}

	using Image1b = Image<uchar, 1>;
	using Image3b = Image<uchar, 3>;
	using Image4b = Image<uchar, 4>;

	using Image1f = Image<float, 1>;
	using Image2f = Image<float, 2>;
	using Image3f = Image<float, 3>;

	Image3b checkersTexture(int w, int h, int size);
	Image1f perlinNoise(int w, int h, int size = 5, int levels = 1);

	struct ImageInfosData {
		ImageInfosData(int w, int h, int n, const void* data)
			: _w(w), _h(h), _n(n), _data(data) {
		}

		ImageInfosData(int w, int h, int d, int n, const void* data)
			: _w(w), _h(h), _d(d), _n(n), _data(data) {
		}

		int w() const { return _w; }
		int h() const { return _h; }
		int n() const { return _n; }
		int d() const { return _d; }

		const void* data() const { return _data; }

		int _w, _h, _d = 0, _n;
		const void* _data;
	};

	template<typename T>
	struct ImageInfos : ImageInfosData {
		ImageInfos(const T& t)
			: ImageInfosData(t.w(), t.h(), t.n(), t.data()) { }
	};

}
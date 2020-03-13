#include "Image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace gloops {

	uchar* stbiImageLoad(const std::string& path, int& w, int& h, int& n)
	{
		return stbi_load(path.c_str(), &w, &h, &n, 0);
	}

	void stbiImageFree(uchar* ptr)
	{
		stbi_image_free(ptr);
	}

	Image3b checkersTexture(int w, int h, int size)
	{
		const int r = std::max(size, 1);

		Image3b out(w, h);
		for (int i = 0; i < h; ++i) {
			for (int j = 0; j < w; ++j) {
				int c = (i / r + j / r) % 2 ? 255 : 0;
				out.pixel(j, i) = v3b(c, c, c);
			}
		}
		return out;
	}

	Image1f perlinNoise(int w, int h, int size, int levels)
	{
		size = std::max(size, 1);

		Image<float, 2> grads(w / size + 1, h / size + 1);
		for (int i = 0; i < grads.h(); ++i) {
			for (int j = 0; j < grads.w(); ++j) {
				grads.pixel(j, i) = randomUnit<float, 2>();
			}
		}

		Image1f out(w, h);

		const float ratio = 1 / (float)size;
		for (int i = 0; i < h; ++i) {
			int iy = i / size;
			float fy = i * ratio;
			float dy = fy - (float)iy;

			for (int j = 0; j < w; ++j) {
				int ix = j / size;
				float fx = j * ratio;
				float dx = fx - (float)ix;

				float vx0 = smoothstep3(
					grads.pixel(ix + 0, iy + 0).dot(v2f(fx - (ix + 0), fy - (iy + 0))),
					grads.pixel(ix + 1, iy + 0).dot(v2f(fx - (ix + 1), fy - (iy + 0))),
					dx
				);

				float vx1 = smoothstep3(
					grads.pixel(ix + 0, iy + 1).dot(v2f(fx - (ix + 0), fy - (iy + 1))),
					grads.pixel(ix + 1, iy + 1).dot(v2f(fx - (ix + 1), fy - (iy + 1))),
					dx
				);

				out.at(j, i) = smoothstep3(vx0, vx1, dy);
			}
		}

		return out;
	}

}
#pragma once

#include "config.hpp"

#include <string>
#include <list>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <map>

namespace gloops {

	template<typename T, int N>
	class Image {
	public:

		using Pixel = Eigen::Matrix<T, N, 1>;

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

		void load(const std::string & path)
		{
			static_assert(std::is_same_v<T, uchar>, "GLoops only support uchar sbti image loads");

			int  w, h, n;
			uchar * data_ptr = stbi_load(path.c_str(), &w, &h, &n, 0);
			
			if (n != N) {
				std::cout << " wrong number of channels when loading " << path << "\n" <<
					" expecting " << N << " channels but received " << n << std::endl;
				return;
			}

			if (data_ptr) {
				_path = path;
				allocate(w, h);
				std::memcpy(_pixels.data(), data_ptr, _pixels.size() * sizeof(T));
				stbi_image_free(data_ptr);
				std::cout << path << " : " << _w << " x " << _h << " x " << N << std::endl;
			} else {
				std::cout << " cant load " << path << std::endl;
			}
		}

		const uchar *data() const
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

		void allocate(int w, int h) {
			if (w == _w && h == _h) {
				return;
			}
			_pixels.resize(w * static_cast<size_t>(h) * N);
			_w = w;
			_h = h;
		}


	private:

		std::string _path;
		std::vector<Pixel> _pixels;
		int _w = 0, _h = 0;
	};


	using Image3b = Image<uchar, 3>;

	struct ImageInfosData {
		ImageInfosData(int w, int h, int n, const void* data)
			: _w(w), _h(h), _n(n), _data(data) {
		}

		int w() const { return _w; }
		int h() const { return _h; }
		int n() const { return _n; }
		const void* data() const { return _data; }

		int _w, _h, _n;
		const void* _data;
	};

	template<typename T>
	struct ImageInfos : ImageInfosData {
		ImageInfos(const T& t)
			: ImageInfosData(t.w(), t.h(), t.n(), t.data()) { }
	};

	class TexParamsFormat
	{
	public:
		TexParamsFormat() = default;

		TexParamsFormat(GLenum target, GLenum type, GLenum internal_format, GLenum format)
		: target(target), type(type), internal_format(internal_format), format(format) { }

		bool operator==(const TexParamsFormat& other) const {
			return target == other.target && type == other.type && internal_format == other.internal_format && format == other.format;
		}
		bool operator!=(const TexParamsFormat& other) const {
			return !(*this == other);
		}

		static const TexParamsFormat RGB, BGR, RGBA, RGBA32F;

	public:
		GLenum target = GL_TEXTURE_2D, type = GL_UNSIGNED_BYTE, internal_format = GL_RGB8, format = GL_RGB;
		mutable bool dirtyFormat = false;
	};

	class TexParamsFilter {

	public:
		bool operator==(const TexParamsFilter& other) const {
			return mag_filter == other.mag_filter && min_filter == other.min_filter;
		}
		bool operator!=(const TexParamsFilter& other) const {
			return !(*this == other);
		}

	public:
		GLint mag_filter = GL_LINEAR, min_filter = GL_LINEAR_MIPMAP_LINEAR;
		mutable bool dirtyFilter = false;
	};

	class TexParamsMipmap {

	public:
		bool operator==(const TexParamsMipmap& other) const {
			return useMipmap == other.useMipmap;
		}
		bool operator!=(const TexParamsMipmap& other) const {
			return !(*this == other);
		}

	protected:
		bool useMipmap = true;
		mutable bool dirtyMipmap = true;
	};

	class TexParamsWrap {
	public:
		bool operator==(const TexParamsWrap& other) const {
			return wrap_s == other.wrap_s && wrap_t == other.wrap_t && wrap_r == other.wrap_r;
		}
		bool operator!=(const TexParamsWrap& other) const {
			return !(*this == other);
		}
	protected:
		GLint wrap_s = GL_REPEAT, wrap_t = GL_REPEAT, wrap_r = GL_REPEAT;
		mutable bool dirtyWrap = true;
	};

	class TexParams : 
		public TexParamsFormat,
		public TexParamsFilter,
		public TexParamsMipmap,
		public TexParamsWrap
	{
	public:
		TexParams() = default;

		TexParams& setTarget(GLenum t);
		TexParams& setInternalFormat(GLenum t);
		TexParams& setFormat(GLenum t);
		TexParams& setType(GLenum t);

		TexParams& setMagFilter(GLint v);
		TexParams& setMipmapStatus(bool enabled);
		
		TexParams& setWrapS(GLint parameter);
		TexParams& setWrapT(GLint parameter);
		TexParams& setWrapR(GLint parameter);

		TexParams(const TexParamsFormat& format) : TexParamsFormat(format) {
		}

		bool operator==(const TexParams& other) const {
			return 
				static_cast<const TexParamsFormat&>(*this) != other &&
				static_cast<const TexParamsFilter&>(*this) != other &&
				static_cast<const TexParamsMipmap&>(*this) != other &&
				static_cast<const TexParamsWrap&>(*this) != other;
		}
		bool operator!=(const TexParams& other) const {
			return !(*this == other);
		}

	};

	template<typename T>
	struct DefaultTexParams;

	template<>
	struct DefaultTexParams<Image3b> : TexParams {
	};

	class Texture : public TexParams {

	public:
		using Ptr = std::shared_ptr<Texture>;

		Texture(TexParams params = {});

		Texture(int w, int h, int nchannels, TexParams params = {});

		static Texture fromPath(const std::string & img_path, TexParams params = {});

		template<typename T>
		Texture(const T& t, TexParams params = DefaultTexParams<T>{});

		template<typename T>
		void update(const T& t, TexParams params = DefaultTexParams<T>{});

		void allocate(int width, int height, int nchannels);
		void uploadToGPU(int xoffset, int yoffset, int width, int height, const void * data);

		void bind() const;
		void bindSlot(GLuint slot) const;

		int w() const;
		int h() const;
		int n() const;

		const TexParams& getParams() const;

		//~Texture();

		operator GLuint() const;
		GLuint getId() const;

	protected:
		void check() const; 
		void setFilter() const;
		void generateMipmaps() const;
		void setWrap() const;

		void createGPUid();
		//void releaseGPUmemory();

		template<typename T>
		void createFromImage(const ImageInfos<T> & img);

		int _w, _h, _n;
		//TexParams params;
		GLptr id;
		//bool filter_changed = true;
	};

	class Framebuffer {
	public:
		using Ptr = std::shared_ptr<Framebuffer>;

		Framebuffer();
		Framebuffer(int w, int h, int n = 4, const TexParams& params = TexParamsFormat::RGBA, int numAttachments = 1);
		
		void resize(int w, int h);

		static Framebuffer empty(int w, int h);

		void setAttachment(const Texture& tex, GLenum attachment, GLint level = 0);

		void bind(GLenum target = GL_FRAMEBUFFER) const;
		void bindDraw() const;
		void bindDraw(GLenum attachment) const;
		void bindRead(GLenum attachment) const;

		void clear(const v4f& color = { 0,0,0,0 }, GLbitfield mask = (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

		GLuint getId() const;

		const Texture& getAttachment(GLenum attachment = GL_COLOR_ATTACHMENT0) const;
		Texture& getAttachment(GLenum attachment = GL_COLOR_ATTACHMENT0);

		int w() const;
		int h() const;

		void blitFrom(
			const Framebuffer& src,  
			GLenum attach_from = GL_COLOR_ATTACHMENT0,
			GLenum attach_to = GL_COLOR_ATTACHMENT0,
			GLenum filter = GL_NEAREST,
			GLbitfield mask = (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
		);

		void blitFrom(
			const Texture& tex, 
			GLenum filter = GL_NEAREST,
			GLenum attach_to = GL_COLOR_ATTACHMENT0
		);

		template<typename T, int N>
		void readBack(Image<T, N>& img, int width, int height, int x, int y, GLenum attach_from = GL_COLOR_ATTACHMENT0) const;

		template<typename T, int N>
		void readBack(Image<T, N>& img, GLenum attach_from = GL_COLOR_ATTACHMENT0) const;

		static void bindDefault(GLenum target = GL_FRAMEBUFFER);

	protected:
		void createBuffer();
		void createDepth(int w, int h);

		std::map<GLenum, Texture> attachments;
		GLptr id, depth_id;
		int _w = 0, _h = 0;
	};



	enum class TextureStatus { EMPTY, MEM_ALLOCATED, DATA_IN_TRANSFERT, DATA_TRANSFERRED, FINISHED };

	class TextureManager {

	public:
			
		enum Flags : uint { ReleaseCPUmemoryAfterGPUupload };

		using Ptr = std::shared_ptr<TextureManager>;

		TextureManager(const std::string & _p, uint flags = ReleaseCPUmemoryAfterGPUupload);
		
		void load_from_disk();

		std::shared_ptr<Texture> getTex();
		std::shared_ptr<Image3b> getImage();

		bool ready() const;

		TextureStatus performNextGPUuploadTask();

		static int tile_size_bytes;

	protected:
		void update_to_gpu();

		std::shared_ptr<Texture> tex_ptr;		
		std::shared_ptr<Image3b> img_ptr;
		std::atomic<bool> img_loaded = false;
		std::string path;
		bool gpu_mem_allocated = false, all_data_transferred = false;
		uint flags;
		int gpu_tile_id = 0;
	};

	class LoaderManager {

	public:
		using Tex = std::shared_ptr<TextureManager>;

		LoaderManager();
		~LoaderManager();

		void addTexture(const Tex & tex);
		void nextGPUtask();
		void release();

		static LoaderManager & getMainLoader();

	protected:		

		static LoaderManager mainLoader;

		std::list<Tex> tex_list_for_decoding, tex_list_for_GPU_upload;

		Tex currentTexDecoding, currentTexUpload;
		std::mutex tex_list_decoding_mutex, tex_list_upload_mutex;
		std::thread loadingThread;
		bool shouldContinue = true;
	};

	template<typename T>
	inline Texture::Texture(const T& t, TexParams params)
		: Texture(params)
	{
		createFromImage(ImageInfos<T>(t));
	}

	template<typename T>
	inline void Texture::createFromImage(const ImageInfos<T>& img)
	{
		allocate(img.w(), img.h(), img.n());
		uploadToGPU(0, 0, _w, _h, img.data());
	}
	
	template<typename T>
	inline void Texture::update(const T& t, TexParams _params)
	{
		ImageInfos<T> infos(t);
		if (w() != infos.w() || h() != infos.h() || n() != infos.n() || getParams() != _params) {
			static_cast<TexParams&>(*this) = _params;
			createGPUid();
			createFromImage(infos);
		} else {	
			check();
			bind();
			uploadToGPU(0, 0, infos.w(), infos.h(), infos.data());
		}
	}

	template<typename T, int N>
	inline void Framebuffer::readBack(Image<T, N>& img, int width, int height, int x, int y, GLenum attach_from) const
	{
		img.allocate(width, height);
		bindRead(attach_from);
		glReadPixels(x, y, width, height, getAttachment(attach_from).format, getAttachment(attach_from).type, img.data());
	}

	template<typename T, int N>
	inline void Framebuffer::readBack(Image<T, N>& img, GLenum attach_from) const
	{
		readBack(img, w(), h(), 0, 0, attach_from);
	}



}
#pragma once

#include "config.hpp"

#include "Input.hpp"
#include "Image.hpp"

#include <list>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <filesystem>
#include <array>

namespace gloops {

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

		static const TexParamsFormat RED, RGB, BGR, RGBA, RGBA32F;

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

		GLint getMagFilter() const {
			return mag_filter;
		}

		GLint getMinFilter() const {
			return min_filter;
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

		GLint getWrapS() const {
			return wrap_s;
		}

		GLint getWrapT() const {
			return wrap_t;
		}

		GLint getWrapR() const {
			return wrap_r;
		}

	protected:
		GLint wrap_s = GL_REPEAT, wrap_t = GL_REPEAT, wrap_r = GL_REPEAT;
		mutable bool dirtyWrap = true;
	};

	class TexParamsSwizzleMask {
	public:
		bool operator==(const TexParamsSwizzleMask& other) const {
			return swizzleMask == other.swizzleMask;
		}
		bool operator!=(const TexParamsSwizzleMask& other) const {
			return !(*this == other);
		}
		const std::array<GLint, 4>& getSwizzleMask() const {
			return swizzleMask;
		}

	protected:
		std::array<GLint, 4> swizzleMask = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
		mutable bool dirtySwizzle = true;
	};

	class TexParamsAlignement {
	public:
		bool operator ==(const TexParamsAlignement& other) const {
			return pack == other.pack && pack && other.unpack;
		}
		bool operator!=(const TexParamsAlignement& other) const {
			return !(*this == other);
		}
	protected:
		GLint pack = 1, unpack = 1;
		mutable bool dirtyAlignment = true;
	};
	class TexParams :
		public TexParamsFormat,
		public TexParamsFilter,
		public TexParamsMipmap,
		public TexParamsWrap,
		public TexParamsAlignement,
		public TexParamsSwizzleMask
	{
	public:
		TexParams() = default;

		TexParams& setTarget(GLenum t);
		TexParams& setInternalFormat(GLenum t);
		TexParams& setFormat(GLenum t);
		TexParams& setType(GLenum t);

		TexParams& setMagFilter(GLint v);
		TexParams& setMinFilter(GLint v);

		TexParams& disableMipmap();
		TexParams& enableMipmap();

		TexParams& setWrapS(GLint parameter);
		TexParams& setWrapT(GLint parameter);
		TexParams& setWrapR(GLint parameter);
		TexParams& setWrapAll(GLint parameter);

		TexParams& setPackAlignment(GLint value);
		TexParams& setUnpackAlignment(GLint value);

		TexParams& setSwizzleMask(const std::array<GLint, 4>& mask);

		TexParams(const TexParamsFormat& format) : TexParamsFormat(format) {
		}

		bool changeRequiresReallocating(const TexParams& other) const
		{
			return
				static_cast<const TexParamsFormat&>(*this) != other ||
				static_cast<const TexParamsAlignement&>(*this) != other ||
				static_cast<const TexParamsMipmap&>(*this) != other;
		}

		bool operator==(const TexParams& other) const {
			return
				static_cast<const TexParamsFormat&>(*this) == other &&
				static_cast<const TexParamsFilter&>(*this) == other &&
				static_cast<const TexParamsMipmap&>(*this) == other &&
				static_cast<const TexParamsWrap&>(*this) == other &&
				static_cast<const TexParamsAlignement&>(*this) == other &&
				static_cast<const TexParamsSwizzleMask&>(*this) == other;
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

	template<>
	struct DefaultTexParams<Image4b> : TexParams {
		DefaultTexParams() {
			internal_format = GL_RGBA8;
			format = GL_RGBA;
		}
	};

	template<>
	struct DefaultTexParams<Image1b> : TexParams {
		DefaultTexParams() {
			internal_format = GL_R8;
			format = GL_RED;
		}
	};

	template<>
	struct DefaultTexParams<Image1f> : TexParams {
		DefaultTexParams() {
			format = GL_RED;
			internal_format = GL_R32F;
			type = GL_FLOAT;
		}
	};

	class Texture : public TexParams {

	public:
		using Ptr = std::shared_ptr<Texture>;

		Texture(const TexParams& params = {});

		Texture(int w, int h, int nchannels, const TexParams& params = {});
		Texture(int w, int h, int l, int nchannels, const TexParams& params = {});

		static Texture fromPath2D(const std::string& img_path, const TexParams& params = {});
		static Texture fromPathCube(const std::string& img_path, const TexParams& params = {});

		template<typename T>
		explicit Texture(const T& t, const TexParams& params = DefaultTexParams<T>{});

		void update(const TexParams& params);

		template<typename T>
		void update2D(const T& t, const TexParams& params = DefaultTexParams<T>{});

		void allocate2D(int width, int height, int nchannels);
		void uploadToGPU2D(int lod, int xoffset, int yoffset, int width, int height, const void* data); 
		
		void allocate3D(int width, int height, int depth, int nchannels);
		void updloadToGPU3D(int lod, int xoffset, int yoffset, int zoffset, int width, int height, int depth, const void* data);

		void allocateCube(int width, int height, int nchannels);
		
		template<typename T>
		void updateCubeFace(const T& t, GLenum face, const TexParams& params = DefaultTexParams<T>{});

		void bind() const;
		void bindSlot(GLuint slot) const;

		int w() const;
		int h() const;
		int n() const;
		int d() const;

		int nLods() const;

		const TexParams& getParams() const;

		//~Texture();

		operator GLuint() const;
		GLuint getId() const;

	
	protected:
		void check() const;

		void setFilter() const;
		void generateMipmaps() const;
		void setWrap() const;
		void setAlignment() const;
		void setSwizzling() const;

		void createGPUid();
		//void releaseGPUmemory();

		template<typename T>
		void createFromImage2D(const ImageInfos<T>& img);

		struct Size {
			int _w, _h, _d = 1, _n, _lods = 1;
		};
		
		std::shared_ptr<Size> size;

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
		void bindDraw(const Viewporti& vp) const;
		void bindDraw(GLenum attachment) const;
		void bindDraw(GLenum attachment, const Viewporti& vp) const;

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
			GLenum attach_to = GL_COLOR_ATTACHMENT0,
			GLenum filter = GL_NEAREST
		);

		template<typename T, int N>
		void readBack(Image<T, N>& img, int width, int height, int x, int y, GLenum attach_from = GL_COLOR_ATTACHMENT0) const;

		template<typename T, int N>
		void readBack(Image<T, N>& img, GLenum attach_from = GL_COLOR_ATTACHMENT0) const;

		static void bindDefault(GLenum target = GL_FRAMEBUFFER);

	protected:
		void createBuffer();
		void createDepth(int w, int h);

		Viewporti viewport() const;

		std::map<GLenum, Texture> attachments;
		GLptr id, depth_id;
		int _w = 0, _h = 0;
	};

	enum class TextureStatus { EMPTY, MEM_ALLOCATED, DATA_IN_TRANSFERT, DATA_TRANSFERRED, FINISHED };

	class TextureManager {

	public:

		enum Flags : uint {
			ReleaseCPUmemoryAfterGPUupload = 1 << 0 
		};

		using Ptr = std::shared_ptr<TextureManager>;

		TextureManager(const std::string& _p, uint flags = ReleaseCPUmemoryAfterGPUupload);

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

		void addTexture(const Tex& tex);
		bool nextGPUtask();
		void release();

		static LoaderManager& getMainLoader();

	protected:

		static LoaderManager mainLoader;

		void startThread();
		void endThread();

		std::list<Tex> tex_list_for_decoding, tex_list_for_GPU_upload;

		Tex currentTexDecoding, currentTexUpload;
		std::mutex tex_list_decoding_mutex, tex_list_upload_mutex;
		std::thread loadingThread;
		std::atomic_bool threadShouldContinue = true;
	};

	template<typename T>
	inline Texture::Texture(const T& t, const TexParams& params)
		: Texture(params)
	{
		createFromImage2D(ImageInfos<T>(t));
	}

	template<typename T>
	inline void Texture::createFromImage2D(const ImageInfos<T>& img)
	{
		allocate2D(img.w(), img.h(), img.n());
		uploadToGPU2D(0, 0, 0, w(), h(), img.data());
	}

	template<typename T>
	inline void Texture::update2D(const T& t, const TexParams& _params)
	{
		ImageInfos<T> infos(t);

		if (w() != infos.w() || h() != infos.h() || n() != infos.n() || changeRequiresReallocating(_params)) {
			createGPUid();
			update(_params);
			createFromImage2D(infos);
		} else {
			update(_params);
			uploadToGPU2D(0, 0, 0, infos.w(), infos.h(), infos.data());
		}
	}

	template<typename T>
	inline void Texture::updateCubeFace(const T& t, GLenum face, const TexParams& _params)
	{
		ImageInfos<T> infos(t);

		TexParams params(_params);
		params.setTarget(GL_TEXTURE_CUBE_MAP);

		if (w() != infos.w() || h() != infos.h() || n() != infos.n() || changeRequiresReallocating(params)) {
			createGPUid();
			allocateCube(w(), h(), n());
		}

		update(params);

		setAlignment();
		glTexSubImage2D(face, 0, 0, 0, w(), h(), format, type, infos.data());
	}

	template<typename T, int N>
	inline void Framebuffer::readBack(Image<T, N>& img, int width, int height, int x, int y, GLenum attach_from) const
	{
		img.resize(width, height);
		bindRead(attach_from);
		glReadPixels(x, y, width, height, getAttachment(attach_from).format, getAttachment(attach_from).type, img.data());
	}

	template<typename T, int N>
	inline void Framebuffer::readBack(Image<T, N>& img, GLenum attach_from) const
	{
		readBack(img, w(), h(), 0, 0, attach_from);
	}



}
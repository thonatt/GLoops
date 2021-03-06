#include "Texture.hpp"
#include "Debug.hpp"
#include "Input.hpp"

#include <iostream>
#include <chrono>

namespace gloops {

	const TexParamsFormat TexParamsFormat::RED = TexParamsFormat(GL_TEXTURE_2D, GL_UNSIGNED_BYTE, GL_R8, GL_RED);
	const TexParamsFormat TexParamsFormat::RGB = TexParamsFormat(GL_TEXTURE_2D, GL_UNSIGNED_BYTE, GL_RGB8, GL_RGB);
	const TexParamsFormat TexParamsFormat::BGR = TexParamsFormat(GL_TEXTURE_2D, GL_UNSIGNED_BYTE, GL_RGB8, GL_BGR);
	const TexParamsFormat TexParamsFormat::RGBA = TexParamsFormat(GL_TEXTURE_2D, GL_UNSIGNED_BYTE, GL_RGBA8, GL_RGBA);
	const TexParamsFormat TexParamsFormat::RED32F = TexParamsFormat(GL_TEXTURE_2D, GL_FLOAT, GL_R32F, GL_RED);
	const TexParamsFormat TexParamsFormat::RGBA32F = TexParamsFormat(GL_TEXTURE_2D, GL_FLOAT, GL_RGBA32F, GL_RGBA);

	Texture::Texture(const TexParams& params)
		: TexParams(params)
	{	
		size = std::make_shared<Size>();
		createGPUid();
	}

	Texture::Texture(int w, int h, int nchannels, const TexParams& params) 
		: Texture(params)
	{
		allocate2D(w, h, nchannels);
	}

	Texture::Texture(int w, int h, int l, int nchannels, const TexParams& params)
		: Texture(params)
	{
		allocate3D(w, h, l, nchannels);
	}

	Texture Texture::fromPath2D(const std::string& img_path, const TexParams& params)
	{
		Image3b img;
		img.load(img_path);
		return Texture(img, params);
	}

	Texture Texture::fromPathCube(const std::string& img_path, const TexParams& _params)
	{
		Image3b img;
		img.load(img_path);

		int w = img.w() / 4;
		int h = img.h() / 3;

		TexParams params(_params);
		params.setTarget(GL_TEXTURE_CUBE_MAP);

		Texture tex(params);
		tex.allocateCube(w, h, img.n());

		static const std::vector<int> faces = { 3, 1, 0, 5, 2, 4 }, offsetsX = { 1, 0, 1, 2, 3, 1 }, offsetsY = { 0, 1, 1, 1, 1, 2 }; 
		for (int i = 0; i < faces.size(); ++i) {
			tex.updateCubeFace(img.subImage(w * offsetsX[faces[i]], h * offsetsY[faces[i]], w, h), GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, params);
		}

		return tex;
	}

	void Texture::update(const TexParams& params)
	{
		static_cast<TexParams&>(*this) = params;
	}

	void Texture::allocate2D(int width, int height, int nchannels)
	{
		(*size)._w = width;
		(*size)._h = height;
		(*size)._n = nchannels;

		if (useMipmap) {
			int max_dim = std::max(w(), h());
			(*size)._lods = static_cast<int>(std::ceil(std::log(max_dim))) + 1;
		}

		bind();
		glTexStorage2D(target, nLods(), internal_format, w(), h());
	}

	void Texture::uploadToGPU2D(int lod, int xoffset, int yoffset, int width, int height, const void * data)
	{
		setAlignment();
		glTexSubImage2D(target, lod, xoffset, yoffset, width, height, format, type, data);
	}

	void Texture::allocate3D(int width, int height, int depth, int nchannels)
	{
		(*size)._w = width;
		(*size)._h = height;
		(*size)._d = depth;
		(*size)._n = nchannels;

		if (useMipmap) {
			int max_dim = std::max(w(), std::max(h(), d()));
			(*size)._lods = static_cast<int>(std::ceil(std::log(max_dim))) + 1;
		}

		bind();
		glTexStorage3D(target, nLods(), internal_format, w(), h(), d());
	}

	void Texture::updloadToGPU3D(int lod, int xoffset, int yoffset, int zoffset, int width, int height, int depth, const void* data)
	{
		setAlignment();
		glTexSubImage3D(target, lod, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
	}

	void Texture::allocateCube(int width, int height, int nchannels)
	{
		allocate2D(width, height, nchannels);
	}

	void Texture::generateMipmaps() const
	{
		bind();
		glGenerateMipmap(target);
		dirtyMipmap = false;
	}

	void Texture::setWrap() const
	{
		bind();
		glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap_s);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap_t);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, wrap_r);
		dirtyWrap = false;
	}

	void Texture::setAlignment() const
	{
		bind();
		glPixelStorei(GL_PACK_ALIGNMENT, pack);
		glPixelStorei(GL_UNPACK_ALIGNMENT, unpack);
		dirtyAlignment = false;
	}

	void Texture::setSwizzling() const
	{
		bind();
		glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask.data());
		dirtySwizzle = false;
	}

	void Texture::bind() const
	{
		glBindTexture(target, id);
	}

	void Texture::bindSlot(GLuint slot) const
	{
		check();
		glActiveTexture(slot);
		bind();
	}

	int Texture::w() const
	{
		return (*size)._w;
	}

	int Texture::h() const
	{
		return (*size)._h;
	}

	int Texture::n() const
	{
		return (*size)._n;
	}

	int Texture::d() const
	{
		return (*size)._d;
	}

	int Texture::nLods() const
	{
		return (*size)._lods;
	}

	const TexParams& Texture::getParams() const
	{
		return static_cast<const TexParams&>(*this);
	}

	Texture::operator GLuint() const
	{
		check();
		return id;
	}

	GLuint Texture::getId() const
	{
		check();
		return id;
	}

	void Texture::setFilter() const
	{
		bind();
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, mag_filter);	
		dirtyFilter = false;
	}

	void Texture::check() const
	{
		if (dirtyFormat) {
			//TODO full gpu memory reset
			dirtyFormat = false;
		}

		if (dirtyFilter) {
			setFilter();
		}
		
		if (dirtyWrap) {
			setWrap();
		}

		if (dirtyAlignment) {
			setAlignment();
		}

		if (dirtySwizzle) {
			setSwizzling();
		}

		if (dirtyMipmap && useMipmap) {
			generateMipmaps();
			dirtyMipmap = false;
		}
	}

	void Texture::createGPUid()
	{
		id = GLptr(
			[](GLuint* ptr) { glGenTextures(1, ptr); },
			[](const GLuint* ptr) { glDeleteTextures(1, ptr); }
		);
	}

	Framebuffer::Framebuffer() {
		createBuffer();
	}

	Framebuffer::Framebuffer(int width, int height, int numChannels, const TexParams& params, int numAttachments)
		: _w(width), _h(height)
	{

		createBuffer();
		createDepth(_w, _h);

		if (numAttachments > maxColorAttachments()) {
			std::cout << "trying to setup " << numAttachments << " color attachments, " << maxColorAttachments() << " used instead" << std::endl;
			numAttachments = maxColorAttachments();
		}

		for (int i = 0; i < numAttachments; ++i) {
			Texture tex = Texture(w(), h(), numChannels, params);
			setAttachment(tex, GL_COLOR_ATTACHMENT0 + i, 0);
		}

		gl_framebuffer_check(GL_FRAMEBUFFER);
	}

	void Framebuffer::resize(int width, int height)
	{
		if (w() == width && h() == height) {
			return;
		}

		_w = width;
		_h = height;

		createDepth(_w, _h);

		for (auto& attachment : attachments) {
			const auto& currentTex = attachment.second;	
			Texture tex = Texture(w(), h(), currentTex.n(), currentTex.getParams());
			setAttachment(tex, attachment.first, 0);
		}

		gl_framebuffer_check(GL_FRAMEBUFFER);
	}

	Framebuffer Framebuffer::empty(int w, int h)
	{
		Framebuffer out;
		out._w = w;
		out._h = h;
		return out;
	}

	void Framebuffer::setAttachment(const Texture& tex, GLenum attachment, GLint level)
	{
		bind();
		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, tex.getParams().target, tex.getId(), level);
		attachments[attachment] = tex;
	}

	void Framebuffer::bind(GLenum target) const
	{
		glBindFramebuffer(target, id);
	}

	void Framebuffer::bindDraw() const
	{
		bindDraw(viewport());
	}

	void Framebuffer::bindDraw(const Viewporti& vp) const
	{
		if (attachments.size() > maxDrawBuffers()) {
			std::cout << "trying to draw " << attachments.size() << " buffers, " << maxDrawBuffers() << " used instead" << std::endl;
		}

		bind();
		int count = 0;
		for (const auto& attachment : attachments) {
			if (count == maxDrawBuffers()) {
				break;
			}
			glDrawBuffers(1, &attachment.first);
			++count;		
		}
		vp.gl();
	}

	void Framebuffer::bindDraw(GLenum attachment) const
	{
		bindDraw(attachment, viewport());
	}

	void Framebuffer::bindDraw(GLenum attachment, const Viewporti& vp) const
	{
		bind();
		glDrawBuffers(1, &attachment);
		vp.gl();
	}

	void Framebuffer::bindRead(GLenum attachment) const
	{
		bind(GL_READ_FRAMEBUFFER);
		glReadBuffer(attachment);
	}

	void Framebuffer::clear(const v4f& c, GLbitfield mask)
	{
		bind();
		glClearColor(c[0], c[1], c[2], c[3]);
		glClear(mask);
	}

	GLuint Framebuffer::getId() const
	{
		return id;
	}

	const Texture& Framebuffer::getAttachment(GLenum attachment) const
	{
		return attachments.at(attachment);
	}

	Texture& Framebuffer::getAttachment(GLenum attachment) 
	{
		return attachments.at(attachment);
	}

	int Framebuffer::w() const
	{
		return _w;
	}

	int Framebuffer::h() const
	{
		return _h;
	}

	void Framebuffer::blitFrom(const Framebuffer& src, GLenum attach_from, GLenum attach_to, GLenum filter, GLbitfield mask)
	{
		bindDraw(attach_to);	
		src.bindRead(attach_from);

		glBlitFramebuffer(0, 0, src.w(), src.h(), 0, 0, w(), h(), mask, filter);
	}

	void Framebuffer::blitFrom(const Texture& tex, GLenum attach_to, GLenum filter)
	{
		Framebuffer src = Framebuffer::empty(tex.w(), tex.h());
		src.bind(GL_READ_FRAMEBUFFER);
		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex.getParams().target, tex.getId(), 0);

		blitFrom(src, GL_COLOR_ATTACHMENT0, attach_to, filter, GL_COLOR_BUFFER_BIT);
	}

	void Framebuffer::bindDefault(GLenum target)
	{
		glBindFramebuffer(target, 0);
	}

	GLint Framebuffer::maxDrawBuffers()
	{
		static GLint maxDrawBuffers = [] {
			GLint _maxDrawBuffers = 0;
			glGetIntegerv(GL_MAX_DRAW_BUFFERS, &_maxDrawBuffers);
			return _maxDrawBuffers;
		}();

		return maxDrawBuffers;
	}

	GLint Framebuffer::maxColorAttachments()
	{
		static GLint maxAttachments = [] {
			GLint _maxAttachments = 0;
			glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &_maxAttachments);
			return _maxAttachments;
		}();

		return maxAttachments;
	}

	void Framebuffer::createBuffer()
	{
		id = GLptr(
			[](GLuint* ptr) { glGenFramebuffers(1, ptr); },
			[](const GLuint* ptr) { glDeleteFramebuffers(1, ptr); }
		);
	}

	void Framebuffer::createDepth(int w, int h)
	{
		depth_id = GLptr(
			[](GLuint* ptr) { glGenRenderbuffers(1, ptr); },
			[](const GLuint* ptr) { glDeleteRenderbuffers(1, ptr); }
		);

		glBindRenderbuffer(GL_RENDERBUFFER, depth_id);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, w, h);

		bind();
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_id);
	}

	Viewporti Framebuffer::viewport() const
	{
		return Viewporti(v2i(0, 0), v2i(w(), h()));
	}

	LoaderManager LoaderManager::mainLoader = {};

	LoaderManager::LoaderManager()
	{
	}

	LoaderManager::~LoaderManager()
	{
		release();
	}

	void LoaderManager::addTexture(const Tex & tex)
	{
		if (!loadingThread.joinable()) {
			startThread();
		}
		std::lock_guard<std::mutex> guard(tex_list_decoding_mutex);
		tex_list_for_decoding.push_back(tex);
	}

	bool LoaderManager::nextGPUtask()
	{
		if(!currentTexUpload){
			std::lock_guard<std::mutex> guard(tex_list_upload_mutex);
			if (tex_list_for_GPU_upload.empty()) {
				return threadShouldContinue;
			}
			currentTexUpload = tex_list_for_GPU_upload.front();
			tex_list_for_GPU_upload.pop_front();
		}

		if (currentTexUpload->performNextGPUuploadTask() == TextureStatus::FINISHED) {
			currentTexUpload.reset();
		}

		std::lock_guard<std::mutex> guardLoad(tex_list_decoding_mutex), guardUpload(tex_list_upload_mutex);
		if (tex_list_for_decoding.empty() && tex_list_for_GPU_upload.empty() && !currentTexDecoding && !currentTexUpload ) {
			std::cout << " no more jobs " << std::endl;
			threadShouldContinue = false;
			//endThread();
		}

		return threadShouldContinue;
	}

	void LoaderManager::release()
	{
		endThread();
	}

	LoaderManager & LoaderManager::getMainLoader()
	{
		return mainLoader;
	}

	void LoaderManager::startThread()
	{
		std::cout << "start thread" << std::endl;

		threadShouldContinue = true;

		loadingThread = std::thread([&] {

			//std::this_thread::sleep_for(std::chrono::milliseconds(10000));
			while (threadShouldContinue) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				{
					std::lock_guard<std::mutex> guard(tex_list_decoding_mutex);
					if (tex_list_for_decoding.empty()) {
						continue;
					}
					currentTexDecoding = tex_list_for_decoding.front();
					tex_list_for_decoding.pop_front();
				}

				currentTexDecoding->load_from_disk();

				std::lock_guard<std::mutex> guard(tex_list_upload_mutex);
				tex_list_for_GPU_upload.push_back(currentTexDecoding);
				currentTexDecoding.reset();
				//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}

			std::cout << " end thread " << std::endl;
		});
	}

	void LoaderManager::endThread()
	{
		threadShouldContinue = false;
		if (loadingThread.joinable()) {
			loadingThread.join();
		}
	}

	int TextureManager::tile_size_bytes = 2048 * 2048 * 3;
	
	TextureManager::TextureManager(const std::string & _p, uint _flags)
		: path(_p), flags(_flags)
	{	
	}

	void TextureManager::load_from_disk()
	{
		img_ptr.reset(new Image3b());
		img_ptr->load(path);
		img_loaded = true;
	}

	void TextureManager::update_to_gpu()
	{
		if (img_loaded) {
			tex_ptr.reset(new Texture(*img_ptr));
			img_ptr.reset();		
		}
	}

	std::shared_ptr<Texture> TextureManager::getTex()
	{
		using namespace std::chrono;

		if (!ready()) {
			return {};
		} 

		return tex_ptr;
	}

	std::shared_ptr<Image3b> TextureManager::getImage()
	{
		if (img_loaded) {
			return img_ptr;
		}

		return{};
	}

	bool TextureManager::ready() const
	{
		return all_data_transferred;
	}

	TextureStatus TextureManager::performNextGPUuploadTask()
	{
		if (!img_ptr) {
			return TextureStatus::EMPTY;
		}

		if (!tex_ptr) {
			tex_ptr = std::make_shared<Texture>();
		}

		if (!gpu_mem_allocated) {
			tex_ptr->allocate2D(img_ptr->w(), img_ptr->h(), img_ptr->n());
			gpu_tile_id = 0;
			gpu_mem_allocated = true;
			return TextureStatus::MEM_ALLOCATED;
		} 

		if (!all_data_transferred) {
			const int bytes_per_row = tex_ptr->w()*tex_ptr->n();
			const int rows_per_tile = std::max(1, tile_size_bytes / bytes_per_row);
			const int num_tiles = (tex_ptr->h() - 1) / rows_per_tile + 1;
			
			const int offset_i = gpu_tile_id * rows_per_tile;
			const int tilesize_i = std::min(rows_per_tile, tex_ptr->h() - offset_i);
			const void* data = img_ptr->data() + rows_per_tile * static_cast<size_t>(bytes_per_row)* gpu_tile_id;
			
			tex_ptr->uploadToGPU2D(0, 0, offset_i, tex_ptr->w(), tilesize_i, data);

			++gpu_tile_id;
			if (gpu_tile_id == num_tiles) {
				all_data_transferred = true;
				return TextureStatus::DATA_TRANSFERRED;
			} else {			
				return TextureStatus::DATA_IN_TRANSFERT;
			}		
		}

		if (img_ptr && (flags & ReleaseCPUmemoryAfterGPUupload)) {
			img_ptr.reset();
		}
		
		return TextureStatus::FINISHED;
	}


	TexParams& TexParams::setTarget(GLenum t)
	{
		target = t;
		dirtyFormat = true;
		return *this;
	}

	TexParams& TexParams::setInternalFormat(GLenum t)
	{
		internal_format = t;
		dirtyFormat = true;
		return *this;
	}

	TexParams& TexParams::setFormat(GLenum t)
	{
		format = t;
		dirtyFormat = true;
		return *this;
	}

	TexParams& TexParams::setType(GLenum t)
	{
		type = t;
		dirtyFormat = true;
		return *this;
	}

	TexParams& TexParams::setMagFilter(GLint v)
	{
		mag_filter = v;
		dirtyFilter = true;
		return *this;
	}

	TexParams& TexParams::setMinFilter(GLint v)
	{
		min_filter = v;
		dirtyFilter = true;
		return *this;
	}

	TexParams& TexParams::disableMipmap()
	{
		useMipmap = false;
		return *this;
	}

	TexParams& TexParams::enableMipmap()
	{
		useMipmap = true;
		return *this;
	}

	TexParams& TexParams::setWrapS(GLint parameter)
	{
		wrap_s = parameter;
		dirtyWrap = true;
		return *this;
	}

	TexParams& TexParams::setWrapT(GLint parameter)
	{
		wrap_t = parameter;
		dirtyWrap = true;
		return *this;
	}

	TexParams& TexParams::setWrapR(GLint parameter)
	{
		wrap_r = parameter;
		dirtyWrap = true;
		return *this;
	}

	TexParams& TexParams::setWrapAll(GLint parameter)
	{
		wrap_r = parameter;
		wrap_s = parameter;
		wrap_t = parameter;
		dirtyWrap = true;
		return *this;
	}

	TexParams& TexParams::setPackAlignment(GLint value)
	{
		pack = value;
		dirtyAlignment = true;
		return *this;
	}

	TexParams& TexParams::setUnpackAlignment(GLint value)
	{
		unpack = value;
		dirtyAlignment = true;
		return *this;
	}

	TexParams& TexParams::setSwizzleMask(const std::array<GLint, 4>& mask)
	{
		swizzleMask = mask;
		dirtySwizzle = true;
		return *this;
	}

}
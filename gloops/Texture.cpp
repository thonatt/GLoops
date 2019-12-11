#include "Texture.hpp"
#include "Debug.hpp"


#define STB_IMAGE_IMPLEMENTATION
#include "../extlibs/stb_image/stb_image.h"

#include <iostream>
#include <chrono>

namespace gloops {

	const TexParamsFormat TexParamsFormat::RGB = TexParamsFormat(GL_TEXTURE_2D, GL_UNSIGNED_BYTE, GL_RGB8, GL_RGB);
	const TexParamsFormat TexParamsFormat::BGR = TexParamsFormat(GL_TEXTURE_2D, GL_UNSIGNED_BYTE, GL_RGB8, GL_BGR);
	const TexParamsFormat TexParamsFormat::RGBA = TexParamsFormat(GL_TEXTURE_2D, GL_UNSIGNED_BYTE, GL_RGBA8, GL_RGBA);
	const TexParamsFormat TexParamsFormat::RGBA32F = TexParamsFormat(GL_TEXTURE_2D, GL_FLOAT, GL_RGBA32F, GL_RGBA);

	Texture::Texture(TexParams params) : TexParams(params)
	{
		createGPUid();
	}

	Texture::Texture(int w, int h, int nchannels, TexParams params) : TexParams(params)
	{
		createGPUid();
		allocate(w, h, nchannels);
	}

	Texture Texture::fromPath(const std::string& img_path, TexParams params)
	{
		Image3b img;
		img.load(img_path);
		return Texture(img, params);
	}

	void Texture::allocate(int width, int height, int nchannels)
	{
		_w = width;
		_h = height;
		_n = nchannels;

		const int num_lvls_estimate = useMipmap ? static_cast<int>(std::ceil(std::log(std::max(_w, _h)))) + 1 : 1;

		bind();
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		//setFilter();

		//std::cout << " ALLOCATE " << id << " : " << _w << " " << _h << " " << _n << " " << num_lvls_estimate << std::endl;

		gl_check();
		glTexStorage2D(target, num_lvls_estimate, internal_format, _w, _h);
		gl_check();
	}

	void Texture::uploadToGPU(int xoffset, int yoffset, int width, int height, const void * data)
	{
		bind();
		glTexSubImage2D(target, 0, xoffset, yoffset, width, height, format, type, data);
	}

	void Texture::generateMipmaps() const
	{
		bind();
		glGenerateMipmap(target);
	}

	void Texture::setWrap() const
	{
		glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap_s);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap_t);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, wrap_r);
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
		return _w;
	}

	int Texture::h() const
	{
		return _h;
	}

	int Texture::n() const
	{
		return _n;
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
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, mag_filter);	
	}

	void Texture::check() const
	{
		if (dirtyFormat) {
			//TODO full gpu memory reset
			dirtyFormat = false;
		}

		if (dirtyFilter) {
			bind();
			setFilter();
			dirtyFilter = false;
		}
		
		if (dirtyWrap) {
			bind();
			setWrap();
			dirtyWrap = false;
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

	Framebuffer::Framebuffer(int w, int h, int n, const TexParams& params, int numAttachments)
		: _w(w), _h(h)
	{

		createBuffer();
		createDepth(_w, _h);

		for (int i = 0; i < numAttachments; ++i) {
			Texture tex = Texture(_w, _h, n, params);
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

		//releaseDepth();
		createDepth(_w, _h);

		gl_check();

		for (auto& attachment : attachments) {
			const auto& currentTex = attachment.second;
			
			Texture tex = Texture(_w, _h, currentTex.n(), currentTex.getParams());
			gl_check();
			setAttachment(tex, attachment.first, 0);
			gl_check();
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
		bind();
		for (const auto& attachment : attachments) {
			glDrawBuffers(1, &attachment.first);
		}
		glViewport(0, 0, w(), h());
	}

	void Framebuffer::bindDraw(GLenum attachment) const
	{
		bind();
		glDrawBuffers(1, &attachment);
		glViewport(0, 0, w(), h());
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
		gl_check();

		bindDraw(attach_to);
	
		src.bindRead(attach_from);

		//gl_framebuffer_check(GL_READ_FRAMEBUFFER);
		//gl_framebuffer_check(GL_DRAW_FRAMEBUFFER);

		//std::cout << "blit : " << src.w() << " " << src.h() << " " << w() << " " << h() << std::endl;

		glBlitFramebuffer(0, 0, src.w(), src.h(),
			0, 0, w(), h(), mask, filter);

		gl_check();
		//gl_framebuffer_check(GL_READ_FRAMEBUFFER);
		//gl_framebuffer_check(GL_DRAW_FRAMEBUFFER);
	}

	void Framebuffer::blitFrom(const Texture& tex, GLenum filter, GLenum attach_to)
	{
		Framebuffer src = Framebuffer::empty(tex.w(), tex.h());
		src.bind(GL_READ_FRAMEBUFFER);
		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex.getParams().target, tex.getId(), 0);
		
		//gl_framebuffer_check(GL_READ_FRAMEBUFFER);

		blitFrom(src, GL_COLOR_ATTACHMENT0, attach_to, filter, GL_COLOR_BUFFER_BIT);
	}

	void Framebuffer::bindDefault(GLenum target)
	{
		glBindFramebuffer(target, 0);
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

		gl_check();

		glBindRenderbuffer(GL_RENDERBUFFER, depth_id);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, w, h);

		gl_check();

		bind();
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_id);

		gl_check();
	}

	LoaderManager LoaderManager::mainLoader = {};

	LoaderManager::LoaderManager()
	{
		loadingThread = std::thread([&] {
		
			//std::this_thread::sleep_for(std::chrono::milliseconds(10000));
			while (shouldContinue) {		
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
				//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		});
	}

	LoaderManager::~LoaderManager()
	{
		release();
	}

	void LoaderManager::addTexture(const Tex & tex)
	{
		std::lock_guard<std::mutex> guard(tex_list_decoding_mutex);
		tex_list_for_decoding.push_back(tex);
	}

	void LoaderManager::nextGPUtask()
	{
		if(!currentTexUpload){
			std::lock_guard<std::mutex> guard(tex_list_upload_mutex);
			if (tex_list_for_GPU_upload.empty()) {
				return;
			}
			currentTexUpload = tex_list_for_GPU_upload.front();
			tex_list_for_GPU_upload.pop_front();
		}

		if (currentTexUpload->performNextGPUuploadTask() == TextureStatus::FINISHED) {
			currentTexUpload.reset();
		}
	}

	void LoaderManager::release()
	{
		if (shouldContinue) {
			shouldContinue = false;
			loadingThread.join();
		}	
	}

	LoaderManager & LoaderManager::getMainLoader()
	{
		return mainLoader;
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

		//std::cout << path << " loaded" << std::endl;
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
			tex_ptr->allocate(img_ptr->w(), img_ptr->h(), img_ptr->n());
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
			
			tex_ptr->uploadToGPU(0, offset_i, tex_ptr->w(), tilesize_i, data);

			++gpu_tile_id;
			if (gpu_tile_id == num_tiles) {
				all_data_transferred = true;
				return TextureStatus::DATA_TRANSFERRED;
			} else {			
				return TextureStatus::DATA_IN_TRANSFERT;
			}		
		}

		if (img_ptr && flags & ReleaseCPUmemoryAfterGPUupload) {
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

	TexParams& TexParams::setMipmapStatus(bool enabled)
	{
		useMipmap = enabled;
		dirtyMipmap = true;
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

}
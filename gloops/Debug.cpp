#include "Debug.hpp"
#include <map>

namespace gloops {

	void gl_check()
	{
		static const std::map<GLenum, std::string> errors = {
				{ GL_INVALID_ENUM, "GL_INVALID_ENUM"},
				{ GL_INVALID_VALUE, "GL_INVALID_VALUE"},
				{ GL_INVALID_OPERATION, "GL_INVALID_OPERATION"},
				{ GL_STACK_OVERFLOW, "GL_STACK_OVERFLOW"},
				{ GL_STACK_UNDERFLOW, "GL_STACK_UNDERFLOW"},
				{ GL_OUT_OF_MEMORY, "GL_OUT_OF_MEMORY"},
				{ GL_INVALID_FRAMEBUFFER_OPERATION, "GL_INVALID_FRAMEBUFFER_OPERATION"}
		};

		GLenum gl_err = glGetError();
		if (gl_err) {
			std::string err;
			if (errors.count(gl_err)) {
				err = errors.at(gl_err);
			} else {		
				err = "UNKNOWN_GL_ERROR";
			}
			std::cout << err << std::endl;
			throw std::runtime_error("");
		}

	}

	void gl_framebuffer_check(GLenum target)
	{
		static const std::map<GLenum, std::string> errors = {
			{ GL_FRAMEBUFFER_UNDEFINED, "GL_FRAMEBUFFER_UNDEFINED" },
			{ GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT" },
			{ GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT, "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT" },
			{ GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER, "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER" },
			{ GL_FRAMEBUFFER_UNSUPPORTED, "GL_FRAMEBUFFER_UNSUPPORTED" },
			{ GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE, "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE" },
			{ GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS, "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS" }
		};

		GLenum status = glCheckFramebufferStatus(target);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			std::string err;
			if (errors.count(status)) {
				err = errors.at(status);
			} else {
				err = "UNKNOWN_FRAMEBUFFER_ERROR";
			}
			std::cout << err << std::endl;
			throw std::runtime_error("");
		}
	}

}
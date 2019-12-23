#include "Debug.hpp"
#include <map>

#define GLOOPS_ENUM_STR(name) { name, #name }

namespace gloops {

	void gl_check()
	{
		static const std::map<GLenum, std::string> errors = {
			GLOOPS_ENUM_STR(GL_INVALID_ENUM), 
			GLOOPS_ENUM_STR(GL_INVALID_VALUE),
			GLOOPS_ENUM_STR(GL_INVALID_OPERATION),
			GLOOPS_ENUM_STR(GL_STACK_OVERFLOW),
			GLOOPS_ENUM_STR(GL_STACK_UNDERFLOW),
			GLOOPS_ENUM_STR(GL_OUT_OF_MEMORY),
			GLOOPS_ENUM_STR(GL_INVALID_FRAMEBUFFER_OPERATION)
		};

		//return;

		GLenum gl_err = glGetError();
		if (gl_err) {
			std::string err;
			auto it = errors.find(gl_err);
			if (it != errors.end()) {
				err = it->second;
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
			GLOOPS_ENUM_STR(GL_FRAMEBUFFER_UNDEFINED),
			GLOOPS_ENUM_STR(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
			GLOOPS_ENUM_STR(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
			GLOOPS_ENUM_STR(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER),
			GLOOPS_ENUM_STR(GL_FRAMEBUFFER_UNSUPPORTED),
			GLOOPS_ENUM_STR(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE),
			GLOOPS_ENUM_STR(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)
		};

		GLenum status = glCheckFramebufferStatus(target);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			std::string err;
			auto it = errors.find(status);
			if (it != errors.end()) {
				err = it->second;
			} else {
				err = "UNKNOWN_FRAMEBUFFER_ERROR";
			}
			std::cout << err << std::endl;
			throw std::runtime_error("");
		}
	}

}
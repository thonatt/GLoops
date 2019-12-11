#pragma once

#include "config.hpp"

namespace gloops {
	
	void gl_check();

	void gl_framebuffer_check(GLenum target = GL_FRAMEBUFFER);

}
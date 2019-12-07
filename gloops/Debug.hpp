#pragma once

#include "Config.hpp"

namespace gloops {
	
	void gl_check();

	void gl_framebuffer_check(GLenum target = GL_FRAMEBUFFER);

}
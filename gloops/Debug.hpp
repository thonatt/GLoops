#pragma once

#include "config.hpp"

#include <list>
#include <map>

namespace gloops {
	
	void gl_check();

	void gl_framebuffer_check(GLenum target = GL_FRAMEBUFFER);

	enum class LogType { LOG, SUCCESS, WARNING, ERROR };

	struct GLDebugMessage {

		std::string log;
		double time;
		LogType type;
	};


	class Window;
	class WindowComponent;

	class GLDebugLogs {

	public:
		GLDebugLogs();

		void addLog(LogType type, const std::string& log);

		void show(const Window& win);

	protected:
		void display();

		std::shared_ptr<WindowComponent> logWindow;
		std::vector<GLDebugMessage> logs;
	
		bool scrollToBottom = false;

		static std::map<LogType, v3f> logColors;
	};

	GLDebugLogs& getDebugLogs();

	void addToLogs(LogType type, const std::string& log);

	void glErrorCallBack(GLenum source, GLenum type, GLuint id, GLenum severity,
		GLsizei length, const GLchar* message, const void* userParam);


}
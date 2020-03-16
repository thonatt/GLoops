#include "Debug.hpp"
#include "Window.hpp"

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

		return;

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

	std::map<LogType, v3f> GLDebugLogs::logColors = {
		{ LogType::LOG, v3f(1,1,1) },
		{ LogType::SUCCESS, v3f(0,1,0) },
		{ LogType::WARNING, v3f(1,0.5,0) },
		{ LogType::ERROR, v3f(1,0,0) }
	};

	GLDebugLogs& getDebugLogs()
	{
		static GLDebugLogs logs;
		return logs;
	}

	void addToLogs(LogType type, const std::string& log)
	{
		getDebugLogs().addLog(type, log);
	}

	GLDebugLogs::GLDebugLogs()
	{
		logWindow = std::make_shared<WindowComponent>("logs", WindowComponent::Type::FLOATING, [&](const Window&) {
			display();
		});
	}

	void GLDebugLogs::addLog(LogType type, const std::string& log)
	{
		logs.push_back(GLDebugMessage{ log, ImGui::GetTime(), type });
		scrollToBottom = true;
	}

	void GLDebugLogs::show(const Window& win)
	{
		logWindow->show(win);
	}

	void GLDebugLogs::display()
	{
		ImGui::Separator();

		const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();	
		ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

		ImGuiListClipper clipper((int)logs.size());
		while (clipper.Step()) {
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
				const auto& log = logs[i];
				std::stringstream s;
				s << "[" << log.time << "] " << log.log;
				ImGui::TextColored(s.str(), logColors.at(log.type));
			}
		}

		if (scrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
			ImGui::SetScrollHereY(1.0f);
		}

		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::Separator();
	}

	void glErrorCallBack(GLenum source, GLenum type, GLuint id, GLenum severity,
		GLsizei length, const GLchar* message, const void* userParam)
	{

#define GLOOPS_ENUM_STR(name) { name, #name }

		static const std::map<GLenum, std::string> errors = {
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_ERROR),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_PORTABILITY),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_PERFORMANCE),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_MARKER),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_PUSH_GROUP),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_POP_GROUP),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_OTHER),
		};

		static const std::map<GLenum, std::string> sources_str = {
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_API),
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_WINDOW_SYSTEM),
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_SHADER_COMPILER),
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_THIRD_PARTY),
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_APPLICATION),
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_OTHER)
		};

#undef GLOOPS_ENUM_STR

		enum class SeveriyLevel : uint { NOTIFICATION = 0, LOW = 1, MEDIUM = 2, HIGH = 3, UNKNOWN = 4 };
		struct GLseverity {
			std::string str;
			SeveriyLevel lvl = SeveriyLevel::UNKNOWN;
		};

#define GLOOPS_SEV_STR(name,sev) { name, { #name, SeveriyLevel::sev } }

		static const std::map<GLenum, GLseverity> severities = {
			GLOOPS_SEV_STR(GL_DEBUG_SEVERITY_HIGH, HIGH),
			GLOOPS_SEV_STR(GL_DEBUG_SEVERITY_MEDIUM, MEDIUM),
			GLOOPS_SEV_STR(GL_DEBUG_SEVERITY_LOW, LOW),
			GLOOPS_SEV_STR(GL_DEBUG_SEVERITY_NOTIFICATION, NOTIFICATION),
		};

#undef GLOOPS_SEV_STR

		GLseverity glSev;
		auto sev_it = severities.find(severity);
		if (sev_it != severities.end()) {
			glSev = sev_it->second;
		} else {
			glSev.str = "UNKOWN SEVERITY";
			glSev.lvl = SeveriyLevel::UNKNOWN;
		}

		if ((uint)glSev.lvl < (uint)SeveriyLevel::LOW) {
			return;
		}

		std::string err_str;
		auto err_it = errors.find(type);
		if (err_it != errors.end()) {
			err_str = err_it->second;
		} else {
			err_str = "UNKNOWN GL ERROR";
		}

		std::stringstream s;
		s << sources_str.at(source) << " " << glSev.str << " " << err_str << "\n\t" << message << std::endl;

		if ((uint)glSev.lvl == (uint)SeveriyLevel::HIGH) {
			addToLogs(LogType::ERROR, s.str());
		} else {
			addToLogs(LogType::WARNING, s.str());
		}

	}
}
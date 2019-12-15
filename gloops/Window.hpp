#pragma once

#include "config.hpp"
#include "Debug.hpp"
#include "Input.hpp"
#include "Texture.hpp"

#include <string>
#include <functional>

namespace gloops {

	//helpers to get C pointer function form capturing lambda

	template<typename F>
	struct CfuncPointer;

	template< typename F, typename ReturnType, typename... Args>
	struct CfuncPointer< ReturnType(F::*)(Args...) const> {
	
		using f_ptr = ReturnType(*)(Args...);

		static f_ptr get(F && f) {
			static F f_internal = std::forward<F>(f);
			return [](Args... args) {
				return f_internal(std::forward<Args>(args)...);
			};
		}
	};

	template<typename F>
	auto getCfuncPtr(F && f){
		return CfuncPointer<decltype(&F::operator())>::get(std::forward<F>(f));
	}

	///////////////////////////////////////////////////////////////////////////////////////

	class Framebuffer;

	class Window : public Input {

	public:

		Window(const std::string & name = "name");

		~Window();	 

		void showImguiDemo();

		void pollEvents();
		void swapBuffers();

		int shouldClose() const;
		void close();

		void clear();

		const v2i & winSize() const;

		void bind() const;

		void displayFramebuffer(const Framebuffer & src);

		void renderingLoop(const std::function<void()>& renderingFunc);

	protected:

		void keyboardCallback(GLFWwindow * win, int key, int scancode, int action, int mods);
		void mouseButtonCallback(GLFWwindow * win, int button, int action, int mods);
		void mouseScrollCallback(GLFWwindow * win, double x, double y);
		void mousePositionCallback(GLFWwindow * win, double x, double y);
		void winResizeCallback(GLFWwindow * win, int w, int h);

		template<typename GLFWfun, typename Lambda>
		void setupGLFWcallback(const GLFWfun& glfwFun, Lambda&& f) {
			glfwFun(window.get(), getCfuncPtr(std::forward<Lambda>(f)));
		}

		static void glErrorCallBack(GLenum source, GLenum type, GLuint id, GLenum severity, 
			GLsizei length, const GLchar* message, const void* userParam);

		std::shared_ptr<GLFWwindow> window;
		v2i size;
	};

	class SubWindow : public Input {

	public:
		using UpdateFunc = std::function<void(const Input&)>;
		using RenderingFunc = std::function<void(Framebuffer&)>;
		using GuiFunc = std::function<void(void)>;

		SubWindow(const std::string& name, const v2i& renderingSize,
			GuiFunc guiFunction = {}, UpdateFunc upFunc = {}, RenderingFunc renderFunc = {});

		void show(const Window & win);

	private:
		void fitContent(v2f& outOffset, v2f& outSize, const v2f& vpSize, const v2f& availableSize);
		std::string gui_text(const std::string& str) const;

		static const float ImGuiTitleHeight();

		Framebuffer framebuffer;
		GuiFunc guiFunc; 
		UpdateFunc updateFunc;
		RenderingFunc renderingFunc;
		std::string win_name;
		v2f gui_render_size;
		bool inFocus = false, shouldUpdate = false, showGui = true, showInput = false, showDebug = false;
	};

}
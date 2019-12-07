#pragma once

#include "config.hpp"
#include "Debug.hpp"
#include "Input.hpp"
#include "Texture.hpp"

#include <string>
#include <functional>

namespace gloops {

	enum CallbackType { MOUSE_POSITION, MOUSE_SCROLL, MOUSE_BUTTON, KEY_BUTTON, WIN_RESIZE };

	template <CallbackType type, typename T>
	struct CallbackHelper;

	template <CallbackType type, typename R, typename... Params>
	struct CallbackHelper<type, R(Params...)> {
		
		template <typename... Args>
		static R callback(Args... args) {
			func(args...);
		}
		static std::function<R(Params...)> func;
	};

	template<CallbackType type, typename R, typename... Params>
	std::function<R(Params...)> CallbackHelper<type, R(Params...)>::func;


	template <CallbackType type>
	struct GLFWcallbackSignature;

	template<> struct GLFWcallbackSignature<MOUSE_POSITION> {
		using Helper = CallbackHelper<MOUSE_POSITION, void(GLFWwindow*, double, double)>;
		template<typename F> static void setup(GLFWwindow * win, F && f) {
			Helper::func = f;
			glfwSetCursorPosCallback(win, Helper::callback);
		}
	};

	template<> struct GLFWcallbackSignature<MOUSE_SCROLL> {
		using Helper = CallbackHelper<MOUSE_SCROLL, void(GLFWwindow*, double, double)>;
		template<typename F> static void setup(GLFWwindow * win, F && f) {
			Helper::func = f;
			glfwSetScrollCallback(win, Helper::callback);
		}
	};
	template<> struct GLFWcallbackSignature<MOUSE_BUTTON> {
		using Helper = CallbackHelper<MOUSE_BUTTON, void(GLFWwindow*, int, int, int)>;
		template<typename F> static void setup(GLFWwindow * win, F && f) {
			Helper::func = f;
			glfwSetMouseButtonCallback(win, Helper::callback);
		}
	};
	template<> struct GLFWcallbackSignature<KEY_BUTTON> {
		using Helper = CallbackHelper<KEY_BUTTON, void(GLFWwindow*, int, int, int, int)>;
		template<typename F> static void setup(GLFWwindow * win, F && f) {
			Helper::func = f;
			glfwSetKeyCallback(win, Helper::callback);
		}
	};
	template<> struct GLFWcallbackSignature<WIN_RESIZE> {
		using Helper = CallbackHelper<WIN_RESIZE, void(GLFWwindow*, int, int)>;
		template<typename F> static void setup(GLFWwindow * win, F && f) {
			Helper::func = f;
			glfwSetWindowSizeCallback(win, Helper::callback);
		}
	};

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

		template<CallbackType type, typename F>
		void setCallback(F && f) {
			GLFWcallbackSignature<type>::setup(window, std::forward<F>(f));
		}

		GLFWwindow * window = nullptr;
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
		v2i gui_render_size;
		bool inFocus = false, shouldUpdate = false, showGui = true, showInput = false, showDebug = false;
	};

}
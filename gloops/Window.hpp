#pragma once

#include "config.hpp"
#include "Debug.hpp"
#include "Input.hpp"
#include "Texture.hpp"

#include <string>
#include <functional>
#include <set>

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

	class Window;

	class WindowComponent : public Viewport
	{

	public:
		enum class Type { RENDERING, GUI, DEBUG };

		using Func = std::function<void(const Window&)>;

		WindowComponent(const std::string& name = "", Type type = Type::RENDERING, const Func& guiFunc = {});

		void show(const Window& win);
		bool& isActive();

		void resize(const Viewport& vp);
		const std::string& name() const;

		//const Viewport& viewport() const;

		bool isInFocus() const;
		Type getType() const;

		v4f backgroundColor = { 0,0,0,1 };

	protected:

		std::string _name;
		Func _guiFunc;
		Type _type = Type::RENDERING;
		bool _active = true, shouldResize = false, inFocus = false;
	};


	///////////////////////////////////////////////////////////////

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

		v2i winSize() const;

		void bind() const;

		void displayFramebuffer(const Framebuffer & src);

		void renderingLoop(const std::function<void()>& renderingFunc);

		void registerWindowComponent(WindowComponent& subwin) const;

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

		void setupWinViewport();

		void automatic_subwins_layout();

		static void glErrorCallBack(GLenum source, GLenum type, GLuint id, GLenum severity, 
			GLsizei length, const GLchar* message, const void* userParam);

		std::shared_ptr<GLFWwindow> window;
		WindowComponent debugComponent;
		v2d menuBarSize = { 0,0 };
		float ratio_rendering_gui = 2 / 3.0f;
		mutable std::map<std::string, std::reference_wrapper<WindowComponent>> subWinsCurrent, subWinsNext;
	};

	//enum class WinFlags : uint {
	//	UpdateWhenNotInFocus = 1 << 1
	//};
	//inline WinFlags operator| (WinFlags f, WinFlags g) { return (WinFlags)((uint)f | (uint)g); }
	//inline bool operator& (WinFlags f, WinFlags g) { return (bool)((uint)f & (uint)g); }

	class SubWindow : public Input {

	public:
		using UpdateFunc = std::function<void(const Input&)>;
		using RenderingFunc = std::function<void(Framebuffer&)>;
		using GuiFunc = std::function<void(void)>;

		SubWindow(const std::string& name, const v2i& renderingSize,
			const GuiFunc& guiFunction = {},
			const UpdateFunc& upFunc = {},
			const RenderingFunc& renderFunc = {}
		);

		void setGuiFunction(const GuiFunc& guiFunction);
		void setUpdateFunction(const UpdateFunc& upFunc);
		void setRenderingFunction(const RenderingFunc& renderFunc);

		void show(const Window & win);

		//void setFlags(WinFlags _flags);

		bool& active();

		v4f clearColor = { 0.1f, 0.1f, 0.1f, 1.0f };

		bool updateWhenNoFocus = false;

		WindowComponent& getRenderComponent();
		WindowComponent& getGuiComponent();

	private:
		void fitContent(v2f& outOffset, v2f& outSize, const v2f& vpSize, const v2f& availableSize);
		std::string gui_text(const std::string& str) const;
		void menuBar();
		void debugWin();

		Framebuffer framebuffer;
		
		GuiFunc guiFunc; 
		UpdateFunc updateFunc;
		RenderingFunc renderingFunc;

		std::string win_name;
		v2f gui_render_size;
		//Viewport winviewport;

		std::shared_ptr<WindowComponent> renderComponent, guiComponent;

		bool shouldUpdate = false, showGui = true,
			showDebug = false;
	};

}
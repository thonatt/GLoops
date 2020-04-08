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

	class WindowComponent : public Viewportd
	{

	public:
		enum class Type { RENDERING, GUI, DEBUG, FLOATING = DEBUG };

		using WinFunc = std::function<void(const Window&)>;
		using MenuFunc = std::function<void()>;

		WindowComponent(const std::string& name = "", Type type = Type::RENDERING, const WinFunc& guiFunc = {});

		void show(const Window& win);
		bool& isActive();

		void resize(const Viewportd& vp);
		const std::string& name() const;

		bool isInFocus() const;
		Type getType() const;

		v4f backgroundColor = { 0,0,0,1 };

		void setMenuFunc(MenuFunc&& fun);
		void menuFunc() const;

	protected:

		static size_t counter;

		std::string _name;
		WinFunc _guiFunc;
		MenuFunc _menuFunc;
		Type _type = Type::RENDERING;
		size_t id = 0;
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

		bool automaticLayout = true;

	protected:

		void keyboardCallback(GLFWwindow * win, int key, int scancode, int action, int mods);
		void charCallback(GLFWwindow* win, unsigned int codepoint);
		void mouseButtonCallback(GLFWwindow * win, int button, int action, int mods);
		void mouseScrollCallback(GLFWwindow * win, double x, double y);
		void mousePositionCallback(GLFWwindow * win, double x, double y);
		void winResizeCallback(GLFWwindow * win, int w, int h);

		template<typename GLFWfun, typename Lambda>
		void setupGLFWcallback(const GLFWfun& glfwFun, Lambda&& f) {
			glfwFun(window.get(), getCfuncPtr(std::forward<Lambda>(f)));
		}

		void setupWinViewport();

		void automaticSubwinsLayout();

		std::shared_ptr<GLFWwindow> window;
		WindowComponent debugComponent;
		v2d menuBarSize = { 0,0 };
		float ratio_rendering_gui = 0.6f;
		mutable std::map<std::string, std::reference_wrapper<WindowComponent>> subWinsCurrent, subWinsNext;
	};

	enum class WinFlags : uint {
		Default = 0,
		UpdateWhenNotInFocus = 1 << 1
	};
	inline WinFlags operator| (WinFlags f, WinFlags g) { return (WinFlags)((uint)f | (uint)g); }
	inline WinFlags operator& (WinFlags f, WinFlags g) { return (WinFlags)((uint)f & (uint)g); }
	inline WinFlags operator~ (WinFlags f) { return (WinFlags)(~(uint)f); }

	class SubWindow  {

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

		void setFlags(WinFlags flags);

		bool& active();

		v4f& clearColor();

		WindowComponent& getRenderComponent();
		WindowComponent& getGuiComponent();

		void menuGui();

	private:

		void debugWin();
		
		//Viewport winviewport;

		struct Internal {

			Internal(
				const std::string& name,
				const v2i& renderingSize,
				const GuiFunc& guiFunction,
				const UpdateFunc& upFunc,
				const RenderingFunc& renderFunc
			);

			static void fitContent(v2d& outOffset, v2d& outSize, const v2d& vpSize, const v2d& availableSize);

			void debugWin();
			void setupComponents();
		
			void menuGui();
			std::string gui_text(const std::string& str) const;

			std::shared_ptr<WindowComponent> renderComponent, guiComponent;
			Framebuffer framebuffer;
			Input input;

			GuiFunc guiFunc;
			UpdateFunc updateFunc;
			RenderingFunc renderingFunc;

			std::string win_name;
			v4f clearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
			v2f gui_render_size;
			WinFlags _flags = WinFlags::Default;

			bool shouldUpdate = false, showGui = true,
				showDebug = false;
		};

		std::shared_ptr<Internal> data;

	};

}
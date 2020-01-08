#include "Window.hpp"
#include <iostream>

#include <imgui-master/examples/imgui_impl_glfw.h>
#include <imgui-master/examples/imgui_impl_opengl3.h>

#include "Texture.hpp"

namespace gloops {

	Window::Window(const std::string & name)
	{
		if (!glfwInit()){
			std::cout << "glfwInit Initialization failed " << std::endl;
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);	
		//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

		const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		if (!mode) {
			std::cout << "no monitor detected " << std::endl;
			return;
		}
		glfwWindowHint(GLFW_RED_BITS, mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

		//window = glfwCreateWindow(mode->width, mode->height, name.c_str(), glfwGetPrimaryMonitor(), NULL);
		window = std::shared_ptr<GLFWwindow>(glfwCreateWindow(1600, 1000, name.c_str(), NULL, NULL), glfwDestroyWindow);

		if (!window){
			std::cout << " Window or OpenGL context creation failed " << std::endl;	
		} 
		
		setupWinViewport();

		glfwMakeContextCurrent(window.get());
		int desired_fps = 60;
		int interval = mode->refreshRate / desired_fps;
		std::cout << "screen refresh rate : " << mode->refreshRate << "fps" << std::endl;
		glfwSwapInterval(interval);

		setupGLFWcallback(glfwSetCursorPosCallback, [&](GLFWwindow* win, double x, double y) {
			mousePositionCallback(win, x, y);
		});

		setupGLFWcallback(glfwSetScrollCallback, [&](GLFWwindow* win, double x, double y) {
			mouseScrollCallback(win, x, y);
		});

		setupGLFWcallback(glfwSetMouseButtonCallback, [&](GLFWwindow* win, int button, int action, int mods) {
			mouseButtonCallback(win, button, action, mods);
		});

		setupGLFWcallback(glfwSetKeyCallback, [&](GLFWwindow* win, int key, int scancode, int action, int mods) {
			keyboardCallback(win, key, scancode, action, mods);
		});

		setupGLFWcallback(glfwSetWindowSizeCallback, [&](GLFWwindow* win, int w, int h) {
			winResizeCallback(win, w, h);
		});

		setupGLFWcallback(glfwSetWindowPosCallback, [&](GLFWwindow* win, int w, int h) {
			winResizeCallback(win, w, h);
		});

		GLenum glew_init_code = glewInit();
		if ( glew_init_code == GLEW_OK) {
			std::string version_str = (const char*)glGetString(GL_VERSION);
			std::cout << "OpenGL version " << version_str << std::endl;		
		} else {
			std::string glew_error_str = (const char*)glewGetErrorString(glew_init_code);
			std::cout << " glewInit failed " << glew_error_str << std::endl;
		}

		gl_check();

		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(glErrorCallBack, 0);

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		//imgui scaling
		float xscale, yscale;
		glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &xscale, &yscale);
		
		//float scaling = static_cast<float>(std::max(winSize()[0] / 1920.0, winSize()[1] / 1080.0));
		float scaling = std::max(xscale, yscale);
		ImGui::GetStyle().ScaleAllSizes(scaling);
		ImGui::GetIO().FontGlobalScale = scaling;


		debugComponent = WindowComponent("registered wins", WindowComponent::Type::GUI, [&](const Window& win) {
			std::stringstream s;
			s << "subviews : " << std::endl;
			for (auto& component : subWinsCurrent) {
				const auto& vp = component.second.get();
				s << "\t" << component.first << " " << vp.center().transpose() << " " << vp.diagonal().transpose() << std::endl;
			}
			ImGui::Text(s.str());
		});

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForOpenGL(window.get(), false);
		ImGui_ImplOpenGL3_Init("#version 420");
		//ImGui_ImplGlfw_CharCallback(window, )
		//ImGui_ImplOpenGL3_Init("#version 420");

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		gl_check();
	}

	Window::~Window()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		glfwTerminate();
	}

	void Window::showImguiDemo()
	{
		ImGui::ShowDemoWindow();
	}

	void Window::pollEvents()
	{
		//std::swap(mouseStatus, mouseStatusPrevious);
		//std::swap(keyStatus, keyStatusPrevious);

		mouseStatusPrevious = mouseStatus;
		keyStatusPrevious = keyStatus;

		//std::swap(currentStatus, previousStatus);
		//currentStatus.clear();

		_mouseScroll = { 0, 0 };

		glfwPollEvents();

		if (keyReleased(GLFW_KEY_ESCAPE)) {
			glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
		}

	}

	void Window::swapBuffers()
	{
		Framebuffer::bindDefault();
		glClearColor(0, 0, 0, 0);
		viewport().gl();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


		glfwSwapBuffers(window.get());

		std::swap(subWinsCurrent, subWinsNext);
		subWinsNext.clear();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	int Window::shouldClose() const
	{
		return glfwWindowShouldClose(window.get());
	}

	void Window::close()
	{
		glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
	}

	void Window::clear()
	{
		bind();
		glClearColor(0, 0, 0, 0);
		glClear((GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	}

	v2i Window::winSize() const
	{
		return viewport().diagonal().template cast<int>();
	}

	void Window::bind() const
	{
		Framebuffer::bindDefault();
		//glViewport(0, 0, winSize()[0], winSize()[1]);
	}

	void Window::displayFramebuffer(const Framebuffer& src)
	{
		src.bind(GL_READ_FRAMEBUFFER);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		gl_check();

		glBlitFramebuffer(
			0, 0, src.w(), src.h(),
			0, 0, winSize()[0], winSize()[1],
			GL_COLOR_BUFFER_BIT,
			GL_LINEAR
		);

		gl_check();
	}

	void Window::renderingLoop(const std::function<void()>& renderingFunc)
	{
		bool pause = false, pauseNext = false;

		bool showImGuiDemo = false, showDebug = false;

		while (!shouldClose()) {

			pause = pauseNext;

			if (!pause) {
				clear();
			}

			pollEvents();

			if (ImGui::BeginMainMenuBar()) {
				menuBarSize = v2d(ImGui::GetWindowWidth(), ImGui::GetWindowHeight());
				if (ImGui::BeginMenu("Options")) {
					ImGui::MenuItem("Automatic layout", 0, &automaticLayout);	
					ImGui::MenuItem("Pause", 0, &pauseNext);
					ImGui::MenuItem("Debug", 0, &showDebug);
					ImGui::MenuItem("ImGui demo", 0, &showImGuiDemo);		
					ImGui::SliderFloat("Ratio rendering/gui", &ratio_rendering_gui, 0, 1);
					ImGui::EndMenu();
				}
				ImGui::EndMainMenuBar();
			}

			if (automaticLayout) {
				automatic_subwins_layout();
			}

			if (showDebug) {
				debugComponent.show(*this);
				if(ImGui::Begin("window debug")) {
					std::stringstream s;
					s << viewport();
					ImGui::Text(s.str());
				}
				ImGui::End();
			}
			
			if (!pause) {
				renderingFunc();
			} 

			if (showImGuiDemo) {
				ImGui::ShowDemoWindow();
			}

			swapBuffers();	
		}
	}

	void Window::registerWindowComponent(WindowComponent& subwin) const
	{
		subWinsNext.emplace(subwin.name(), std::ref(subwin));
	}

	void Window::keyboardCallback(GLFWwindow * win, int key, int scancode, int action, int mods)
	{
		if (key >= 0) {
			keyStatus[key] = action;
			//currentStatus[std::make_pair(InputType::KEYBOARD, key)] = action;
		} else {
			std::cout << "unknown key" << std::endl;
		}
	}

	void Window::mouseButtonCallback(GLFWwindow * win, int button, int action, int mods)
	{
		//if (!ImGui::GetIO().WantCaptureMouse) {
		//	
		//}

		if (button >= 0) {
			mouseStatus[button] = action;
			//currentStatus[std::make_pair(InputType::MOUSE, button)] = action;
			//std::cout << button << " : " << action << std::endl;
		} else {
			std::cout << "unknown mouse button" << std::endl;
		}
	}

	void Window::mouseScrollCallback(GLFWwindow * win, double x, double y)
	{
		_mouseScroll = { x, y };
		//std::cout << "call back scroll " << x << " " << y << std::endl;
	}

	void Window::mousePositionCallback(GLFWwindow * win, double x, double y)
	{
		_mousePosition = { x, y };
		//std::cout << x << " " << y << std::endl;
	}

	void Window::winResizeCallback(GLFWwindow * win, int w, int h)
	{
		if (w == 0 || h == 0) {
			return;
		}
		setupWinViewport();
	}

	void Window::setupWinViewport()
	{
		v2i size;
		glfwGetWindowSize(window.get(), &size[0], &size[1]);
		_viewport = Viewport(v2d(0, menuBarSize.y()), size.template cast<double>());
	}

	void Window::automatic_subwins_layout()
	{
		size_t num_render_win = 0, num_gui_win = 0; 
		for (const auto& win : subWinsCurrent) {
			if (win.second.get().getType() == WindowComponent::Type::RENDERING) {
				++num_render_win;
			} else if(win.second.get().getType() == WindowComponent::Type::GUI){
				++num_gui_win;
			}
		}

		double ratio = std::clamp<double>(ratio_rendering_gui, 0, 1);

		Viewport render_grid_vp = Viewport(viewport().min(), viewport().min() + viewport().diagonal().cwiseProduct(v2d(ratio, 1.0)));
		Viewport gui_grid_vp = Viewport(viewport().min() + viewport().diagonal().cwiseProduct(v2d(ratio, 0.0)), viewport().max());

		const size_t render_grid_size = (size_t)std::ceil(std::sqrt(num_render_win));
		v2d render_grid_res = v2d(render_grid_size, render_grid_size);
		v2d gui_grid_res = v2d(1, num_gui_win);

		auto drawVP = [](WindowComponent& comp, const Viewport& vp, const v2i& coords, const v2d& res) {
			v2d uv = vp.diagonal().cwiseQuotient(res);
			Viewport subVP = Viewport(
				vp.min() + uv.cwiseProduct(coords.template cast<double>()),
				vp.min() + uv.cwiseProduct((coords + v2i(1, 1)).template cast<double>())
			);
			comp.resize(subVP);
		};

		size_t render_win_id = 0, gui_win_id = 0;

		v2f weights_gui(0, 0), weights_render(0, 0);
		for (auto& win : subWinsCurrent) {
			auto& comp = win.second.get();
			if (comp.getType() == WindowComponent::Type::RENDERING) {
				v2i coords = v2i(render_win_id % render_grid_size, render_win_id / render_grid_size);
				drawVP(comp, render_grid_vp, coords, render_grid_res);
				++render_win_id;
			} else if (win.second.get().getType() == WindowComponent::Type::GUI) {
				drawVP(comp, gui_grid_vp, v2i(0, gui_win_id), gui_grid_res);
				++gui_win_id;
			}
		}
	}

	void Window::glErrorCallBack(GLenum source, GLenum type, GLuint id, GLenum severity,
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
			GLOOPS_ENUM_STR(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_LOGGED_MESSAGES_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_CALLBACK_FUNCTION_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_CALLBACK_USER_PARAM_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_API_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_SHADER_COMPILER_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_THIRD_PARTY_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_SOURCE_APPLICATION_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_ERROR_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_PORTABILITY_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_PERFORMANCE_ARB),
			GLOOPS_ENUM_STR(GL_DEBUG_TYPE_OTHER_ARB)
		};

		//GLOOPS_ENUM_STR(GL_INVALID_ENUM),
		//GLOOPS_ENUM_STR(GL_INVALID_VALUE),
		//GLOOPS_ENUM_STR(GL_INVALID_OPERATION),
		//GLOOPS_ENUM_STR(GL_STACK_OVERFLOW),
		//GLOOPS_ENUM_STR(GL_STACK_UNDERFLOW),
		//GLOOPS_ENUM_STR(GL_OUT_OF_MEMORY),
		//GLOOPS_ENUM_STR(GL_INVALID_FRAMEBUFFER_OPERATION),
		//GLOOPS_ENUM_STR(GL_MAX_DEBUG_MESSAGE_LENGTH_ARB),
		//GLOOPS_ENUM_STR(GL_MAX_DEBUG_LOGGED_MESSAGES_ARB),

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
			GLOOPS_SEV_STR(GL_DEBUG_SEVERITY_HIGH_ARB, HIGH),
			GLOOPS_SEV_STR(GL_DEBUG_SEVERITY_MEDIUM_ARB, MEDIUM),
			GLOOPS_SEV_STR(GL_DEBUG_SEVERITY_LOW_ARB, LOW),
			GLOOPS_SEV_STR(GL_DEBUG_SEVERITY_HIGH_AMD, HIGH),
			GLOOPS_SEV_STR(GL_DEBUG_SEVERITY_MEDIUM_AMD, MEDIUM),
			GLOOPS_SEV_STR(GL_DEBUG_SEVERITY_LOW_AMD, LOW)
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

		if ((uint)glSev.lvl < (uint)SeveriyLevel::HIGH) {
			return;
		}

		std::string err_str = "UNKNOWN GL ERROR";
		auto err_it = errors.find(type);
		if (err_it != errors.end()) {
			err_str = err_it->second;
		}

		std::cout << "GL callback :  source : " << sources_str.at(source) << ", error = " << err_str <<
			", severity = " << glSev.str << "\n" << message << std::endl;
	}

	size_t WindowComponent::counter = 0;

	WindowComponent::WindowComponent(const std::string& name, Type type, const Func& guiFunc, const v2f& weights)
		: Viewport(v2d(0, 0), v2d(1, 1)), _name(name), _guiFunc(guiFunc), _type(type), _weights(weights), id(counter)
	{
		++counter;
	}

	void WindowComponent::show(const Window& win)
	{
		if (_active && _guiFunc) {

			win.registerWindowComponent(*this);

			if (shouldResize) {
				ImGui::SetNextWindowPos(ImVec2((float)min()[0], (float)min()[1]));
				ImGui::SetNextWindowSize(ImVec2((float)diagonal()[0], (float)diagonal()[1]));
				shouldResize = false;
			}

			inFocus = false;

			const auto& bg = backgroundColor;

			ImGui::PushStyleColor(ImGuiCol_WindowBg, { bg[0], bg[1], bg[2], bg[3] });

			ImGuiWindowFlags imguiWinFlags = 0;
			if (_type == Type::RENDERING) {
				imguiWinFlags |= ImGuiWindowFlags_MenuBar;
			}
			if (win.automaticLayout && (_type == Type::GUI || _type == Type::RENDERING)) {
				imguiWinFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
			}
			if (_type == Type::DEBUG) {
				imguiWinFlags |= ImGuiWindowFlags_AlwaysAutoResize;
			}

			if (ImGui::Begin(_name.c_str(), 0, imguiWinFlags)) {

				v2d screenTopLeft(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
				v2d availableSize(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
				//std::cout << _name << " topleft / available : " << screenTopLeft.transpose() << " " << availableSize.transpose() << std::endl;

				static_cast<Viewport&>(*this) = Viewport(screenTopLeft, screenTopLeft + availableSize);

				//std::cout << _name << " vp min max : " << min().transpose() << " " << max().transpose() << std::endl;
				//std::cout << _name <<  " " << this << std::endl;

				if (ImGui::IsItemHovered()) {
					ImGui::CaptureKeyboardFromApp(false);
					ImGui::CaptureMouseFromApp(false);
				}

				inFocus |= ImGui::IsWindowFocused();

				_guiFunc(win);
			}
			ImGui::End();

			ImGui::PopStyleColor();


		} 
		
	}

	bool& WindowComponent::isActive()
	{
		return _active;
	}

	void WindowComponent::resize(const Viewport& vp)
	{
		static_cast<Viewport&>(*this) = vp;
		shouldResize = true;
	}

	const std::string& WindowComponent::name() const
	{
		return _name;
	}

	bool WindowComponent::isInFocus() const
	{
		return inFocus;
	}

	WindowComponent::Type WindowComponent::getType() const
	{
		return _type;
	}

	const v2f& WindowComponent::weights() const
	{
		return _weights;
	}

	//bool WindowComponent::operator<(const WindowComponent& other) const
	//{
	//	return id < other.id;
	//}

	SubWindow::SubWindow(const std::string& name, const v2i& renderingSize,
		const GuiFunc& guiFunction, const UpdateFunc& upFunc, const RenderingFunc& renderFunc)
		: guiFunc(guiFunction), updateFunc(upFunc), renderingFunc(renderFunc), win_name(name)
	{
		v2i render_size = renderingSize.cwiseMax(v2i(1, 1));

		TexParams params = TexParams::RGBA;
		params.disableMipmap().setWrapS(GL_CLAMP_TO_EDGE).setWrapT(GL_CLAMP_TO_EDGE);	
		framebuffer = Framebuffer(render_size[0], render_size[1], 4, params);
		
		//std::cout << "subwin rsize : " << render_size.transpose() << std::endl;

		_viewport = Viewport(v2d(0, ImGui::TitleHeight()), v2d(render_size[0], render_size[1] + ImGui::TitleHeight()));


		//std::cout << name << " ctor : rcom " << &renderComponent << std::endl;

		renderComponent = std::make_shared<WindowComponent>(name + "##render", WindowComponent::Type::RENDERING, [&](const Window& win) {

			shouldUpdate = false;

			menuBar();

			//std::cout << win_name << " rcom " << &renderComponent << std::endl;

			v2f offset, size;

			//std::cout << "vp diag / rcomp diag  : " << viewport().diagonal().transpose() << " " << renderComponent.diagonal().transpose() << std::endl;
			
			fitContent(offset, size, viewport().diagonal().cwiseMax(v2d(1, 1)).template cast<float>(), renderComponent->diagonal().template cast<float>());
			
			//std::cout << "offset / size : " << offset.transpose() << " " << size.transpose() << std::endl;

			if (offset.hasNaN() || size.hasNaN()) {
				std::cout << "offset size " << offset.transpose() << " " << size.transpose() << std::endl;
			}

			v2f screenTopLeft = renderComponent->min().template cast<float>() + offset;
			v2f screenBottomRight = screenTopLeft + size;

			debugWin();

			_viewport = Viewport(screenTopLeft.template cast<double>(), screenBottomRight.template cast<double>());

			//viewport().checkNan();

			Input& subInput = static_cast<Input&>(*this);
			subInput = win.subInput(viewport(), !renderComponent->isInFocus());

			shouldUpdate |= renderComponent->isInFocus();
			shouldUpdate |= updateWhenNoFocus;

			if (showGui) {
				guiComponent->show(win);
			}

			if (shouldUpdate && updateFunc) {
				updateFunc(subInput);
			}

			framebuffer.clear(clearColor, (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

			if (renderingFunc) {
				renderingFunc(framebuffer);
			}

			ImGui::SetCursorPos({ offset[0], offset[1] });

			ImGui::InvisibleButton((win_name + "_dummy").c_str(), { size[0], size[1] });
			ImGui::GetWindowDrawList()->AddImage(
				framebuffer.getAttachment().getId(),
				{ screenTopLeft[0], screenBottomRight[1] },
				{ screenBottomRight[0], screenTopLeft[1] }
			);
		});

		guiComponent = std::make_shared<WindowComponent>(name + " gui", WindowComponent::Type::GUI, [&](const Window& win) {
			shouldUpdate |= ImGui::IsWindowFocused();	
			if (guiFunc) {
				guiFunc();
			}			
		});
	}

	void SubWindow::setGuiFunction(const GuiFunc& guiFunction)
	{
		guiFunc = guiFunction;
	}

	void gloops::SubWindow::setUpdateFunction(const UpdateFunc& upFunc)
	{
		updateFunc = upFunc;
	}

	void gloops::SubWindow::setRenderingFunction(const RenderingFunc& renderFunc)
	{
		renderingFunc = renderFunc;
	}

	void SubWindow::show(const Window & win)
	{
		renderComponent->show(win);
	}

	bool& gloops::SubWindow::active()
	{
		return renderComponent->isActive();
	}

	WindowComponent& gloops::SubWindow::getRenderComponent()
	{
		return *renderComponent;
	}

	WindowComponent& gloops::SubWindow::getGuiComponent()
	{
		return *guiComponent;
	}

	void SubWindow::fitContent(v2f& outOffset, v2f& outSize, const v2f& vpSize, const v2f& availableSize)
	{
		const v2f ratios = vpSize.cwiseQuotient(availableSize);
		if (ratios.x() < ratios.y()){
			outSize.y() = availableSize.y();
			outSize.x() = outSize.y() * vpSize.x() / vpSize.y();
		} else {
			outSize.x() = availableSize.x();
			outSize.y() = outSize.x() * vpSize.y() / vpSize.x();
		}
		outOffset = (availableSize - outSize) / 2;
	
	}

	std::string SubWindow::gui_text(const std::string& str) const
	{
		return str + "##" + win_name;
	}

	void gloops::SubWindow::menuBar()
	{
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu(gui_text("settings").c_str())) {
				ImGui::MenuItem(gui_text("gui").c_str(), NULL, &showGui);
				ImGui::MenuItem(gui_text("update when not in focus").c_str(), NULL, &updateWhenNoFocus);
				ImGui::MenuItem(gui_text("debug").c_str(), NULL, &showDebug);

				if (ImGui::BeginMenu(gui_text("rendering res").c_str())) {
					gui_render_size = v2f(framebuffer.w(), framebuffer.h());

					bool changed = false;
					if (ImGui::SliderFloat(gui_text("W").c_str(), &gui_render_size[0], 1, (float)viewport().width())) {
						gui_render_size[1] = framebuffer.h() * gui_render_size[0] / (float)framebuffer.w();
						changed = true;
					}
					if (ImGui::SliderFloat(gui_text("H").c_str(), &gui_render_size[1], 1, (float)viewport().height())) {
						gui_render_size[0] = framebuffer.w() * gui_render_size[1] / (float)framebuffer.h();
						changed = true;
					}

					if (changed) {
						framebuffer.resize((int)std::ceil(gui_render_size[0]), (int)std::ceil(gui_render_size[1]));
					}

					ImGui::EndMenu();

				}

				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

	}

	void gloops::SubWindow::debugWin()
	{
		if (showDebug) {
			if (ImGui::Begin((win_name + "debug win").c_str())) {
				std::stringstream s;
				s << "framebufer id : " << framebuffer.getAttachment().getId();
				ImGui::Text(s.str());

				if (ImGui::CollapsingHeader(gui_text("input").c_str())) {
					guiInputDebug();
				}
			}
			ImGui::End();
		}
	}

}
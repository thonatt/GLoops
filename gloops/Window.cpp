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
		
		glfwGetFramebufferSize(window.get(), &size[0], &size[1]);
		_viewport = Viewport(v2d(0, 0), size. template cast<double>());

		glfwMakeContextCurrent(window.get());
		int desired_fps = 60;
		int interval = mode->refreshRate / desired_fps;
		std::cout << "screen refresh rate : " << mode->refreshRate << "fps" << std::endl;
		glfwSwapInterval(interval);
		
		{
			using namespace std::placeholders;
			setCallback<KEY_BUTTON>(std::bind(&Window::keyboardCallback, this, _1, _2, _3, _4, _5));
			setCallback<MOUSE_BUTTON>(std::bind(&Window::mouseButtonCallback, this, _1, _2, _3, _4));
			setCallback<MOUSE_SCROLL>(std::bind(&Window::mouseScrollCallback, this, _1, _2, _3));
			setCallback<MOUSE_POSITION>(std::bind(&Window::mousePositionCallback, this, _1, _2, _3));
			setCallback<WIN_RESIZE>(std::bind(&Window::winResizeCallback, this, _1, _2, _3));
		}

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

		gl_check();
		ImGui::Render();

		glViewport(0, 0, size[0], size[1]);

		gl_check();

		auto data = ImGui::GetDrawData();

		gl_check();

		ImGui_ImplOpenGL3_RenderDrawData(data);

		gl_check();

		glfwSwapBuffers(window.get());

		gl_check();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		gl_check();
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
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
	}

	const v2i & Window::winSize() const
	{
		return size;
	}

	void Window::bind() const
	{
		Framebuffer::bindDefault();
		glViewport(0, 0, winSize()[0], winSize()[1]);
	}

	void Window::displayFramebuffer(const Framebuffer & src)
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

		while (!shouldClose()) {
			
			pause = pauseNext;

			if (!pause) {
				clear();
			}
			
			pollEvents();
			
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("options")) {
					ImGui::MenuItem("pause", 0, &pauseNext, true);
				}
			}

			if (!pause) {
				renderingFunc();
			} 

			swapBuffers();	
		}
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

		size = { w,h };

		float xscale, yscale;
		glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &xscale, &yscale);
		float scaling = std::max(xscale, yscale);

		//float scaling = std::max(winSize()[0] / 1920.0f, winSize()[1] / 1080.0f);
		ImGui::GetStyle() = ImGuiStyle();
		ImGui::GetStyle().ScaleAllSizes(scaling);
		ImGui::GetIO().FontGlobalScale = scaling;
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
			SeveriyLevel lvl;
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

		if ((uint)glSev.lvl <= (uint)SeveriyLevel::NOTIFICATION) {
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

	SubWindow::SubWindow(const std::string& name, const v2i& renderingSize,
		GuiFunc guiFunction, UpdateFunc upFunc, RenderingFunc renderFunc)
		: guiFunc(guiFunction), updateFunc(upFunc), renderingFunc(renderFunc), win_name(name)
	{
		TexParams params = TexParams::RGBA;
		params.setMipmapStatus(false).setWrapS(GL_CLAMP_TO_EDGE).setWrapT(GL_CLAMP_TO_EDGE);
		framebuffer = Framebuffer(renderingSize[0], renderingSize[1], 4, params),
		_viewport = Viewport(v2d(0, ImGuiTitleHeight()), v2d(renderingSize[0], renderingSize[1] + ImGuiTitleHeight()));
	}

	void SubWindow::show(const Window & win)
	{
		inFocus = false;
		shouldUpdate = false;

		if (ImGui::Begin(win_name.c_str(), 0, ImGuiWindowFlags_MenuBar)) {
			inFocus = ImGui::IsWindowFocused();

			if (ImGui::BeginMenuBar()) {
				if (ImGui::BeginMenu(gui_text("settings").c_str())) {
					ImGui::MenuItem(gui_text("gui").c_str(), NULL, &showGui);
					ImGui::MenuItem(gui_text("debug").c_str(), NULL, &showDebug);
					ImGui::MenuItem(gui_text("input").c_str(), NULL, &showInput);

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

			if (showGui) {
				if (ImGui::Begin((win_name + " gui").c_str())) {
					shouldUpdate = ImGui::IsWindowFocused();
					if (guiFunc) {
						guiFunc();
					}
				}
				ImGui::End();
			}

			v2f screenTopLeft(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
			v2f availableSize(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

			v2f offset, size, screenBottomRight;
			fitContent(offset, size, viewport().diagonal().cwiseMax(v2d(1,1)).template cast<float>(), availableSize);

			screenTopLeft += offset;
			screenBottomRight = screenTopLeft + size;

			shouldUpdate = shouldUpdate || inFocus;

			if (ImGui::IsItemHovered()) {
				ImGui::CaptureKeyboardFromApp(false);
				ImGui::CaptureMouseFromApp(false);
			}

			_viewport = Viewport(screenTopLeft.template cast<double>(), screenBottomRight.template cast<double>());

			Input& subInput = static_cast<Input&>(*this);
			subInput = win.subInput(viewport(), !inFocus);

			if (showInput) {
				//if (ImGui::Begin((win_name + " input").c_str())) {
				//	win.guiInputDebug();
				//}
				//ImGui::End();

				if (ImGui::Begin((win_name + " subinput").c_str())) {
					guiInputDebug();
				}
				ImGui::End();
			}

			if (shouldUpdate && updateFunc) {
				updateFunc(subInput);
			}
			
			float bg = 0.0f;
			framebuffer.clear(v4f(bg, bg, bg, 1.0), (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

			if (renderingFunc) {
				renderingFunc(framebuffer);
			}
			
			if (showDebug) {
				if (ImGui::Begin(gui_text("debugwin").c_str())) {
					std::stringstream s;
					s << " size : " << size[0] << " " << size[1] << std::endl;
					s << " viewport : " << screenBottomRight[0] - screenTopLeft[0] << " " << screenBottomRight[1] - screenTopLeft[1] << std::endl;
					s << " offset : " << offset[0] << " " << offset[1] << std::endl;
					ImGui::Text(s.str());
				}
				ImGui::End();
			}

			ImGui::SetCursorPos({ offset[0], offset[1] });

			ImGui::InvisibleButton((win_name + "_dummy").c_str(), { size[0], size[1] });
			ImGui::GetWindowDrawList()->AddImage(
				framebuffer.getAttachment().getId(),
				{ screenTopLeft[0], screenBottomRight[1] },
				{ screenBottomRight[0], screenTopLeft[1] });
		}
		ImGui::End();
	}

	void SubWindow::fitContent(v2f& outOffset, v2f& outSize, const v2f& vpSize, const v2f& availableSize)
	{
		const v2f ratios = vpSize.cwiseQuotient(availableSize);
		if (ratios.x() < ratios.y()){
			const float aspect = vpSize.x() / vpSize.y();
			outSize.y() = availableSize.y();
			outSize.x() = outSize.y() * aspect;
		} else {
			const float aspect = vpSize.y() / vpSize.x();
			outSize.x() = availableSize.x();
			outSize.y() = outSize.x() * aspect;
		}
		outOffset = (availableSize - outSize) / 2;
	}

	std::string SubWindow::gui_text(const std::string& str) const
	{
		return str + "##" + win_name;
	}

	const float SubWindow::ImGuiTitleHeight()
	{
		return ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
	}

}
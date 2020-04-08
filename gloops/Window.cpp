#include "Window.hpp"
#include <iostream>

#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_opengl3.h>

#include "Texture.hpp"
#include "Debug.hpp"

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
		int interval = mode->refreshRate / desired_fps - 1;
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

		setupGLFWcallback(glfwSetCharCallback, [&](GLFWwindow* win, unsigned int codepoint) {
			charCallback(win, codepoint);
		});

		setupGLFWcallback(glfwSetWindowSizeCallback, [&](GLFWwindow* win, int w, int h) {
			winResizeCallback(win, w, h);
		});

		setupGLFWcallback(glfwSetWindowPosCallback, [&](GLFWwindow* win, int w, int h) {
			winResizeCallback(win, w, h);
		});
	
		int version = gladLoadGL(); // gladLoadGL(glfwGetProcAddress);
		std::cout << "OpenGL version " << GLVersion.major << "." << GLVersion.minor << std::endl;
		
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
		mouseStatusPrevious = mouseStatus;
		keyStatusPrevious = keyStatus;

		_mouseScroll = { 0, 0 };

		glfwPollEvents();

		if (keyReleased(GLFW_KEY_ESCAPE)) {
			glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
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
		std::cout << " starting main loop " << std::endl;

		bool pause = false, pauseNext = false;
		bool showImGuiDemo = false, showDebug = false, showLogs = false;

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
					ImGui::MenuItem("Logs", 0, &showLogs);
					ImGui::MenuItem("ImGui demo", 0, &showImGuiDemo);		
					ImGui::SliderFloat("Ratio rendering/gui", &ratio_rendering_gui, 0, 1);
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Subwindows")) {
					for (auto& subwin : subWinsCurrent) {
						if (subwin.second.get().getType() == WindowComponent::Type::RENDERING && ImGui::BeginMenu((subwin.second.get().name() + "##menu").c_str())) {
							subwin.second.get().menuFunc();
							ImGui::EndMenu();
						}						
					}
					ImGui::EndMenu();
				}
				ImGui::EndMainMenuBar();
			}

			if (automaticLayout) {
				automaticSubwinsLayout();
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
			
			if (showLogs) {
				getDebugLogs().show(*this);
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
		ImGui_ImplGlfw_KeyCallback(win, key, scancode, action, mods);

		if (key >= 0) {
			keyStatus[key] = action;
		} else {
			std::cout << "unknown key" << std::endl;
		}
	}

	void Window::charCallback(GLFWwindow* win, unsigned int codepoint)
	{
		ImGui_ImplGlfw_CharCallback(win, codepoint);
	}

	void Window::mouseButtonCallback(GLFWwindow* win, int button, int action, int mods)
	{
		ImGui_ImplGlfw_MouseButtonCallback(win, button, action, mods);

		if (button >= 0) {
			mouseStatus[button] = action;
		} else {
			std::cout << "unknown mouse button" << std::endl;
		}
	}

	void Window::mouseScrollCallback(GLFWwindow* win, double x, double y)
	{
		ImGui_ImplGlfw_ScrollCallback(win, x, y);

		_mouseScroll = { x, y };
	}

	void Window::mousePositionCallback(GLFWwindow * win, double x, double y)
	{
		_mousePosition = { x, y };
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
		_viewport = Viewportd(v2d(0, 0), size.template cast<double>());
	}

	void Window::automaticSubwinsLayout()
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

		//+ v2d(0, ImGui::TitleHeight())

		Viewportd render_grid_vp = Viewportd(
			viewport().min() + v2d(0, ImGui::TitleHeight()), 
			viewport().min() + viewport().diagonal().cwiseProduct(v2d(ratio, 1.0))
		);
		Viewportd gui_grid_vp = Viewportd(
			viewport().min() + viewport().diagonal().cwiseProduct(v2d(ratio, 0.0)) + v2d(0, ImGui::TitleHeight()),
			viewport().max()
		);

		const size_t render_grid_size = (size_t)std::ceil(std::sqrt(num_render_win));
		v2d render_grid_res = v2d(render_grid_size, render_grid_size);
		v2d gui_grid_res = v2d(1, num_gui_win);

		auto drawVP = [](WindowComponent& comp, const Viewportd& vp, const v2i& coords, const v2d& res) {
			v2d uv = vp.diagonal().cwiseQuotient(res);
			Viewportd subVP = Viewportd(
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

	size_t WindowComponent::counter = 0;

	WindowComponent::WindowComponent(const std::string& name, Type type, const WinFunc& guiFunc)
		: Viewportd(v2d(0, 0), v2d(1, 1)), _name(name), _guiFunc(guiFunc), _type(type), id(counter)
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
				//imguiWinFlags |= ImGuiWindowFlags_MenuBar;
			}
			if (win.automaticLayout && (_type == Type::GUI || _type == Type::RENDERING)) {
				imguiWinFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
			}

			if (ImGui::Begin(_name.c_str(), 0, imguiWinFlags)) {

				v2d screenTopLeft(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
				v2d availableSize(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
				//std::cout << _name << " topleft / available : " << screenTopLeft.transpose() << " " << availableSize.transpose() << std::endl;

				static_cast<Viewportd&>(*this) = Viewportd(screenTopLeft, screenTopLeft + availableSize);

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

	void WindowComponent::resize(const Viewportd& vp)
	{
		static_cast<Viewportd&>(*this) = vp;
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

	void WindowComponent::setMenuFunc(MenuFunc&& fun)
	{
		_menuFunc = fun;
	}

	void WindowComponent::menuFunc() const
	{
		if (_menuFunc) {
			_menuFunc();
		}
	}

	//bool WindowComponent::operator<(const WindowComponent& other) const
	//{
	//	return id < other.id;
	//}

	SubWindow::SubWindow(const std::string& name, const v2i& renderingSize,
		const GuiFunc& guiFunction, const UpdateFunc& upFunc, const RenderingFunc& renderFunc)
	{
		data = std::make_shared<SubWindow::Internal>(name, renderingSize, guiFunction, upFunc, renderFunc);
		data->setupComponents();
	}

	SubWindow::Internal::Internal(const std::string& name, const v2i& renderingSize, const GuiFunc& guiFunction, const UpdateFunc& upFunc, const RenderingFunc& renderFunc)
		: guiFunc(guiFunction), updateFunc(upFunc), renderingFunc(renderFunc), win_name(name)
	{
		v2i render_size = renderingSize.cwiseMax(v2i(1, 1));

		TexParams params = TexParams::RGBA;
		params.disableMipmap().setWrapS(GL_CLAMP_TO_EDGE).setWrapT(GL_CLAMP_TO_EDGE);
		framebuffer = Framebuffer(render_size[0], render_size[1], 4, params);
	}

	void SubWindow::Internal::debugWin()
	{
		if (showDebug) {
			if (ImGui::Begin((win_name + "debug win").c_str())) {
				std::stringstream s;
				s << "framebufer id : " << framebuffer.getAttachment().getId();
				ImGui::Text(s.str());

				if (ImGui::CollapsingHeader(gui_text("input").c_str())) {
					input.guiInputDebug();
				}
			}
			ImGui::End();
		}
	}

	void SubWindow::Internal::setupComponents()
	{
		renderComponent = std::make_shared<WindowComponent>(win_name + "##render", WindowComponent::Type::RENDERING, [&](const Window& win) {

			shouldUpdate = false;

			//std::cout << win_name << " rcom " << &renderComponent << std::endl;

			v2d offset, size;

			//std::cout << "vp diag / rcomp diag  : " << viewport().diagonal().transpose() << " " << renderComponent.diagonal().transpose() << std::endl;

			fitContent(offset, size, input.viewport().diagonal().cwiseMax(v2d(1, 1)), renderComponent->diagonal().cwiseMax(v2d(1, 1)));

			//std::cout << "offset / size : " << offset.transpose() << " " << size.transpose() << std::endl;

			if (offset.hasNaN() || size.hasNaN()) {
				std::cout << "offset size " << offset.transpose() << " " << size.transpose() << std::endl;
			}

			const v2d screenTopLeft = renderComponent->min() + offset;
			const v2d screenBottomRight = screenTopLeft + size;

			debugWin();

			input = win.subInput(Viewportd(screenTopLeft, screenBottomRight), !renderComponent->isInFocus());

			shouldUpdate |= renderComponent->isInFocus();
			shouldUpdate |= (bool)(_flags & WinFlags::UpdateWhenNotInFocus);

			if (showGui) {
				guiComponent->show(win);
			}

			if (shouldUpdate && updateFunc) {
				updateFunc(input);
			}

			framebuffer.clear(clearColor, (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

			if (renderingFunc) {
				renderingFunc(framebuffer);
			}

			ImGui::SetCursorPos({ (float)offset[0], (float)offset[1] });

			ImGui::InvisibleButton((win_name + "_dummy").c_str(), { (float)size[0], (float)size[1] });
			ImGui::GetWindowDrawList()->AddImage(
				framebuffer.getAttachment().getId(),
				{ (float)screenTopLeft[0], (float)screenBottomRight[1] },
				{ (float)screenBottomRight[0], (float)screenTopLeft[1] }
			);
		}

		);

		renderComponent->setMenuFunc([&] {
			menuGui();
		});

		guiComponent = std::make_shared<WindowComponent>(win_name + " gui", WindowComponent::Type::GUI, [&](const Window& win) {
			shouldUpdate |= ImGui::IsWindowFocused();
			if (guiFunc) {
				guiFunc();
			}
		});
	}

	void SubWindow::Internal::fitContent(v2d& outOffset, v2d& outSize, const v2d& vpSize, const v2d& availableSize)
	{
		const v2d ratios = vpSize.cwiseQuotient(availableSize);
		if (ratios.x() < ratios.y()) {
			outSize.y() = availableSize.y();
			outSize.x() = outSize.y() * vpSize.x() / vpSize.y();
		} else {
			outSize.x() = availableSize.x();
			outSize.y() = outSize.x() * vpSize.y() / vpSize.x();
		}
		outOffset = (availableSize - outSize) / 2;
	}

	void SubWindow::Internal::menuGui()
	{
		if (ImGui::BeginMenu(gui_text("settings").c_str())) {
			ImGui::MenuItem(gui_text("gui").c_str(), NULL, &showGui);
			bool updateWhenNoFocus = (bool)(_flags & WinFlags::UpdateWhenNotInFocus);
			if (ImGui::MenuItem(gui_text("update when not in focus").c_str(), NULL, &updateWhenNoFocus)) {
				 _flags = (updateWhenNoFocus ? _flags | WinFlags::UpdateWhenNotInFocus : _flags & ~WinFlags::UpdateWhenNotInFocus);
			}
			ImGui::MenuItem(gui_text("debug").c_str(), NULL, &showDebug);

			if (ImGui::BeginMenu(gui_text("rendering res").c_str())) {
				gui_render_size = v2f(framebuffer.w(), framebuffer.h());

				bool changed = false;
				if (ImGui::SliderFloat(gui_text("W").c_str(), &gui_render_size[0], 1, 1920)) {
					gui_render_size[1] = framebuffer.h() * gui_render_size[0] / (float)framebuffer.w();
					changed = true;
				}
				if (ImGui::SliderFloat(gui_text("H").c_str(), &gui_render_size[1], 1, 1080)) {
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

		if (ImGui::BeginMenu(gui_text("save").c_str())) {
			std::stringstream filename;
			filename << win_name << "_" << ImGui::GetTime();
			std::string file = filename.str();
			std::replace(file.begin(), file.end(), ' ', '_');
			std::replace(file.begin(), file.end(), '.', '_');

			if (ImGui::Button("jpg")) {
				Image4b img;
				framebuffer.readBack(img);
				img.convert<uchar, 3>().flip().save(file + ".jpg");
			}
			if (ImGui::Button("png")) {
				Image4b img;
				framebuffer.readBack(img);
				img.convert<uchar, 3>().flip().save(file + ".png");
			}
			ImGui::EndMenu();
		}
	}

	std::string SubWindow::Internal::gui_text(const std::string& str) const
	{
		return str + "##" + win_name;
	}

	void SubWindow::setGuiFunction(const GuiFunc& guiFunction)
	{
		data->guiFunc = guiFunction;
	}

	void gloops::SubWindow::setUpdateFunction(const UpdateFunc& upFunc)
	{
		data->updateFunc = upFunc;
	}

	void gloops::SubWindow::setRenderingFunction(const RenderingFunc& renderFunc)
	{
		data->renderingFunc = renderFunc;
	}

	void SubWindow::show(const Window & win)
	{
		data->renderComponent->show(win);
	}

	void SubWindow::setFlags(WinFlags flags)
	{
		data->_flags = flags;
	}

	bool& gloops::SubWindow::active()
	{
		return data->renderComponent->isActive();
	}

	v4f& gloops::SubWindow::clearColor()
	{
		return data->clearColor;
	}

	WindowComponent& gloops::SubWindow::getRenderComponent()
	{
		return *data->renderComponent;
	}

	WindowComponent& gloops::SubWindow::getGuiComponent()
	{
		return *data->guiComponent;
	}

	void gloops::SubWindow::menuGui()
	{
		data->menuGui();
	}

	void SubWindow::debugWin()
	{
		data->debugWin();
	}

}
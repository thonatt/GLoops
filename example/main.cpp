
#include <gloops/Window.hpp>
#include <gloops/Mesh.hpp>
#include <gloops/Camera.hpp>
#include <gloops/Shader.hpp>

#include <map>

using namespace gloops_types;

gloops::Image3b getCheckersImage(int w, int h, int r)
{
	gloops::Image3b image;
	image.allocate(w, h);
	for (int i = 0; i < h; ++i) {
		for (int j = 0; j < w; ++j) {
			int c = (i / r + j / r) % 2 ? 255 : 0;
			image.pixel(j, i) = v3b(c, c, c);
		}
	}
	return image;
}

gloops::SubWindow checker_subwin() 
{
	static bool doReadback = false, linear = false;
	static int currentCol;
	static v2d uv;

	static gloops::Texture checkers_tex(getCheckersImage(50, 50, 5));

	static GLint filter = GL_NEAREST;

	auto sub = gloops::SubWindow("checkers",
		v2i(600, 600),
		[&] 
	{
		if (ImGui::Checkbox("linear interpolation", &linear)) {
			filter = linear ? GL_LINEAR : GL_NEAREST;
		}
		std::stringstream s;
		s << "cursor current color : ";
		if (doReadback) {
			s << (currentCol ? "black" : "white");
		} else {
			s << "cursor out of texture viewport";
		}
		ImGui::Text(s.str());
	},
		[&](const gloops::Input& i) 
	{
		doReadback = i.insideViewport();
		uv = i.mousePosition().cwiseQuotient(i.viewport().diagonal());
	},
		[&](gloops::Framebuffer& dst) 
	{
		gloops::gl_check();
		dst.blitFrom(checkers_tex, GL_COLOR_ATTACHMENT0, filter);
		gloops::gl_check();

		if (doReadback) {
			gloops::Image3b tmp;
			v2i coords = uv.cwiseProduct(v2d(dst.w(), dst.h())).template cast<int>();
			dst.readBack(tmp, 1, 1, coords.x(), coords.y());
			currentCol = static_cast<int>(tmp.pixel(0, 0).x() / 255);
		}

	});

	sub.renderComponent.backgroundColor = v4f(0.3f, 0.3f, 0.3f, 1.0f);

	return sub;
}

gloops::SubWindow& mesh_viewer_subwin()
{
	static gloops::MeshGL
		sphere = gloops::Mesh::getSphere().setScaling(2.0f),
		cube = gloops::Mesh::getCube(gloops::BBox3f(v3f(-3, -3, -3), v3f(-1, -1, -1))),
		axis = gloops::MeshGL::getAxis();

	static gloops::Raycaster raycaster;
	raycaster.addMesh(sphere, cube);

	static gloops::Trackballf tb = gloops::Trackballf::fromMesh(sphere, cube);
	tb.setRaycaster(raycaster);

	static gloops::ShaderCollection shaders;

	static v4f color = { 1.0f, 0.0f, 1.0f, 0.5f };

	static std::map<std::string, gloops::MeshGL> meshes;
	meshes["cube"] = cube;
	meshes["sphere"] = sphere;
	meshes["axis"] = axis;

	static gloops::SubWindow mesh_viewer("mesh viewer", v2i(800, 600));
	
	mesh_viewer.setGuiFunction([&]
	{
		auto colPicker = [&](const std::string& s, float* ptr, uint flag = 0) {
			ImGui::Text(s);
			ImGui::SameLine();
			ImGui::ColorEdit4(s.c_str(), ptr, flag | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
		};

		colPicker("sphere color : ", &color[0]);
		colPicker("clear color", &mesh_viewer.clearColor[0], ImGuiColorEditFlags_NoAlpha);
		colPicker("background color", &mesh_viewer.renderComponent.backgroundColor[0], ImGuiColorEditFlags_NoAlpha);
		colPicker("gui background color", &mesh_viewer.guiComponent.backgroundColor[0], ImGuiColorEditFlags_NoAlpha);

		ImGui::Separator();

		if (ImGui::CollapsingHeader("meshes data")) {
			ImGui::Separator();
			for (const auto& mesh : meshes) {
				std::stringstream header;
				header << mesh.first << "\n";

				const auto& m = mesh.second;
				header << "  num vertices : " << m.getVertices().size() << "\n";
				header << "  num triangles : " << m.getTriangles().size() << "\n";
				ImGui::Text(header.str());

				if (ImGui::TreeNode(("vertex data attributes##" + mesh.first).c_str())) {
					for (const auto& attribute : m.getAttributes()) {
						std::stringstream s;
						const auto& att = attribute.second;
						s << "location : " << att.index << "\n";
						s << "  nchannels : " << att.num_channels << "\n";
						s << "  type : " << att.type << "\n";
						s << "  stride : " << att.stride << "\n";
						s << "  normalized : " << std::boolalpha << (bool)att.normalized << "\n";
						s << "  data ptr : " << att.pointer << "\n";
						ImGui::Text(s.str());
					}
					ImGui::TreePop();
				}
				ImGui::Separator();
			}
		}
	});

	mesh_viewer.setUpdateFunction([&](const gloops::Input& i){
		tb.update(i);
	});

	mesh_viewer.setRenderingFunction([&](gloops::Framebuffer& dst)
	{
		dst.bindDraw();

		gloops::Cameraf eye = tb.getCamera();

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		shaders.renderColoredMesh(eye, axis);

		shaders.renderPhongMesh(eye, cube);

		shaders.renderBasicMesh(eye, sphere, color);

		glDisable(GL_BLEND);
	});

	return mesh_viewer;
}

gloops::SubWindow mesh_modes_subwin()
{
	using namespace gloops;

	static MeshGL torusA = Mesh::getTorus(3, 1), torusB = Mesh::getTorus(3, 1)
		.setTranslation(3 * v3f::UnitY()).setRotation(Eigen::AngleAxis<float>(pi<float>() / 2, v3f::UnitY()));

	torusB.mode = GL_LINE;

	static gloops::Trackballf tb = gloops::Trackballf::fromMeshComputingRaycaster(torusA, torusB);

	static gloops::ShaderCollection shaders;

	return gloops::SubWindow("render modes", v2i(800, 600),
		[&]
	{

		static std::map<GLenum, std::string> modes = {
			{ GL_POINT, "points" },
			{ GL_LINE, "lines" },
			{ GL_FILL, "fill" }
		};

		for (const auto& mode : modes) {
			if (ImGui::RadioButton(mode.second.c_str(), torusB.mode == mode.first)) {
				torusB.mode = mode.first;
			}
			ImGui::SameLine();
		}

	},
		[&](const gloops::Input& i)
	{
		tb.update(i);
	},
		[&](gloops::Framebuffer& dst)
	{
		dst.bindDraw();

		shaders.renderPhongMesh(tb.getCamera(), torusA);
		shaders.renderPhongMesh(tb.getCamera(), torusB);
	});
}

int main(int argc, char** argv)
{
	auto win = gloops::Window("GLoops example");

	auto win_checkers = checker_subwin();
	auto& win_mesh = mesh_viewer_subwin();
	auto win_mesh_modes = mesh_modes_subwin();

	auto demoOptions = gloops::WindowComponent("demo settings", gloops::WindowComponent::Type::GUI, 
		[&] (const gloops::Window& win) {
		ImGui::Checkbox("checkers texture", &win_checkers.active());
		ImGui::Checkbox("mesh viewer", &win_mesh.active());
		ImGui::Checkbox("mesh modes", &win_mesh_modes.active());
	});

	win.renderingLoop([&]{

		demoOptions.show(win);
		
		win_checkers.show(win);
		win_mesh.show(win);
		win_mesh_modes.show(win);
	});

}
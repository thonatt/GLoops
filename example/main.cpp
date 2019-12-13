
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

	return gloops::SubWindow("checkers",
		v2i(600, 600),
		[&] 
	{
		if (ImGui::Checkbox("linear interpolation", &linear)) {
			filter = linear ? GL_LINEAR : GL_NEAREST;
		}
		std::stringstream s;
		if (doReadback) {
			s << "current color : " << (currentCol ? "black" : "white");
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
}

gloops::SubWindow mesh_viewer_subwin() 
{
	static gloops::MeshGL
		sphere = gloops::MeshGL::getSphere(1, v3f(1, 1, 1)),
		cube = gloops::MeshGL::getCube(gloops::BBox3f(v3f(-3, -3, -3), v3f(-1, -1, -1)));
	
	static gloops::Raycaster raycaster;
	raycaster.addMesh(sphere, cube);

	static gloops::Trackballf tb = gloops::Trackballf::fromMesh(sphere);
	tb.setRaycaster(raycaster);

	static gloops::ShaderCollection shaders;

	static v4f color = { 1.0f, 0.0f, 1.0f, 0.5f };
	static v4f background_color = { 0.9f, 0.9f, 0.9f, 1.0f };

	static std::map<std::string, gloops::MeshGL> meshes;
	meshes["cube"] = cube;
	meshes["sphere"] = sphere;

	return gloops::SubWindow("mesh viewer", v2i(800, 600),
		[&]
	{
		auto colPicker = [&](const std::string& s, float* ptr, uint flag = 0) {
			ImGui::Text(s);
			ImGui::SameLine();
			ImGui::ColorEdit4(s.c_str(), ptr, flag | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
		};

		colPicker("sphere color : ", &color[0]);
		colPicker("background color", &background_color[0], ImGuiColorEditFlags_NoAlpha);

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
	},
		[&](const gloops::Input& i)
	{
		tb.update(i);
	},
		[&](gloops::Framebuffer& dst) 
	{
		dst.clear(background_color);
		dst.bindDraw();

		gloops::Cameraf eye = tb.getCamera();

		shaders.mvp = eye.viewProj();
		shaders.cam_pos = eye.position();
		shaders.light_pos = eye.position();
		shaders.color = color;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		shaders.get(gloops::ShaderCollection::Name::COLORED_MESH).use();
		gloops::MeshGL::getAxis().draw();

		shaders.get(gloops::ShaderCollection::Name::PHONG).use();
		cube.draw();

		shaders.get(gloops::ShaderCollection::Name::BASIC).use();
		sphere.draw();

		glDisable(GL_BLEND);
	});
}

int main(int argc, char** argv)
{
	auto win = gloops::Window("GLoops example");

	auto win_checkers = checker_subwin();
	auto win_mesh = mesh_viewer_subwin();

	bool showImGuiDemo = false, showCheckers = false, showMesh = false;

	win.renderingLoop([&]{
		if (ImGui::Begin("demo settings")) {
			ImGui::Checkbox("ImGui demo", &showImGuiDemo);
			ImGui::Checkbox("checkers texture", &showCheckers);
			ImGui::Checkbox("mesh viewer", &showMesh);
		}
		ImGui::End();

		if (showImGuiDemo) {
			win.showImguiDemo();
		}

		if (showCheckers) {
			win_checkers.show(win);
		}

		if (showMesh) {
			win_mesh.show(win);
		}
		
	});

}
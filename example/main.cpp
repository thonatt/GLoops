#include <gloops/Window.hpp>
#include <gloops/Mesh.hpp>
#include <gloops/Camera.hpp>
#include <gloops/Shader.hpp>

#include <map>
#include <limits>

using namespace gloops;

SubWindow checker_subwin() 
{
	static bool doReadback = false;
	static v2d uv;
	static v4f color; 

	static Texture
		checkers_tex(checkersTexture(50, 50, 5)),
		perlinTex(perlinNoise(200, 200, 5)),
		zoom;

	static GLint filter = GL_NEAREST;

	enum class Mode { CHECKERS, PERLIN };
	struct ModeData { 
		Texture tex;
		std::string name;
	};

	static const std::map<Mode, ModeData> modes = {
		{ Mode::CHECKERS, { checkers_tex, "Checkers"}},
		{ Mode::PERLIN, { perlinTex, "Perlin noise"}},
	};

	static Mode currentMode = Mode::PERLIN;

	auto sub = SubWindow("Texture viewer",
		v2i(600, 600),
		[&]
	{
		int mode_id = 0;
		for (const auto& mode : modes) {
			if (ImGui::RadioButton(mode.second.name.c_str(), currentMode == mode.first )) {
				currentMode = mode.first;
			}
			if (mode_id != ((int)modes.size() - 1)) {
				ImGui::SameLine();
			}
			++mode_id;
		}

		std::stringstream s;
		s << "texture zoom : " << (doReadback ? "" : " cursor out of viewport !");
		ImGui::Text(s.str());
		if (doReadback) {
			ImGui::SameLine();
			ImGui::Image(zoom.getId(), { 100,100 }, { 0,1 }, { 1,0 });
		}
	},
		[&](const Input& i) 
	{
		doReadback = i.insideViewport();
		uv = i.mousePosition().cwiseQuotient(i.viewport().diagonal());
	},
		[&](Framebuffer& dst) 
	{
		dst.blitFrom(modes.at(currentMode).tex, GL_COLOR_ATTACHMENT0, filter);

		if (doReadback) {
			v2i coords = uv.cwiseProduct(v2d(dst.w(), dst.h())).template cast<int>();
			coords.y() = dst.h() - 1 - coords.y();

			const int r = 10;
			v2i xy = v2i(std::clamp(coords.x() - r, 0, dst.w() - 1), std::clamp(coords.y() - r, 0, dst.h() - 1));

			Image4b tmp(2 * r + 1, 2 * r + 1);
			tmp.setTo(v4b(0, 0, 0, 0));
			dst.readBack(tmp, 2 * r + 1, 2 * r + 1, xy[0], xy[1]);
			zoom.update(tmp, DefaultTexParams<Image4b>().disableMipmap().setMagFilter(GL_NEAREST));
		}

	});

	sub.getRenderComponent().backgroundColor = v4f(0.3f, 0.3f, 0.3f, 1.0f);
	sub.updateWhenNoFocus = true;

	return sub;
}

SubWindow mesh_viewer_subwin()
{
	static MeshGL
		sphere = Mesh::getSphere().setScaling(2.0f),
		cube = Mesh::getCube(BBox3f(v3f(-3, -3, -3), v3f(-1, -1, -1))),
		axis = MeshGL::getAxis();

	static Raycaster raycaster;
	raycaster.addMesh(sphere, cube);

	static Trackballf tb = Trackballf::fromMesh(sphere, cube);
	tb.setRaycaster(raycaster);

	static ShaderCollection shaders;

	static v4f color = { 1.0f, 0.0f, 1.0f, 0.5f };

	static std::map<std::string, MeshGL> meshes;
	meshes["cube"] = cube;
	meshes["sphere"] = sphere;
	meshes["axis"] = axis;

	static SubWindow mesh_viewer("Mesh viewer", v2i(800, 600));
	
	mesh_viewer.setGuiFunction([&]
	{
		auto colPicker = [&](const std::string& s, float* ptr, uint flag = 0) {
			ImGui::Text(s);
			ImGui::SameLine();
			ImGui::ColorEdit4(s.c_str(), ptr, flag | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
		};

		colPicker("sphere color : ", &color[0]);
		ImGui::SameLine();
		colPicker("clear color", &mesh_viewer.clearColor[0], ImGuiColorEditFlags_NoAlpha);
		colPicker("background color", &mesh_viewer.getRenderComponent().backgroundColor[0], ImGuiColorEditFlags_NoAlpha);
		ImGui::SameLine();
		colPicker("gui background color", &mesh_viewer.getGuiComponent().backgroundColor[0], ImGuiColorEditFlags_NoAlpha);

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

	mesh_viewer.setUpdateFunction([&](const Input& i){
		tb.update(i);
	});

	mesh_viewer.setRenderingFunction([&](Framebuffer& dst)
	{
		dst.bindDraw();

		Cameraf eye = tb.getCamera();

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		shaders.renderColoredMesh(eye, axis);

		shaders.renderPhongMesh(eye, cube);

		shaders.renderBasicMesh(eye, sphere, color);

		glDisable(GL_BLEND);
	});

	return mesh_viewer;
}

SubWindow mesh_modes_subwin()
{
	static MeshGL meshA = Mesh::getTorus(3, 1),
		meshB = Mesh::getTorus(3, 1).setTranslation(3 * v3f::UnitY()).setRotation(v3f(0, pi<float>() / 2, 0)),
		meshC = Mesh::getSphere().setScaling(3).setTranslation(-8 * v3f(0, 1, 0)),
		meshD = Mesh::getCube().setScaling(2).setTranslation(v3f(-5, -5, 5)).setRotation(v3f(1, 1, 1));

	static Texture tex(perlinNoise(1024, 1024, 5));
	
	static Trackballf tb = Trackballf::fromMeshComputingRaycaster(meshA, meshB, meshC, meshD);

	static ShaderCollection shaders;

	static ShaderProgram uv_shader;
	uv_shader.init(R"(
				#version 420
				layout(location = 0) in vec3 position;
				layout(location = 1) in vec2 uv;
				uniform mat4 mvp;
				out vec2 in_uv; 
				void main() {
					in_uv = uv;
					gl_Position = mvp * vec4(position, 1.0);
				}
			)",
		R"(
				#version 420
				layout(location = 0) out vec4 outColor;
				in vec2 in_uv; 
				void main(){
					outColor = vec4(in_uv,0,1);
				}
			)"
	);
	uv_shader.addUniforms(shaders.mvp);

	enum class Mode { POINT, LINE, PHONG, UVS, TEXTURED };
	struct ModeMesh {
		Mode mode;
		MeshGL mesh;
		v4f color = v4f(1, 0, 0, 1);
		bool showBoundingBox = true;
	};

	static std::map<int, ModeMesh> meshes =
	{
		{ 0, { Mode::UVS, meshA } },
		{ 1, { Mode::UVS, meshB } },
		{ 2, { Mode::UVS, meshC } },
		{ 3, { Mode::PHONG, meshD } }
	};

	static int selectedMesh = -1;

	for (auto& m : meshes) {
		m.second.mesh.setCPUattribute("id", std::vector<int>(m.second.mesh.getVertices().size(), m.first));
	}

	static const std::map<Mode, std::string> modes = {
		{ Mode::POINT, "points" },
		{ Mode::LINE, "lines" },
		{ Mode::PHONG, "phong" },
		{ Mode::UVS, "uvs" },
		{ Mode::TEXTURED, "textured" }
	};

	static Hit hit;

	auto sub = SubWindow("Render modes", v2i(800, 600),
		[&]
	{

		if (selectedMesh < 0) {
			ImGui::Text("no mesh selected");
		} else {
			auto& mesh = meshes.at(selectedMesh);
			for (const auto& mode : modes) {
				if (ImGui::RadioButton((mode.second + "##" + std::to_string(selectedMesh)).c_str(), mesh.mode == mode.first)) {
					mesh.mode = mode.first;
				}
				ImGui::SameLine();
			}
			ImGui::Checkbox(("bbox##" + std::to_string(selectedMesh)).c_str(), &mesh.showBoundingBox);

			static v3f pos, rot;
			pos = mesh.mesh.transform().translation();
			if (ImGui::SliderFloat3("translation", pos.data(), -5, 5)) {
				mesh.mesh.setTranslation(pos);
			}
			rot = mesh.mesh.transform().eulerAngles();
			if (ImGui::SliderFloat3("rotation", rot.data(), 0, 2.0f*pi<float>())) {
				mesh.mesh.setRotation(rot);
			}
			static float scale;
			scale = mesh.mesh.transform().scaling()[0];
			if (ImGui::SliderFloat("scale", &scale, 0, 5)) {
				mesh.mesh.setScaling(scale);
			}
		}
	},
		[&](const Input& i)
	{
		tb.update(i);
		RaycastingCameraf eye(tb.getCamera(), i.viewport().diagonal().template cast<int>());
		hit = tb.getRaycaster().intersect(eye.getRay(i.mousePosition<float>()));

		if (hit.successful()) {
			ImGui::BeginTooltip();
			ImGui::Text("Click to select : " + std::to_string(hit.instanceId()));
			ImGui::EndTooltip();

			if (i.buttonClicked(GLFW_MOUSE_BUTTON_LEFT)) {
				int id = (int)tb.getRaycaster().interpolate(hit, &Mesh::getAttribute<int>, "id");
				selectedMesh = (selectedMesh == id ? -1 : id);
			}
		}
	},
		[&](Framebuffer& dst)
	{
		dst.bindDraw();

		Cameraf eye = tb.getCamera();
		for (auto& m : meshes) {

			MeshGL& mesh = m.second.mesh;
			switch (m.second.mode)
			{
			case Mode::PHONG: {
				mesh.mode = GL_FILL;
				shaders.renderPhongMesh(eye, mesh);
				break;
			}
			case Mode::UVS: {
				mesh.mode = GL_FILL;
				shaders.mvp = tb.getCamera().viewProj() * mesh.model();
				uv_shader.use();
				mesh.draw();
				break;
			}
			case Mode::TEXTURED: {
				mesh.mode = GL_FILL;
				shaders.renderTexturedMesh(eye, mesh, tex);
				break;
			}
			case Mode::POINT: {
				mesh.mode = GL_POINT;
				shaders.renderBasicMesh(eye, mesh, m.second.color);
				break;
			}
			case Mode::LINE: {
				mesh.mode = GL_LINE;
				shaders.renderBasicMesh(eye, mesh, m.second.color);
				break;
			}
			default: {}
			}

			if (m.first == selectedMesh) {
				shaders.renderBasicMesh(eye, MeshGL::getCubeLines(mesh.getBoundingBox()), v4f(0, 1, 0, 1));
			} else if (m.second.showBoundingBox) {
				shaders.renderBasicMesh(eye, MeshGL::getCubeLines(mesh.getBoundingBox()), v4f(1, 0, 0, 1));
			}
		}
	});

	return sub;
}

SubWindow rayTracingWin()
{
	//setup Cornell Box
	static MeshGL outerBox = Mesh::getCube().invertFaces(),
		innerBoxA = Mesh::getCube().setScaling(0.4f).setTranslation(v3f(-0.4f, -0.3f, -0.1f)),
		innerBoxB = Mesh::getCube().setScaling(0.3f).setTranslation(v3f(0.4f, -0.5f, 0.1f));

	std::vector<v3f> colorsOut(outerBox.getVertices().size(), v3f::Ones());
	for (int i = 0; i < 4; ++i) {
		colorsOut[8 + i] = v3f(1, 0, 0);
		colorsOut[12 + i] = v3f(0, 1, 0);
	}

	outerBox.setColors(colorsOut);
	innerBoxA.setColors(std::vector<v3f>(innerBoxA.getVertices().size(), { 0,0,1 }));
	innerBoxB.setColors(std::vector<v3f>(innerBoxB.getVertices().size(), { 1,0,1 }));

	static const v3f lightPosition = 0.9*v3f::UnitY(), lightColor = v3f::Ones();

	//setup raycaster, camera and buffers
	static Raycaster raycaster;
	raycaster.addMesh(outerBox, innerBoxA, innerBoxB);

	static Image1b hits;
	static auto hitMask = [&](int x, int y) { return (bool)hits.at(x, y); };

	static Trackballf tb = Trackballf::fromMeshComputingRaycaster(outerBox).setLookAt(v3f(0.8f, 0.5f, 2.3f), v3f::Zero());
	static Cameraf previousCam;

	static Image3f currentSamplesAverage;
	static Texture tex;

	enum class Mode { COLOR, DIRECT_LIGHT, NORMAL, POSITION, DEPTH };
	static const std::map<Mode, std::string> modes = {
		{ Mode::DEPTH, "Depth" },
		{ Mode::POSITION, "Position" },
		{ Mode::NORMAL, "Normal" },
		{ Mode::DIRECT_LIGHT, "Direct" },
		{ Mode::COLOR, "Color" }
	};
	static Mode mode = Mode::COLOR;

	static int numBounces = 2, maxNumSamples = 8, samplesPerPixel = 1, currentNumSamples = 0;
	static bool resetRayCasting = true, useMT = false;

	static const int w = 360, h = 240;
	static SubWindow sub = SubWindow("Ray tracing", v2i(w, h));

	sub.setGuiFunction([&] {
		int mode_id = 0;
		for (const auto& m : modes) {
			if (ImGui::RadioButton(m.second.c_str(), mode == m.first)) {
				mode = m.first;
				resetRayCasting = true;
			}
			if (mode_id != ((int)modes.size() - 1)) {
				ImGui::SameLine();
			}
			++mode_id;
		}
		ImGui::Separator();
		ImGui::PushItemWidth(150);
		resetRayCasting |= ImGui::SliderInt("num bounces", &numBounces, 1, 3);
		resetRayCasting |= ImGui::SliderInt("samples per pixel per frame", &samplesPerPixel, 1, 4);
		resetRayCasting |= ImGui::SliderInt("max samples per pixel", &maxNumSamples, 1, 64);
		ImGui::PopItemWidth();

		resetRayCasting |= ImGui::Checkbox(
			("use multi-threading, " + std::to_string(std::thread::hardware_concurrency()) + " available cores").c_str(),
			&useMT
		);
		std::stringstream s;
		s << "current num samples per pixel : " << currentNumSamples << " / " << maxNumSamples;
		ImGui::Text(s.str());
	});

	sub.setUpdateFunction([&](const Input& i) {
		tb.update(i);
	});

	sub.setRenderingFunction([&](Framebuffer& dst) {
		hits.resize(w, h);
		currentSamplesAverage.resize(w, h);

		RaycastingCameraf cam = RaycastingCameraf(tb.getCamera(), w, h);
		resetRayCasting |= (previousCam != cam);

		if (resetRayCasting) {
			currentNumSamples = 0;
			currentSamplesAverage.setTo(v3f(0, 0, 0));
			hits.setTo(Vec<uchar, 1>(1));
			resetRayCasting = false;
		} 

		if (useMT) {
			raycaster.checkScene();
		}

		if (currentNumSamples >= maxNumSamples) {
			dst.blitFrom(tex);
			return;
		}

		auto rowJob = [&](int i) {
			for (int j = 0; j < w; ++j) {
				int numSamples = currentNumSamples;
				
				for (int s = 0; s < samplesPerPixel; ++s) {
					RayT<float> ray = cam.getRay(v2f(j, h - 1 - i) + 0.5 * (randomVec<float,2>() + v2f(1, 1)));

					bool continueRT = true;
					v3f sampleColor = v3f::Zero();
					v3f color = v3f::Ones();

					for (int b = 0; (b < numBounces) && continueRT; ++b) {

						Hit hit = raycaster.intersect(ray, 0.001f);
						bool successful = hit.successful();

						hits.at(j, i) |= (uchar)successful;
						if (!successful) {
							continueRT = false; continue;
						}

						float d = hit.distance();
						v3f p = ray.pointAt(d);
						v3f n = raycaster.interpolate(hit, &Mesh::getNormals).normalized();
						v3f col = raycaster.interpolate(hit, &Mesh::getColors);

						switch (mode) {
						case Mode::DEPTH: {
							sampleColor = v3f(d, d, d);
							continueRT = false; continue;
						}
						case Mode::POSITION: {
							sampleColor = p;
							continueRT = false; continue;
						}
						case Mode::NORMAL: {
							sampleColor = n;
							continueRT = false; continue;
						}
						default: {
							v3f dir;
							float distToLight;
							if (raycaster.visible(p, lightPosition, dir, distToLight)) {
								const float diffuse = std::max(dir.dot(n), 0.0f);
								const float attenuation = std::clamp<float>(1.0f - (distToLight * distToLight) / (2.5f * 2.5f), 0, 1);

								if (mode == Mode::DIRECT_LIGHT) {
									const float l = diffuse * attenuation;
									sampleColor = v3f(l, l, l);
									continueRT = false; continue;
								}

								v3f illumination = (diffuse * attenuation) * lightColor;
								color = color.cwiseProduct(col);
								sampleColor += color.cwiseProduct(illumination);

							} else if (mode == Mode::DIRECT_LIGHT) {
								sampleColor = v3f::Zero();
								continueRT = false;
							}
						}
						}

						if (b < (numBounces - 1)) {
							ray = RayT<float>(p, (n + randomUnit<float, 3>()).normalized());
						}
					}

					numSamples += 1;
					currentSamplesAverage.pixel(j, i) += (sampleColor - currentSamplesAverage.pixel(j, i)) / numSamples;
				}
			}
		};

		if (useMT) {
			parallelForEach(0, h, [&](int i) {
				rowJob(i);
			});
		} else {
			for (int i = 0; i < h; ++i) {
				rowJob(i);
			}
		}
		  
		currentNumSamples += samplesPerPixel;
		previousCam = cam;

		Image3b img;
		switch (mode)
		{
		case Mode::COLOR: {
			img = currentSamplesAverage.convert<uchar>(255, 0);
			break;
		}
		case Mode::DEPTH: {
			img = currentSamplesAverage.normalized<uchar>(0, 255, hitMask, 255);
			break;
		}
		case Mode::DIRECT_LIGHT: {
			img = currentSamplesAverage.convert<uchar>(255, 0, hitMask, 50);
			break;
		}
		default:
			img = currentSamplesAverage.convert<uchar>(128, 128, hitMask, 255);
		}

		tex.update(img);
		dst.blitFrom(tex);
	});

	return sub;
}

int main(int argc, char** argv)
{
	auto mainWin = Window("GLoops demos");

	auto win_checkers = checker_subwin();
	auto win_mesh = mesh_viewer_subwin();
	auto win_mesh_modes = mesh_modes_subwin();
	//auto win_raytracing = rayTracingWin();

	auto demoOptions = WindowComponent("Demo settings", WindowComponent::Type::GUI, 
		[&] (const Window& win) {
		ImGui::Checkbox("texture viewer", &win_checkers.active());
		ImGui::SameLine();
		ImGui::Checkbox("mesh viewer", &win_mesh.active());
		ImGui::Checkbox("mesh modes", &win_mesh_modes.active());
		ImGui::SameLine();
		//ImGui::Checkbox("ray tracing", &win_raytracing.active());
	});

	mainWin.renderingLoop([&]{

		demoOptions.show(mainWin);
		
		win_checkers.show(mainWin);
		win_mesh.show(mainWin);
		win_mesh_modes.show(mainWin);
		//win_raytracing.show(mainWin);
	});

}
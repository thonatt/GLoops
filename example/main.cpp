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

	static Mode currentMode = Mode::CHECKERS;

	auto sub = SubWindow("Texture viewer",
		v2i(600, 600),
		[&]
	{
		//if (ImGui::Checkbox("linear interpolation", &linear)) {
		//	filter = linear ? GL_LINEAR : GL_NEAREST;
		//}

		for (const auto& mode : modes) {
			if (ImGui::RadioButton(mode.second.name.c_str(), currentMode == mode.first )) {
				currentMode = mode.first;
			}
		}

		if (doReadback) {
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

			Image4b tmp;
			dst.readBack(tmp, 2 * r + 1, 2 * r + 1, xy[0], xy[1]);
			zoom.update(tmp, DefaultTexParams<Image4b>().setMipmapStatus(false).setMagFilter(GL_NEAREST));
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
		colPicker("clear color", &mesh_viewer.clearColor[0], ImGuiColorEditFlags_NoAlpha);
		colPicker("background color", &mesh_viewer.getRenderComponent().backgroundColor[0], ImGuiColorEditFlags_NoAlpha);
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
	static MeshGL torusA = Mesh::getTorus(3, 1), torusB = Mesh::getTorus(3, 1).setTranslation(3 * v3f::UnitY())
		.setRotation(Eigen::AngleAxis<float>(pi<float>() / 2, v3f::UnitY()));

	static Trackballf tb = Trackballf::fromMeshComputingRaycaster(torusA, torusB);

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

	enum class Mode { POINT, LINE, TRIANGLE, UVS };
	struct ModeMesh {
		Mode mode;
		MeshGL mesh;
	};

	static std::map<uint, ModeMesh> meshes =
	{
		{ 0, { Mode::UVS, torusA } },
		{ 1, { Mode::UVS, torusB } },
	};

	static const std::map<Mode, std::pair<GLenum, std::string>> modes = {
		{ Mode::POINT, { GL_POINT, "points"} },
		{ Mode::LINE, { GL_LINE, "lines" } },
		{ Mode::TRIANGLE, { GL_FILL, "fill" } },
		{ Mode::UVS, { GL_FILL, "uvs" } }
	};

	return SubWindow("Render modes", v2i(800, 600),
		[&]
	{
		for (auto& mesh : meshes) {
			for (auto mode_it = modes.begin(); mode_it != modes.end(); ++mode_it) {
				if (ImGui::RadioButton((mode_it->second.second + "##" + std::to_string(mesh.first)).c_str(), mesh.second.mode == mode_it->first)) {
					mesh.second.mode = mode_it->first;
					mesh.second.mesh.mode = mode_it->second.first;
				}
				if (mode_it != (--modes.end())) {
					ImGui::SameLine();
				}			
			}
			ImGui::Separator();
		}
	},
		[&](const Input& i)
	{
		tb.update(i);
	},
		[&](Framebuffer& dst)
	{
		dst.bindDraw();
		for (const auto& mesh : meshes) {
			const MeshGL& m = mesh.second.mesh;
			if (mesh.second.mode == Mode::UVS) {			
				shaders.mvp = tb.getCamera().viewProj() * m.model();
				uv_shader.use();
				m.draw();
			} else {
				shaders.renderPhongMesh(tb.getCamera(), m);
			}
		}
		
	});
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
	static float currentNumSamples = 0;
	static const float maxNumSamples = 64;
	static Texture tex;

	enum class Mode { COLOR, DIRECT_LIGHT, NORMAL, POSITION, DEPTH };
	static const std::map<Mode, std::string> modes = {
		{ Mode::DEPTH, "DEPTH" },
		{ Mode::POSITION, "POSITION" },
		{ Mode::NORMAL, "NORMAL" },
		{ Mode::DIRECT_LIGHT, "DIRECT_LIGHT" },
		{ Mode::COLOR, "COLOR" }
	};
	static Mode mode = Mode::COLOR;

	static int numBounces = 2, samplesPerPixel = 1;
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
			if ((mode_id % 3) != 2 && mode_id != ((int)modes.size() - 1)) {
				ImGui::SameLine();
			}
			++mode_id;
		}
		ImGui::Separator();
		ImGui::PushItemWidth(150);
		resetRayCasting |= ImGui::SliderInt("num bounces", &numBounces, 1, 3);
		resetRayCasting |= ImGui::SliderInt("samples per pixel", &samplesPerPixel, 1, 4);
		ImGui::PopItemWidth();

		resetRayCasting |= ImGui::Checkbox(
			("use multi-threading, " + std::to_string(std::thread::hardware_concurrency()) + " available cores").c_str(),
			&useMT
		);
		ImGui::Text("current samples per pixels : " + std::to_string((int)currentNumSamples));
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
				float numSamples = currentNumSamples;
				
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

		switch (mode)
		{
		case Mode::DEPTH: {
			auto depth_img = currentSamplesAverage.normalized<uchar, 1>(0, 255, hitMask, 255);
			tex.update(depth_img);
			break;
		}
		case Mode::POSITION: {
			auto positions_img = currentSamplesAverage.convert<uchar>(128, 128, hitMask, 255);
			tex.update(positions_img);
			break;
		}		
		case Mode::NORMAL: {
			auto normals_img = currentSamplesAverage.convert<uchar>(128, 128, hitMask, 255);
			tex.update(normals_img);
			break;
		}
		case Mode::DIRECT_LIGHT: {
			auto is_light_visible_img = currentSamplesAverage.convert<uchar>(255, 0, hitMask, 50);
			tex.update(is_light_visible_img);
			break;
		}
		default:
			auto img = currentSamplesAverage.convert<uchar>(255, 0);
			tex.update(img);
		}

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
	auto win_raytracing = rayTracingWin();

	auto demoOptions = WindowComponent("demo settings", WindowComponent::Type::GUI, 
		[&] (const Window& win) {
		ImGui::Checkbox("checkers texture", &win_checkers.active());
		ImGui::Checkbox("mesh viewer", &win_mesh.active());
		ImGui::Checkbox("mesh modes", &win_mesh_modes.active());
		ImGui::Checkbox("ray tracing", &win_raytracing.active());
	});

	mainWin.renderingLoop([&]{

		demoOptions.show(mainWin);
		
		win_checkers.show(mainWin);
		win_mesh.show(mainWin);
		win_mesh_modes.show(mainWin);
		win_raytracing.show(mainWin);
	});

}
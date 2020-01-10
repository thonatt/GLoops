#include <gloops/Window.hpp>
#include <gloops/Mesh.hpp>
#include <gloops/Camera.hpp>
#include <gloops/Shader.hpp>

#include <map>
#include <limits>

using namespace gloops;

bool colPicker(const std::string& s, float* ptr, uint flag = 0) {
	ImGui::Text(s);
	ImGui::SameLine();
	return ImGui::ColorEdit4(s.c_str(), ptr, flag | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
};

static Window mainWin = Window("GLoops demos");

static TexParams texParams = TexParams().enableMipmap().setMagFilter(GL_NEAREST);

Texture checkers_tex = Texture(checkersTexture(80, 80, 10), texParams), perlinTex, 
	kittenTex = Texture::fromPath("../kitten.png", texParams);

enum class TexMode { CHECKERS, PERLIN, KITTEN };
struct ModeData {
	Texture tex;
	std::string name;
};

static std::map<TexMode, ModeData> modes = {
	{ TexMode::CHECKERS, { checkers_tex, "Checkers" }},
	{ TexMode::PERLIN, { perlinTex, "Perlin noise" }},
	{ TexMode::KITTEN, { kittenTex, "Kitten png" }},
};

static TexMode currentMode = TexMode::KITTEN;

Texture& currentTex() {
	return modes.at(currentMode).tex;
}

SubWindow checker_subwin() 
{
	static bool doReadback = false;
	static v2d uv;
	static v4f color; 

	static ShaderCollection shaders;

	static const std::map<GLenum, std::string> wraps = {
		{ GL_REPEAT, "REPEAT" },
		{ GL_MIRRORED_REPEAT , "MIRRORED_REPEAT" },
		{ GL_CLAMP_TO_EDGE, "CLAMP_TO_EDGE" },
		{ GL_CLAMP_TO_BORDER, "CLAMP_TO_BORDER" },
	};

	static Texture zoom;

	static const std::map<GLenum, std::string> mag_filters = {
		{ GL_NEAREST , "NEAREST" },
		{ GL_LINEAR, "LINEAR" },
	};

	static const std::map<GLenum, std::string> min_filters = {
		//{ GL_NEAREST, "NEAREST" },
		//{ GL_LINEAR , "LINEAR" },
		{ GL_NEAREST_MIPMAP_NEAREST , "N_MIPMAP_N" },
		{ GL_LINEAR_MIPMAP_NEAREST , "L_MIPMAP_N" },
		{ GL_NEAREST_MIPMAP_LINEAR , "N_MIPMAP_L" },
		{ GL_LINEAR_MIPMAP_LINEAR , "L_MIPMAP_L" },
	};

	static const std::map<GLenum, std::string> channels = {
		{ GL_RED , "GL_RED" },
		{ GL_GREEN , "GL_GREEN" },
		{ GL_BLUE , "GL_BLUE" },
	};

	static v2f texCenter = v2f(0, 0), previousCenter = v2f(0,0), clickedPosition = v2f(0, 0);
	static float lod = 0, zoomLevel = 1.3f;

	auto sub = SubWindow("Texture viewer",
		v2i(400, 400),
		[&]
	{
		int mode_id = 0;
		for (const auto& mode : modes) {
			if (ImGui::RadioButton(mode.second.name.c_str(), currentMode == mode.first)) {
				currentMode = mode.first;
			}
			if (mode_id != ((int)modes.size() - 1)) {
				ImGui::SameLine();
			}
			++mode_id;
		}

		switch (currentMode)
		{
		case TexMode::PERLIN: {
			static v3f colA = { 0,0,0 }, colB = { 1,1,1 };
			static bool colChanged = true;
			static Image1f perlin;
			
			ImGui::Separator();
			if (ImGui::Button("Shuffle") || colChanged) {
				perlin = 0.5 * perlinNoise(80, 80, 5) + 0.5;
				colChanged = true;
			}

			ImGui::SameLine();
			colChanged |= colPicker("first", &colA[0]);
			ImGui::SameLine();
			colChanged |= colPicker("second", &colB[0]);
			if (colChanged) {	
				Image3b img = (perlin * colA + (1 - perlin) * colB).convert<uchar>(255);
				perlinTex.update(img, texParams);
				colChanged = false;
			}
		}
		default:
			break;
		}

		ImGui::Separator();
		ImGui::Text("Wrapping : ");
		for (const auto& wrap : wraps) {
			ImGui::SameLine();
			if (ImGui::RadioButton(wrap.second.c_str(), texParams.getWrapS() == wrap.first)) {
				texParams.setWrapAll(wrap.first);
			}
		}

		static bool fixLods = false;
		ImGui::ItemWithSize(150, [&] { ImGui::SliderFloat("zoom", &zoomLevel, 0.6f, 10.0f); });
		ImGui::SameLine();
		if (ImGui::Checkbox("Fixed lod", &fixLods)) {
			lod = fixLods ? 0.0f : -1.0f;
		}
		if (fixLods) {
			ImGui::SameLine();
			ImGui::ItemWithSize(150, [&] { ImGui::SliderFloat("##lod", &lod, 0, float(currentTex().nLods() - 1)); });
		}

		ImGui::Text("Mag filtering : ");
		for (const auto& filter : mag_filters) {
			ImGui::SameLine();
			if (ImGui::RadioButton((filter.second + "##mag").c_str(), texParams.getMagFilter() == filter.first)) {
				texParams.setMagFilter(filter.first);
			}
		}

		ImGui::Text("Min filtering : ");
		for (const auto& filter : min_filters) {
			ImGui::SameLine();
			if (ImGui::RadioButton((filter.second + "##min").c_str(), texParams.getMinFilter() == filter.first)) {
				texParams.setMinFilter(filter.first);
			}
		}

		static std::array<GLint, 4> mask;
		mask = texParams.getSwizzleMask();
		ImGui::Text("Channel swizzling");
		ImGui::SameLine();
		ImGui::ItemWithSize(210, [&] {
			if (ImGui::SliderInt3("##swizzling", mask.data(), GL_RED, GL_BLUE)) {
				texParams.setSwizzleMask(mask);
			}
		});
		for (int c = 0; c < 3; ++c) {
			ImGui::SameLine();
			ImGui::Text(channels.at(mask[c]));
		}
		
	},
		[&](const Input& i)
	{
		doReadback = i.insideViewport();
		uv = i.mousePosition().cwiseQuotient(i.viewport().diagonal());
		currentTex().update(texParams);

		zoomLevel *= std::pow(1.1f, -(float)i.scrollY());
		zoomLevel = std::clamp(zoomLevel, 0.6f, 10.0f);

		const v2f mPos = (2 * zoomLevel - 1.0f) * i.mousePosition().cwiseQuotient(i.viewport().diagonal()).template cast<float>();
		if (i.buttonClicked(GLFW_MOUSE_BUTTON_RIGHT)) {
			clickedPosition = mPos;
			previousCenter = texCenter;
		}
		if (i.buttonActive(GLFW_MOUSE_BUTTON_RIGHT)) {
			texCenter = previousCenter + clickedPosition - mPos;
		}
	},
		[&](Framebuffer& dst)
	{
		//dst.blitFrom(modes.at(currentMode).tex, GL_COLOR_ATTACHMENT0, filter);

		static MeshGL screenQuad;
		const v2f uvCenter = texCenter;
		const v2f tl = uvCenter + v2f(1.0f - zoomLevel, zoomLevel), br = uvCenter + v2f(zoomLevel, 1.0f - zoomLevel);
		screenQuad = MeshGL::quad({ 0,0,0 }, { 1,1,0 }, { -1,1, 0 }, tl, br);

		dst.bindDraw();
		shaders.renderTexturedMesh(screenQuad, currentTex(), 1.0f, lod);

		if (doReadback) {
			v2i coords = uv.cwiseProduct(v2d(dst.w(), dst.h())).template cast<int>();
			coords.y() = dst.h() - 1 - coords.y();

			const int r = 10;
			const v2i xy = coords - v2i(r, r);

			Image4b tmp(2 * r + 1, 2 * r + 1);
			tmp.setTo(v4b(0, 0, 0, 0));
			dst.readBack(tmp, 2 * r + 1, 2 * r + 1, xy[0], xy[1]);
			zoom.update(tmp, DefaultTexParams<Image4b>().disableMipmap().setMagFilter(GL_NEAREST));

			ImGui::BeginTooltip();
			ImGui::Image(zoom.getId(), { 100, 100 }, { 0,1 }, { 1,0 });
			std::stringstream s;
			s << "click " << clickedPosition.transpose() << std::endl;
			s << "texcenter " << texCenter.transpose() << std::endl;
			s << "prev " << previousCenter.transpose() << std::endl;
			ImGui::Text(s);
			ImGui::EndTooltip();
		}

	});

	sub.getRenderComponent().backgroundColor = v4f(0.3f, 0.3f, 0.3f, 1.0f);
	sub.updateWhenNoFocus = true;

	return sub;
}

SubWindow mesh_modes_subwin()
{
	static MeshGL meshA = Mesh::getTorus(3, 1),
		meshB = Mesh::getTorus(3, 1).setTranslation(3 * v3f::UnitY()).setRotation(v3f(0, pi<float>() / 2, 0)),
		meshC = Mesh::getSphere().setScaling(3).setTranslation(-8 * v3f(0, 1, 0)),
		meshD = Mesh::getCube().setScaling(2).setTranslation(v3f(-5, -5, 5)).setRotation(v3f(1, 1, 1));

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

	enum class Mode { POINT, LINE, PHONG, UVS, COLORED, TEXTURED };
	struct ModeMesh {
		Mode mode;
		MeshGL mesh;
		v4f color = v4f(1, 0, 0, 0.5f);
		bool showNormals = false;
	};
	static bool showAllBB = true;

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
		{ Mode::COLORED, "color" },
		{ Mode::TEXTURED, "textured" }
	};

	static Hit hit;

	auto sub = SubWindow("Render modes", v2i(400, 400),
		[&]
	{
		ImGui::Checkbox("Show AABBs", &showAllBB);

		if (selectedMesh < 0) {
			ImGui::Text("No mesh selected");
			return;
		}
	
		auto& mesh = meshes.at(selectedMesh);

		ImGui::SameLine();
		ImGui::Checkbox("Show normals", &mesh.showNormals);

		if (mesh.mode == Mode::COLORED || mesh.mode == Mode::POINT || mesh.mode == Mode::LINE) {
			ImGui::SameLine();
			ImGui::ColorEdit4("Selected mesh color", &mesh.color[0], ImGuiColorEditFlags_NoInputs);
		}

		ImGui::Separator();

		int mode_id = 0;
		for (const auto& mode : modes) {
			if (ImGui::RadioButton((mode.second + "##" + std::to_string(selectedMesh)).c_str(), mesh.mode == mode.first)) {
				mesh.mode = mode.first;
			}
			if (mode_id != ((int)modes.size() - 1)) {
				ImGui::SameLine();
			}			
			++mode_id;
		}

		static v3f pos, rot;
		pos = mesh.mesh.transform().translation();
		if (ImGui::SliderFloat3("Translation", pos.data(), -5, 5)) {
			mesh.mesh.setTranslation(pos);
		}
		rot = mesh.mesh.transform().eulerAngles();
		if (ImGui::SliderFloat3("Rotation", rot.data(), -2.0f * pi<float>(), 2.0f * pi<float>())) {
			mesh.mesh.setRotation(rot);
		}
		static float scale;
		scale = mesh.mesh.transform().scaling()[0];
		if (ImGui::SliderFloat("Scale", &scale, 0, 5)) {
			mesh.mesh.setScaling(scale);
		}

		const auto& m = mesh.mesh;
		if (ImGui::TreeNode("mesh infos")) {
			std::stringstream h;
			h << "num vertices : " << m.getVertices().size() << "\n";
			h << "num triangles : " << m.getTriangles().size() << "\n";
			ImGui::Text(h);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("vertex data attributes")) {
			std::map<int, std::pair<std::string, VertexAttribute> > attributes_sorted_by_location;
			for (const auto& attribute : m.getAttributes()) {
				attributes_sorted_by_location[attribute.second.index] = { attribute.first, attribute.second };
			}
			for (const auto& attribute : attributes_sorted_by_location) {
				std::stringstream s;
				const auto& att = attribute.second.second;
				s << attribute.second.first << "\n";
				s << "  location : " << att.index << "\n";
				s << "  nchannels : " << att.num_channels << "\n";
				s << "  type : " << att.type << "\n";
				s << "  stride : " << att.stride << "\n";
				s << "  normalized : " << std::boolalpha << (bool)att.normalized << "\n";
				s << "  data ptr : " << att.pointer << "\n";
				ImGui::Text(s);
			}
			ImGui::TreePop();
		}
	},
		[&](const Input& i)
	{
		tb.update(i);
		RaycastingCameraf eye(tb.getCamera(), i.viewport().diagonal().template cast<int>());
		hit = tb.getRaycaster().intersect(eye.getRay(i.mousePosition<float>()));

		if (hit.successful()) {

			int id = (int)std::round(tb.getRaycaster().interpolate(hit, &Mesh::getAttribute<int>, "id"));

			ImGui::BeginTooltip();
			ImGui::Text("Click to " + std::string(id == selectedMesh ? "un" : "") + "select : " + std::to_string(hit.instanceId()));
			ImGui::EndTooltip();

			if (i.buttonClicked(GLFW_MOUSE_BUTTON_LEFT)) {
				selectedMesh = (selectedMesh == id ? -1 : id);
			}
		}
	},
		[&](Framebuffer& dst)
	{
		dst.bindDraw();

		Cameraf eye = tb.getCamera();

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
				shaders.renderTexturedMesh(eye, mesh, currentTex());
				break;
			}
			case Mode::COLORED: {
				mesh.mode = GL_FILL;
				shaders.renderBasicMesh(eye, mesh, m.second.color);
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

			if (m.second.showNormals) {
				shaders.renderNormals(eye, mesh, mesh.getBoundingBox().diagonal().norm() / 25);
			}
			if (m.first == selectedMesh) {
				shaders.renderBasicMesh(eye, MeshGL::getCubeLines(mesh.getBoundingBox()), v4f(0, 1, 0, 1));
			} else if (showAllBB) {
				shaders.renderBasicMesh(eye, MeshGL::getCubeLines(mesh.getBoundingBox()), v4f(1, 0, 0, 1));
			}
		}
		glDisable(GL_BLEND);
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

	using Ray = RayT<float>;
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

	static int numBounces = 2, maxNumSamples = 8, samplesPerPixel = 1, currentNumSamples = 0, maxNumThreads = 4;
	static bool resetRayCasting = true, useMT = false;

	static const int w = 360, h = 240;
	
	SubWindow sub = SubWindow("Ray tracing", v2i(600, 600));

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
		resetRayCasting |= ImGui::SliderInt("max samples per pixel", &maxNumSamples, 1, 128);

		resetRayCasting |= ImGui::Checkbox(
			("use multi-threading, " + std::to_string(std::thread::hardware_concurrency()) + " available cores").c_str(),
			&useMT
		);
		ImGui::SameLine();		resetRayCasting |= ImGui::SliderInt("max num threads", &maxNumThreads, 1, 16);
		ImGui::PopItemWidth();

		std::stringstream s;
		s << "current num samples per pixel : " << currentNumSamples << " / " << maxNumSamples;
		ImGui::Text(s);
	});

	static v2i clicked;
	static bool gatherPaths = false, showPaths = false;
	static std::vector<v3f> paths, colors, normals;
	static ShaderCollection shaders;

	sub.setUpdateFunction([&](const Input& i) {
		tb.update(i);

		if (i.keyActive(GLFW_KEY_LEFT_ALT) && i.buttonClicked(GLFW_MOUSE_BUTTON_LEFT)) {
			if (!gatherPaths) {
				v2d uvs = i.mousePosition().cwiseQuotient(i.viewport().diagonal());
				uvs.y() = 1.0 - uvs.y();
				clicked = uvs.cwiseProduct(v2d(w, h)).template cast<int>();
				paths.resize(0);
				normals.resize(0); 
				colors.resize(0);
				gatherPaths = true;
				showPaths = true;
				resetRayCasting = true;
			} else {
				showPaths = false;
				gatherPaths = false;
			}
		}
	});

	sub.setRenderingFunction([&](Framebuffer& dst) {
		hits.resize(w, h);
		currentSamplesAverage.resize(w, h);

		RaycastingCameraf cam = RaycastingCameraf(tb.getCamera(), w, h);
		const bool sameCam = (previousCam == cam);
		resetRayCasting |= (!sameCam);
		gatherPaths &= sameCam;

		if (gatherPaths) {
			ImGui::BeginTooltip();
			ImGui::Text("collecting paths");
			ImGui::EndTooltip();
		}

		if (resetRayCasting) {
			currentNumSamples = 0;
			currentSamplesAverage.setTo(v3f(0, 0, 0));
			hits.setTo(Vec<uchar, 1>(1));
			resetRayCasting = false;
		} 

		auto rowJob = [&](int i) {
			for (int j = 0; j < w; ++j) {
				int numSamples = currentNumSamples;
				
				for (int s = 0; s < samplesPerPixel; ++s) {
					Ray ray = cam.getRay(v2f(j, h - 1 - i) + 0.5 * (randomVec<float,2>() + v2f(1, 1)));

					bool continueRT = true;
					v3f sampleColor = v3f::Zero();
					v3f color = v3f::Ones();

					for (int b = 0; (b < numBounces) && continueRT; ++b) {

						const Hit hit = raycaster.intersect(ray, 0.001f);
						const bool successful = hit.successful();

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
							//raycaster.visible(p, lightPosition, dir, distToLight)
							float distToLight = (lightPosition - p).norm();
							v3f dir = (lightPosition - p) / distToLight;
							if (!raycaster.occlusion(Ray(p, dir), 0.001f*distToLight, 0.999f*distToLight)) {
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

								if (gatherPaths && j == clicked[0] && i == clicked[1]) {
									paths.push_back(ray.origin());
									paths.push_back(p);
									normals.push_back(p);
									normals.push_back(p + 0.1 * n);
									colors.push_back(sampleColor);
									colors.push_back(sampleColor);
								}
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

	
		if (currentNumSamples < maxNumSamples) {
			raycaster.checkScene();
			if (useMT) {			
				parallelForEach(0, h, [&](int i) {
					rowJob(i);
				}, maxNumThreads);
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
		} else {
			gatherPaths = false;
		}

		dst.blitFrom(tex);

		dst.bindDraw();
		if (showPaths) {		
			auto meshPaths = MeshGL::fromEndPoints(paths);
			meshPaths.setColors(colors);
			shaders.renderColoredMesh(cam, meshPaths);
			shaders.renderBasicMesh(cam, MeshGL::fromEndPoints(normals), v4f(0, 1, 0, 1));
		}
	});

	return sub;
}

SubWindow raymarching_win()
{
	static Trackballf tb = Trackballf::fromMesh(MeshGL::getCube());

	SubWindow win = SubWindow("raymarching", v2i(400, 400));

	win.setUpdateFunction([&](const Input& i) {
		tb.update(i);
	});


	const std::string vert_str = R"(
		#version 420
		layout(location = 0) in vec3 position;
		uniform mat4 mvp;
		out vec3 world_position;
		void main() {
			world_position = position;
			gl_Position = mvp * vec4(position, 1.0);
		}
	)";

	const std::string frag_str = R"(
		#version 420
		layout(location = 0) out vec4 outColor;

		in vec3 world_position;
		uniform vec3 eye_pos;
		uniform vec3 bmin = vec3(-1,-1,-1);
		uniform vec3 bmax = vec3(1,1,1);
		uniform int gridSize = 10;

		int getMinIndex(vec3 v) {
			return v.x < v.y ? (v.x < v.z ? 0 : 2 ) : (v.y < v.z ? 1 : 2);
		}

		float maxCoef(vec3 v){
			return max(v[0],max(v[1],v[2]));
		}

		float minCoef(vec3 v){
			return min(v[0],min(v[1],v[2]));
		}

		ivec3 getCell(vec3 p){
			vec3 uv = (p - bmin)/(bmax - bmin);
			return ivec3(round(gridSize * uv));
		}

		bool unitBoxIntersection(vec3 dir, out vec3 pt) {
			vec3 minTs = (bmin - eye_pos)/dir;
			vec3 maxTs = (bmax - eye_pos)/dir;
			float nearT = maxCoef(min(minTs,maxTs)); 
			float farT = minCoef(max(minTs,maxTs)); 
			if( 0 <= nearT && nearT <= farT){
				pt = eye_pos + nearT*dir;
				return true;
			}
			return false;
		}

		void main(){
	
			outColor = vec4(0,0,0,1);

			vec3 dir = normalize(world_position - eye_pos);
			
			float alpha = 0;

			//setup start
			vec3 start = eye_pos;
			if(!(all(greaterThan(eye_pos, bmin)) && all(greaterThan(bmax, eye_pos)))){
				vec3 intersection;
				if(unitBoxIntersection(dir, intersection)){
					start = intersection;
				} else {
					return;
				}
			} 

			vec3 cellSize = (bmax - bmin)/gridSize; 
			start = clamp(start, bmin + 0.01*cellSize, bmax - 0.01*cellSize);

			//outColor.xyz = 0.5*start+0.5;
			//return; 

			//setup raymarching
			
			vec3 deltas = cellSize / abs(dir);
			vec3 fracs = fract((start - bmin)/cellSize);
			vec3 ts;
			ivec3 finalCell, steps, currentCell = getCell(start);
			for(int c=0; c<3; ++c){
				steps[c] = (dir[c] >= 0 ? 1 : -1);
				ts[c] = deltas[c] * (dir[c] >= 0 ? 1.0 - fracs[c] : fracs[c]);
				finalCell[c] = (dir[c] >= 0 ? gridSize : - 1);
			}

			//actual raymarching
			while(true){

				//do stuff with currentCell
				alpha += 0.5/float(gridSize);

				int c = getMinIndex(ts);

				currentCell[c] += steps[c];
				if(currentCell[c] == finalCell[c]){
					break;
				}	
				ts[c] += deltas[c];
			}

			outColor.xyz = alpha*vec3(1,0,0)+(1.0-alpha)*vec3(0,0,1);
		}
	)";

	static ShaderCollection shaders;
	static ShaderProgram shader;
	static Uniform<v3f> eye_pos = { "eye_pos" };
	shader.init(vert_str, frag_str);
	shader.addUniforms(shaders.mvp, eye_pos);

	win.setRenderingFunction([&](Framebuffer& dst) {
		dst.bindDraw();

		const RaycastingCameraf eye = RaycastingCameraf(tb.getCamera(), v2i(dst.w(), dst.h()));
		eye_pos = eye.position();
		shaders.mvp = eye.viewProj();
		shader.use();
		eye.getQuad(0.5f * (eye.zNear() + eye.zFar())).draw();
	});

	return win;
}

int main(int argc, char** argv)
{

	auto win_checkers = checker_subwin();
	auto win_mesh_modes = mesh_modes_subwin();
	auto win_raytracing = rayTracingWin();
	auto win_raymarch = raymarching_win();

	auto demoOptions = WindowComponent("Demo settings", WindowComponent::Type::DEBUG,
		[&](const Window& win) {
		ImGui::Checkbox("texture viewer", &win_checkers.active());
		ImGui::SameLine();
		ImGui::Checkbox("ray marching", &win_raymarch.active());
		ImGui::Checkbox("mesh modes", &win_mesh_modes.active());
		ImGui::SameLine();
		ImGui::Checkbox("ray tracing", &win_raytracing.active());
	});

	mainWin.renderingLoop([&]{
		win_checkers.show(mainWin);
		win_raymarch.show(mainWin);
		win_mesh_modes.show(mainWin);
		win_raytracing.show(mainWin);

		demoOptions.show(mainWin);
	});

}
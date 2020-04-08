#include <gloops/Window.hpp>
#include <gloops/Mesh.hpp>
#include <gloops/Camera.hpp>
#include <gloops/Shader.hpp>
#include <gloops/Texture.hpp>
#include <gloops/Utils.hpp>

#include <map>
#include <limits>

//dont try this at home
using namespace gloops;

static Window mainWin = Window("GLoops demos");

static ShaderCollection shaders;

static TexParams texParams = TexParams().enableMipmap().setMagFilter(GL_NEAREST);
static TexParams cubeParams = TexParams().disableMipmap().setTarget(GL_TEXTURE_CUBE_MAP).setWrapAll(GL_CLAMP_TO_EDGE);

static const std::string gloops_demo_shaders_folder = std::string(GLOOPS_DEMO_RESOURCES_PATH) + "/shaders/";
static const std::string gloops_demo_textures_folder = std::string(GLOOPS_DEMO_RESOURCES_PATH) + "/textures/";
static const std::string gloops_demo_meshes_folder = std::string(GLOOPS_DEMO_RESOURCES_PATH) + "/meshes/";

Texture checkers_tex = Texture(checkersTexture(100, 100, 10), texParams), perlinTex, 
	kittenTex = Texture::fromPath2D(gloops_demo_textures_folder + "kitten.png", texParams),
	skyCube = Texture::fromPathCube(gloops_demo_textures_folder + "sky.png", cubeParams);

static Texture displacement_tex = Texture(0.2f * perlinNoise(512, 512, 16), TexParams::RED32F);

enum class TexMode { CHECKERS, PERLIN, KITTEN, DISP };
struct ModeData {
	Texture tex;
	std::string name;
};

static std::map<TexMode, ModeData> modes = {
	{ TexMode::CHECKERS, { checkers_tex, "Checkers" }},
	{ TexMode::PERLIN, { perlinTex, "Perlin noise" }},
	{ TexMode::KITTEN, { kittenTex, "Kitten png" }},
	{ TexMode::DISP, { displacement_tex, "Displacement" }},
};

static TexMode currentMode = TexMode::KITTEN;

Texture& currentTex() {
	return modes.at(currentMode).tex;
}

SubWindow texture_subwin()
{
	static bool doReadback = false;
	static v2d uv;
	static v4f color; 

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
			colChanged |= ImGui::colPicker("first", colA);
			ImGui::SameLine();
			colChanged |= ImGui::colPicker("second", colB);
			if (colChanged) {
				Image3b img = (perlin * colA + (1 - perlin) * colB).convert<uchar>(255);
				currentTex().update2D(img, texParams);
				colChanged = false;
			}
		}
		default:
			break;
		}

		ImGui::Separator();
		ImGui::Text("Wrapping");
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
		ImGui::SameLine();
		if (ImGui::Button("Recenter")) {
			texCenter = v2f(0, 0);
		}

		ImGui::Text("Mag filtering");
		for (const auto& filter : mag_filters) {
			ImGui::SameLine();
			if (ImGui::RadioButton((filter.second + "##mag").c_str(), texParams.getMagFilter() == filter.first)) {
				texParams.setMagFilter(filter.first);
			}
		}

		ImGui::Text("Min filtering");
		for (const auto& filter : min_filters) {
			ImGui::SameLine();
			if (ImGui::RadioButton((filter.second + "##min").c_str(), texParams.getMinFilter() == filter.first)) {
				texParams.setMinFilter(filter.first);
			}
		}

		static std::array<GLint, 4> mask;
		static bool channels_changed = true; 
		mask = texParams.getSwizzleMask();

		ImGui::Text("Channel swizzling");
		ImGui::ItemWithSize(100, [&] {
			for (int i = 0; i < 3; ++i) {
				const std::string nm = "##swizzling" + std::to_string(i);
				ImGui::SameLine();
				channels_changed |= ImGui::SliderInt(nm.c_str(), &mask[i], GL_RED, GL_BLUE, channels.at(mask[i]).c_str());
			}
			if (channels_changed) {
				texParams.setSwizzleMask(mask);
				channels_changed = false;
			}
		});
	},
		[&](const Input& i)
	{
		doReadback = i.insideViewport();
		uv = i.mousePosition().cwiseQuotient(i.viewport().diagonal());
		currentTex().update(texParams);

		zoomLevel *= std::pow(1.1f, -(float)i.scrollY());
		zoomLevel = std::clamp(zoomLevel, 0.6f, 10.0f);

		const v2f mPos = (2 * zoomLevel - 1.0f) * i.mousePosition().cwiseQuotient(i.viewport().diagonal()).template cast<float>();
		if (i.buttonClicked(GLFW_MOUSE_BUTTON_RIGHT) || i.buttonClicked(GLFW_MOUSE_BUTTON_LEFT)) {
			clickedPosition = mPos;
			previousCenter = texCenter;
		}
		if (i.buttonActive(GLFW_MOUSE_BUTTON_RIGHT) || i.buttonActive(GLFW_MOUSE_BUTTON_LEFT)) {
			texCenter = previousCenter + clickedPosition - mPos;
		}
	},
		[&](Framebuffer& dst)
	{
		static MeshGL screenQuad;
		const v2f uvCenter = texCenter;
		const v2f tl = uvCenter + v2f(1.0f - zoomLevel, zoomLevel), br = uvCenter + v2f(zoomLevel, 1.0f - zoomLevel);
		screenQuad = MeshGL::quad({ 0,0,0 }, { 1,1,0 }, { -1,1, 0 }, tl, br);
		screenQuad.backface_culling = false;

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
			zoom.update2D(tmp, DefaultTexParams<Image4b>().disableMipmap().setMagFilter(GL_NEAREST));

			ImGui::BeginTooltip();
			ImGui::Image((ImTextureID)zoom.getId(), { 100, 100 }, { 0,1 }, { 1,0 });
			ImGui::EndTooltip();
		}

	});

	sub.getRenderComponent().backgroundColor = v4f(0.3f, 0.3f, 0.3f, 1.0f);

	return sub;
}

SubWindow mesh_modes_subwin()
{
	static int precision = 25;

	auto apple_banana = Mesh::loadMeshes(gloops_demo_meshes_folder + "AppleBanana.obj").at(0).setScaling(1 / 20.0f).setTranslation(v3f(-10, 5, -5)).extractComponents();
	
	static MeshGL meshA = Mesh::getTorus(3, 1, precision),
		meshB = Mesh::getTorus(3, 1, precision).setTranslation(3 * v3f::UnitY()).setRotation(v3f(0, pi<float>() / 2, 0)),
		meshC = Mesh::getSphere(precision).setScaling(3).setTranslation(-8 * v3f(0, 1, 0)),
		meshD = Mesh::getCube().setScaling(2).setTranslation(v3f(-5, -5, 5)).setRotation(v3f(1, 1, 1)),
		banana = apple_banana.at(0),
		apple = apple_banana.at(1).merge(apple_banana.at(2)).merge(apple_banana.at(3)),
		groundPlane = MeshGL::quad({ -20,0,0 }, { 0,100,0 }, { 0, 0,100 });
	groundPlane.backface_culling = false;

	static Trackballf tb = Trackballf::fromMeshComputingRaycaster(meshA, meshB, meshC, meshD, banana, apple);

	static Shader tcs_displacement = Shader(GL_TESS_CONTROL_SHADER, ShaderCollection::tcsTriInTerface()),
		tev_displacement = Shader(GL_TESS_EVALUATION_SHADER, ShaderCollection::tevTriDisplacement());

	enum class Mode { POINT, LINE, PHONG, UVS, COLORED, TEXTURED };
	struct ModeMesh {
		Mode mode;
		MeshGL mesh;
		v4f color = v4f(1, 0, 0, 1);
		float tesselationLevel = 2.0f;
		bool showGeometricNormals = false, showVertexNormals = false, displacement = false;
	};
	static bool showAllBB = false;
	static float displacement_scaling = 5.0f;

	static std::map<int, ModeMesh> meshes =
	{
		{ 0, { Mode::UVS, meshA } },
		{ 1, { Mode::UVS, meshB } },
		{ 2, { Mode::UVS, meshC } },
		{ 3, { Mode::PHONG, meshD } },
		{ 4, { Mode::PHONG, banana } },
		{ 5, { Mode::PHONG, apple } }
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

	auto sub = SubWindow("Render modes", v2i(400, 400));

	static v4f& bgColor = sub.getRenderComponent().backgroundColor, &clearColor = sub.clearColor();

	sub.setGuiFunction([&] {
		ImGui::Checkbox("Show AABBs", &showAllBB);
		ImGui::SameLine();
		ImGui::colPicker("Background", bgColor);
		ImGui::SameLine();
		ImGui::colPicker("Clear", bgColor);

		ImGui::Separator();

		if (selectedMesh < 0) {
			ImGui::Text("No mesh selected");
			return;
		}

		auto& mesh = meshes.at(selectedMesh);
		auto& meshGL = mesh.mesh;

		ImGui::Text("Normals");
		ImGui::SameLine();
		ImGui::Checkbox("Geometric", &mesh.showGeometricNormals);
		if (!mesh.mesh.getNormals().empty()) {
			ImGui::SameLine();
			ImGui::Checkbox("Vertex", &mesh.showVertexNormals);
		}

		ImGui::Checkbox("Displacement", &mesh.displacement);
		if (mesh.displacement) {
			ImGui::SameLine();
			ImGui::ItemWithSize(75,  [&] { 
				ImGui::SliderFloat("Level", &mesh.tesselationLevel, 1, 10);
				ImGui::SameLine(); 
				ImGui::SliderFloat("Scale##disp", &displacement_scaling, 1.0f, 25.0f);
			});
		}
		if (mesh.mode == Mode::COLORED || mesh.mode == Mode::POINT || mesh.mode == Mode::LINE) {
			ImGui::SameLine();
			ImGui::colPicker("Mesh color", mesh.color);
		}

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

		if (ImGui::TreeNode("Transformation")) {
			static v3f pos, rot;
			pos = meshGL.transform().translation();
			if (ImGui::SliderFloat3("Translation", pos.data(), -10, 10)) {
				meshGL.setTranslation(pos);
			}
			rot = meshGL.transform().eulerAngles();
			if (ImGui::SliderFloat3("Rotation", rot.data(), -1.5f * pi<float>(), 1.5f * pi<float>())) {
				meshGL.setRotation(rot);
			}
			static float scale;
			scale = meshGL.transform().scaling()[0];
			ImGui::ItemWithSize(175, [&] {
				if (ImGui::SliderFloat("Scale", &scale, 0.01f, 10)) {
					meshGL.setScaling(scale);
				}

				if (selectedMesh <= 2) {
					ImGui::SameLine();
					if (ImGui::SliderInt("Precision", &precision, 3, 50)) {
						Mesh novelMesh;
						if (selectedMesh == 2) {
							novelMesh = Mesh::getSphere(precision);
						} else {
							novelMesh = Mesh::getTorus(3, 1, precision);
						}
						meshGL.setVertices(novelMesh.getVertices());
						meshGL.setTriangles(novelMesh.getTriangles());
						meshGL.setUVs(novelMesh.getUVs());
						meshGL.setNormals(novelMesh.getNormals());
					}
				}
			});
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("mesh infos")) {
			std::stringstream vinfo, tinfo;
			vinfo << "num vertices : " << meshGL.getVertices().size();
			if (ImGui::TreeNode(vinfo.str().c_str())) {
				if (ImGui::TreeNode("positions")) {
					for (size_t id = 0; id < meshGL.getVertices().size(); ++id) {
						ImGui::Text(std::to_string(id) + " : " + str(meshGL.getVertices()[id]));
					}
					ImGui::TreePop();
				}
				if (!mesh.mesh.getNormals().empty() && ImGui::TreeNode("normals")) {
					for (size_t id = 0; id < meshGL.getNormals().size(); ++id) {
						ImGui::Text(std::to_string(id) + " : " + str(meshGL.getNormals()[id]));
					}
					ImGui::TreePop();
				}
				if (!mesh.mesh.getUVs().empty() && ImGui::TreeNode("tex coords")) {
					for (size_t id = 0; id < meshGL.getUVs().size(); ++id) {
						ImGui::Text(std::to_string(id) + " : " + str(meshGL.getUVs()[id]));
					}
					ImGui::TreePop();
				}
				if (!mesh.mesh.getColors().empty() && ImGui::TreeNode("colors")) {
					for (size_t id = 0; id < meshGL.getColors().size(); ++id) {
						ImGui::Text(std::to_string(id) + " : " + str(meshGL.getColors()[id]));
					}
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}

			tinfo << "num triangles : " << meshGL.getTriangles().size() << "\n";
			if (ImGui::TreeNode(tinfo.str().c_str())) {
				for (const auto& t : meshGL.getTriangles()) {
					ImGui::Text(str(t));
				}
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("gpu vertex attributes")) {
			std::map<int, std::pair<std::string, VertexAttribute> > attributes_sorted_by_location;
			for (const auto& attribute : meshGL.getAttributes()) {
				attributes_sorted_by_location[attribute.second.index] = { attribute.first, attribute.second };
			}
			for (const auto& attribute : attributes_sorted_by_location) {
				const auto& att = attribute.second.second;
				if (ImGui::TreeNode((attribute.second.first + "##gpu").c_str())) {
					std::stringstream s;
					s << "location : " << att.index << "\n";
					s << "nchannels : " << att.num_channels << "\n";
					s << "sizeof attribute : " << 8 * att.total_num_bytes / meshGL.getVertices().size() << "\n";
					s << "stride : " << att.stride << "\n";
					s << "normalized : " << std::boolalpha << (bool)att.normalized << "\n";
					s << "data ptr : " << att.pointer << "\n";
					ImGui::TreePop();
					ImGui::Text(s);
				}
			}
			ImGui::TreePop();
		}
	});
	
	sub.setUpdateFunction([&](const Input& i) {
		tb.update(i);
		RaycastingCameraf eye(tb.getCamera(), i.viewport().diagonal().template cast<int>());
		if (eye.w() == 0 || eye.h() == 0) {
			return;
		}
		hit = tb.getRaycaster().intersect(eye.getRay(i.mousePosition<float>()));

		if (hit.successful()) {

			int id = (int)std::round(tb.getRaycaster().interpolate(hit, &Mesh::getAttribute<int>, "id"));

			ImGui::BeginTooltip();
			ImGui::Text("Left click to " + std::string(id == selectedMesh ? "un" : "") + "select mesh with id : " + std::to_string(hit.instanceId()));
			ImGui::EndTooltip();

			if (i.buttonClicked(GLFW_MOUSE_BUTTON_LEFT)) {
				selectedMesh = (selectedMesh == id ? -1 : id);
			}
		}
	});

	sub.setRenderingFunction( [&](Framebuffer& dst){
		dst.bindDraw();

		Cameraf eye = tb.getCamera();

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		for (auto& m : meshes) {

			MeshGL& mesh = m.second.mesh;

			if (m.second.displacement) {
				mesh.primitive = GL_PATCHES;
				displacement_tex.bindSlot(GL_TEXTURE6);
				shaders.tesselation_size = m.second.tesselationLevel;
				shaders.displacement_scaling = displacement_scaling / mesh.transform().scaling().norm();
				for (auto& program : shaders.shaderPrograms) {
					program.second.setupShader(tcs_displacement);
					program.second.setupShader(tev_displacement);
					program.second.addUniforms(shaders.tesselation_size, shaders.displacement_scaling);
				}	
			}

			switch (m.second.mode)
			{
			case Mode::PHONG: {
				mesh.mode = GL_FILL;
				shaders.renderPhongMesh(eye, mesh);
				break;
			}
			case Mode::UVS: {
				mesh.mode = GL_FILL;
				shaders.renderUVs(eye, mesh);
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

			if (m.second.showGeometricNormals) {
				shaders.renderGeometricNormals(eye, mesh, mesh.getBoundingBox().diagonal().norm() / 25, v4f(1, 0, 1, 1));
			}

			if (m.second.showVertexNormals) {
				shaders.renderVerticeNormals(eye, mesh, mesh.getBoundingBox().diagonal().norm() / 25, v4f(1, 1, 0, 1));
			}

			if (m.second.displacement) {
				mesh.primitive = GL_TRIANGLES;

				for (auto& program : shaders.shaderPrograms) {
					program.second.setupShader(ShaderType::TESSELATION_CONTROL, "");
					program.second.setupShader(ShaderType::TESSELATION_EVALUATION, "");
					program.second.removeUniforms(shaders.tesselation_size, shaders.displacement_scaling);
				}
			}
			
			if (m.first == selectedMesh) {
				shaders.renderBasicMesh(eye, MeshGL::getCubeLines(mesh.getBoundingBox()), v3f(0, 1, 0));
			} else if (showAllBB) {
				shaders.renderBasicMesh(eye, MeshGL::getCubeLines(mesh.getBoundingBox()), v3f(1, 0, 0));
			}


		}

		shaders.renderTexturedMesh(eye, groundPlane, checkers_tex, 0.1f);
		glDisable(GL_BLEND);
	});

	sub.clearColor() = v4f(0.8f, 0.8f, 0.8f, 1.0f);

	return sub;
}

SubWindow rayTracingWin()
{
	//setup Cornell Box

	static MeshGL outerBox = Mesh::getCube().invertFaces(),
		innerBoxA = Mesh::getCube().setScaling(0.4f).setTranslation(v3f(-0.4f, -0.3f, -0.1f)),
		innerBoxB = Mesh::getCube().setScaling(0.3f).setTranslation(v3f(0.4f, -0.5f, 0.1f));

	std::vector<v3f> colorsOut(outerBox.getVertices().size(), v3f::Ones());
	for (size_t i = 0; i < 4; ++i) {
		colorsOut[8 + i] = v3f(1, 0, 0);
		colorsOut[12 + i] = v3f(0, 1, 0);
	}

	outerBox.setColors(colorsOut);
	innerBoxA.setColors(std::vector<v3f>(innerBoxA.getVertices().size(), { 0,0,1 }));
	innerBoxB.setColors(std::vector<v3f>(innerBoxB.getVertices().size(), { 1,0,1 }));

	static const v3f lightPosition = 0.9 * v3f::UnitY(), lightColor = v3f::Ones();
	static const float lightSize = 0.4f;
	static auto surfaceLightPosition = []() -> v3f {
		const v2f uvs = randomVec<float, 2>();
		return lightPosition + lightSize * (v3f::UnitX() * uvs[0] + v3f::UnitZ() * uvs[1]);
	};;
		
	//setup raycaster, camera and buffers

	using Ray = RayT<float>;
	static Raycaster raycaster;
	raycaster.addMesh(outerBox, innerBoxA, innerBoxB);

	static Image1b hits;
	static auto hitMask = [&](int x, int y) { return (bool)hits.at(x, y); };

	static Trackballf tb = Trackballf::fromMeshComputingRaycaster(outerBox).setLookAt(v3f(0.8f, 0.5f, 2.3f), v3f::Zero());
	static RaycastingCameraf currentCam, previousCam;

	static Image3f currentSamplesAverage;
	static Texture tex;

	enum class Mode { COLOR, NORMAL, POSITION, DEPTH };
	static const std::map<Mode, std::string> modes = {
		{ Mode::DEPTH, "Depth" },
		{ Mode::POSITION, "Position" },
		{ Mode::NORMAL, "Normal" },
		{ Mode::COLOR, "Color" }
	};
	static Mode mode = Mode::COLOR;

	static int numBounces = 2, maxNumSamples = 8, samplesPerPixel = 1, currentNumSamples = 0, maxNumThreads = 4;
	static bool resetRayCasting = true, useMT = false;

	static const int w = 128, h = 128;
	
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
		ImGui::ItemWithSize(150, [] {
			resetRayCasting |= ImGui::SliderInt("num bounces", &numBounces, 1, 3);
			resetRayCasting |= ImGui::SliderInt("max samples per pixel", &maxNumSamples, 1, 256);

			resetRayCasting |= ImGui::Checkbox(
				("use multi-threading, " + std::to_string(std::thread::hardware_concurrency()) + " available cores").c_str(),
				&useMT
			);
			if (useMT) {
				ImGui::SameLine();
				resetRayCasting |= ImGui::SliderInt("used threads", &maxNumThreads, 1, 16);
			} else {
				maxNumThreads = 1;
			}
		});

		std::stringstream s;
		s << "current num samples per pixel : " << currentNumSamples << " / " << maxNumSamples;
		ImGui::Text(s);
	});

	static v2i clicked;
	static bool gatherPaths = false, showPaths = false;
	static std::vector<v3f> paths, colors, normals;

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

		hits.resize(w, h);
		currentSamplesAverage.resize(w, h);

		currentCam = RaycastingCameraf(tb.getCamera(), w, h);
		const bool sameCam = (previousCam == currentCam);
		resetRayCasting |= (!sameCam);
		gatherPaths &= sameCam;

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
					Ray ray = currentCam.getRay(v2f(j, h - 1 - i) + 0.5 * (randomVec<float, 2>() + v2f(1, 1)));

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
						const v3f p = ray.pointAt(d);
						const v3f n = raycaster.interpolate(hit, &Mesh::getNormals).normalized();
						const v3f col = raycaster.interpolate(hit, &Mesh::getColors);

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

							if ((p - lightPosition).cwiseAbs().maxCoeff() < lightSize) {
								sampleColor = lightColor;
								continueRT = false; continue;
							}

							color = 0.9 * color.cwiseProduct(col);

							const v3f randomLightPos = surfaceLightPosition();
							float distToLight = (randomLightPos - p).norm();

							v3f dir = (randomLightPos - p) / distToLight;
							if (!raycaster.occlusion(Ray(p, dir), 0.001f * distToLight, 0.999f * distToLight)) {
								const float diffuse = std::max(dir.dot(n), 0.0f);
								const float attenuation = std::clamp<float>(1.0f - (distToLight * distToLight) / (2.5f * 2.5f), 0, 1);

								v3f illumination = (diffuse * attenuation) * lightColor;

								//sampleColor += color.cwiseProduct(illumination);

								sampleColor += color.cwiseProduct(lightColor) * diffuse;

								if (gatherPaths && j == clicked[0] && i == clicked[1]) {
									paths.push_back(ray.origin());
									paths.push_back(p);
									normals.push_back(p);
									normals.push_back(p + 0.1 * n);
									colors.push_back(color.cwiseProduct(lightColor) * diffuse);
									colors.push_back(color.cwiseProduct(lightColor) * diffuse);
								}
							}
						}
						}

						if (b < (numBounces - 1) && continueRT) {
							ray = Ray(p, (n + randomUnit<float, 3>()).normalized());
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
			previousCam = currentCam;

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
			default:
				img = currentSamplesAverage.convert<uchar>(128, 128, hitMask, 255);
			}

			tex.update2D(img);
		} else {
			gatherPaths = false;
		}

	});

	sub.setRenderingFunction([&](Framebuffer& dst) {
		if (gatherPaths) {
			ImGui::BeginTooltip();
			ImGui::Text("Collecting paths");
			ImGui::EndTooltip();
		}

		dst.blitFrom(tex);
		if (showPaths) {		
			auto meshPaths = MeshGL::fromEndPoints(paths);
			meshPaths.setColors(colors);

			dst.bindDraw();
			shaders.renderColoredMesh(currentCam, meshPaths);
			shaders.renderBasicMesh(currentCam, MeshGL::fromEndPoints(normals), v3f(0, 1, 0));
		}
	});

	sub.setFlags(WinFlags::UpdateWhenNotInFocus);

	return sub;
}

SubWindow raymarching_win()
{
	enum class Mode { GRID, SLICE, ISOSURFACE };
	static const std::map<Mode, std::string> modes = {
		{ Mode::GRID, "Grid" },
		{ Mode::SLICE, "Slice" },
		{ Mode::ISOSURFACE, "Iso surface" },
	};
	static Mode mode = Mode::GRID;

	SubWindow win = SubWindow("Ray Marching", v2i(400, 400));

	static Trackballf tb = Trackballf::fromMesh(MeshGL::getCube().setScaling(0.3f));
	tb.setFar(200);

	win.setUpdateFunction([&](const Input& i) {
		tb.update(i);
	});

	static MeshGL unitCube = gloops::Mesh::getCube();
	unitCube.backface_culling = false;

	static ShaderProgram shaderRaymarching, shaderSlice, shaderSDF;
	static Uniform<v3f> eye_pos = { "eye_pos" }, bmax = { "bmax", 0.5*v3f(1,1,1) }, bmin = { "bmin", 0.5*v3f(-1,-1,-1) };
	static Uniform<v3i> gridSize = { "gridSize", 256 * v3i(1,1,1) };
	static Uniform<float> intensity = { "intensity" , 3.0f }, sdf_offset = { "sdf_offset", 0.5f };
	
	shaderRaymarching.init(
		gloops::ShaderCollection::vertexMeshInterface(), 
		gloops::loadFile(gloops_demo_shaders_folder + "voxel_grid_raymarching.frag")
	);
	shaderRaymarching.addUniforms(shaders.vp, shaders.model, eye_pos, bmin, bmax, gridSize, intensity);

	shaderSlice.init(
		gloops::ShaderCollection::vertexMeshInterface(),
		gloops::loadFile(gloops_demo_shaders_folder +  "texture3D_slice.frag")
	);
	shaderSlice.addUniforms(shaders.vp, shaders.model, bmin, bmax);

	shaderSDF.init(
		gloops::ShaderCollection::vertexMeshInterface(),
		gloops::loadFile(gloops_demo_shaders_folder + "texture3D_sdf.frag")
	);
	shaderSDF.addUniforms(shaders.vp, shaders.model, eye_pos, bmin, bmax, gridSize, sdf_offset);

	const int a = 64;
	const int w = a, h = a, d = a;
	TexParams params;
	params.setTarget(GL_TEXTURE_3D).setFormat(GL_RED).setInternalFormat(GL_R8).setWrapAll(GL_CLAMP_TO_BORDER);
	
	static Texture density = Texture(w, h, d, 1, params);
	std::vector<uchar> voxelData(w * h * d, 0);
	for (int i = 0; i < d; ++i) {
		for (int j = 0; j < h; ++j) {
			for (int k = 0; k < w; ++k) {
				float x = 1.0f + 0.75f * (randomVec<float, 1>().x());
				float di = i - d / 2.0f, dj = j - h / 2.0f, dk = k - w / 2.0f;
				float diff = exp(-(di * di + dj * dj + dk * dk)/(2*a));
				voxelData[k + w * (j + h * i)] = saturate_cast<uchar>(255.0f * diff * x);
			}
		}
	}
	density.updloadToGPU3D(0, 0, 0, 0, w, h, d, voxelData.data());

	static bool slice = false;
	static float slice_range = 0;

	win.setGuiFunction([&] {
		int mode_id = 0;
		for (const auto& imode : modes) {
			if (ImGui::RadioButton((imode.second).c_str(), mode == imode.first)) {
				mode = imode.first;
			}
			if (mode_id != ((int)modes.size() - 1)) {
				ImGui::SameLine();
			}
			++mode_id;
		}

		if (ImGui::SliderInt("grid size", &gridSize.get()[0], 1, 512)) {
			gridSize = gridSize.get()[0] * v3i(1, 1, 1);
		}

		switch (mode)
		{
		case Mode::SLICE:
			ImGui::SliderFloat("slice range", &slice_range, -.5f, .5f);  break;
		case Mode::ISOSURFACE:
			ImGui::SliderFloat("isosurface value", &sdf_offset.get(), 0.01f, 0.99f);
		default:
			ImGui::SliderFloat("intensity", &intensity.get(), 2, 4);
		}
	});

	win.setRenderingFunction([&](Framebuffer& dst) {
		const RaycastingCameraf eye = RaycastingCameraf(tb.getCamera(), v2i(dst.w(), dst.h()));
			
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquation(GL_FUNC_ADD);

		dst.clear();
		dst.bindDraw();

		shaders.renderCubemap(eye, v3f(0, 0, 0), 100, skyCube);

		eye_pos = eye.position();
		shaders.vp = eye.viewProj();
		shaders.model = m4f::Identity();

		density.bindSlot(GL_TEXTURE0);

		switch (mode)
		{
		case Mode::SLICE: {
			shaderSlice.use();
			eye.getQuadFront(eye_pos.get().norm() + slice_range).draw();
			break;
		}
		case Mode::ISOSURFACE: {
			shaderSDF.use();
			unitCube.draw();
			break;
		}
		default:
			shaderRaymarching.use();
			unitCube.draw();
		}
	});

	return win;
}

int main(int argc, char** argv)
{
	SubWindow win_textures = texture_subwin();
	SubWindow win_mesh_modes = mesh_modes_subwin();
	SubWindow win_raytracing = rayTracingWin();
	SubWindow win_raymarch = raymarching_win();

	auto demoOptions = WindowComponent("Demo settings", WindowComponent::Type::FLOATING,
		[&](const Window& win) {
		std::stringstream s;
		s << std::round(ImGui::GetIO().Framerate) << " fps";
		ImGui::Text(s);
		ImGui::Checkbox("Texture viewer", &win_textures.active());
		ImGui::SameLine();
		ImGui::Checkbox("Ray marching", &win_raymarch.active());
		ImGui::Checkbox("Mesh modes", &win_mesh_modes.active());
		ImGui::SameLine();
		ImGui::Checkbox("Ray tracing", &win_raytracing.active());
	});

	mainWin.renderingLoop([&]{
		win_textures.show(mainWin);
		win_raymarch.show(mainWin);
		win_mesh_modes.show(mainWin);
		win_raytracing.show(mainWin);

		demoOptions.show(mainWin);
	});
}
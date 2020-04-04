#include "Raycasting.hpp"

#include <embree3/rtcore_geometry.h>

namespace gloops {

	Hit::Hit(const RTCRayHit& rayHit) {
		const auto& hit = rayHit.hit;

		geomId = hit.geomID;
		instId = hit.instID[0];

		if (successful()) {		
			triId = hit.primID;	
			dist = rayHit.ray.tfar;
			normal = v3f(hit.Ng_x, hit.Ng_y, hit.Ng_z).normalized();
			coords = v3f(hit.u, hit.v, std::clamp(1.0f - hit.u - hit.v, 0.0f, 1.0f));
		}
	}

	bool Hit::successful() const
	{
		return geomId != RTC_INVALID_GEOMETRY_ID && instId != RTC_INVALID_GEOMETRY_ID;
	}

	uint Hit::triangleId() const
	{
		return triId;
	}

	uint Hit::geometryId() const
	{
		return geomId;
	}

	uint Hit::instanceId() const
	{
		return instId;
	}

	float Hit::distance() const
	{
		return dist;
	}

	const v3f& Hit::getNormal() const
	{
		return normal;
	}

	const v3f& Hit::getCoords() const
	{
		return coords;
	}

	Raycaster::Raycaster() {

		scene = ScenePtr(rtcNewScene(device()), rtcReleaseScene);
		sceneReady = std::make_shared<bool>(false);

		context = ContextPtr(new RTCIntersectContext());
		rtcInitIntersectContext(context.get());
		//context->flags = RTC_INTERSECT_CONTEXT_FLAG_INCOHERENT;
	}

	Raycaster& Raycaster::operator=(const Raycaster& other)
	{
		scene = other.scene;
		context = other.context;
		meshes = other.meshes;
		sceneReady = other.sceneReady;

		for (const auto& mesh : meshes) {
			setupMeshCallbacks(mesh.second);
		}

		return *this;
	}

	Raycaster::Raycaster(Raycaster&& other)
		: scene(other.scene), context(other.context), meshes(other.meshes), sceneReady(other.sceneReady)
	{
		for (const auto& mesh : meshes) {
			setupMeshCallbacks(mesh.second);
		}
	}

	Raycaster::Raycaster(const Raycaster& other)
		: scene(other.scene), context(other.context), meshes(other.meshes), sceneReady(other.sceneReady)
	{
		for (const auto& mesh : meshes) {
			setupMeshCallbacks(mesh.second);
		}
	}

	void Raycaster::addMeshInternal(const Mesh& mesh)
	{
		ScenePtr localScene = ScenePtr(rtcNewScene(device()), rtcReleaseScene);

		GeometryPtr geometry = GeometryPtr(rtcNewGeometry(
			device(), RTCGeometryType::RTC_GEOMETRY_TYPE_TRIANGLE), rtcReleaseGeometry);

		GeometryPtr instance = GeometryPtr(rtcNewGeometry(
			device(), RTCGeometryType::RTC_GEOMETRY_TYPE_INSTANCE), rtcReleaseGeometry);

		rtcAttachGeometry(localScene.get(), geometry.get());

		rtcSetGeometryInstancedScene(instance.get(), localScene.get());
		rtcSetGeometryTimeStepCount(instance.get(), 1);

		uint instance_id = rtcAttachGeometry(scene.get(), instance.get());

		meshes[instance_id] = { mesh, instance, geometry, localScene };

		setupMeshCallbacks(meshes.at(instance_id));
		
		*sceneReady = false;
	}

	void Raycaster::setupMeshCallbacks(const MeshRaycastingData& data)
	{
		data.mesh.addModelCallback("_gloops_mesh_to_raycaster_model_cb", [&] {
			*data.dirtyModel = true;
			*sceneReady = false;
		});

		data.mesh.addGeometryCallback("_gloops_mesh_to_raycaster_geometry_cb", [&] {
			*data.dirtyModel = true;
			*sceneReady = false;
		});
	}

	void Raycaster::errorCallback(void* userPtr, RTCError code, const char* str)
	{
		static const std::map< RTCError, std::string> errors = {
			{ RTC_ERROR_UNKNOWN , "RTC_ERROR_UNKNOWN : An unknown error has occurred." },
			{ RTC_ERROR_INVALID_ARGUMENT , "RTC_ERROR_INVALID_ARGUMENT : An invalid argument was specified." },
			{ RTC_ERROR_INVALID_OPERATION , "RTC_ERROR_INVALID_OPERATION : The operation is not allowed for the specified object." },
			{ RTC_ERROR_OUT_OF_MEMORY , "RTC_ERROR_OUT_OF_MEMORY : There is not enough memory left to complete the operation." },
			{ RTC_ERROR_UNSUPPORTED_CPU , "RTC_ERROR_UNSUPPORTED_CPU : The CPU is not supported as it does not support the lowest ISA Embree is compiled for." },
			{ RTC_ERROR_CANCELLED , "RTC_ERROR_CANCELLED : The operation got canceled by a memory monitor callback or progress monitor callback function." }
		};

		if (code != RTC_ERROR_NONE) {
			std::cout << "EMBREE ERROR : " << errors.at(code) << std::endl;
			throw std::runtime_error("");
		}

	}

	void Raycaster::checkScene() const
	{
		if (*sceneReady) {
			return;
		}

		for (const auto& mesh : meshes) {
			const auto& m = mesh.second;

			if (*m.dirtyGeometry) {
				using Triangle = Mesh::Tri;
				using Vertice = Mesh::Vert;

				const auto& tris = m.mesh.getTriangles();
				const auto& verts = m.mesh.getVertices();

				Triangle* dst_tris = reinterpret_cast<Triangle*>(rtcSetNewGeometryBuffer(
					m.geometry.get(), RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(Triangle), tris.size()));
				for (size_t i = 0; i < tris.size(); ++i, ++dst_tris) {
					*dst_tris = tris[i];
				}

				Vertice* dst_verts = reinterpret_cast<Vertice*>(rtcSetNewGeometryBuffer(
					m.geometry.get(), RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(Vertice), verts.size()));
				for (size_t i = 0; i < verts.size(); ++i, ++dst_verts) {
					*dst_verts = verts[i];
				}
				rtcCommitGeometry(m.geometry.get());
				rtcCommitScene(m.scene.get());
				*m.dirtyGeometry = false;
			}

			if (*m.dirtyModel) {
				rtcSetGeometryTransform(m.instance.get(), 0, RTCFormat::RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, m.mesh.model().data());
				rtcCommitGeometry(m.instance.get());

				*m.dirtyModel = false;
			}

		}

		rtcCommitScene(scene.get());
		*sceneReady = true;
	}

	RTCDevice Raycaster::device()
	{
		static DevicePtr _device = DevicePtr(rtcNewDevice(0), rtcReleaseDevice);
		static bool first = true;
		if (first) {
			rtcSetDeviceErrorFunction(_device.get(), &errorCallback, nullptr);
			first = false;
		}
		return _device.get();
	}

	void Raycaster::initRayHit(RTCRayHit& out, const Ray& ray, float near, float far) const
	{
		initRay(out.ray, ray, near, far);

		RTCHit& hit = out.hit;
		hit.geomID = RTC_INVALID_GEOMETRY_ID;
	}

	void Raycaster::initRay(RTCRay& out, const Ray& ray, float near, float far) const
	{
		out.tnear = std::max(0.0f, near);
		out.tfar = std::max(near, far);
		out.org_x = ray.origin()[0];
		out.org_y = ray.origin()[1];
		out.org_z = ray.origin()[2];
		out.dir_x = ray.direction()[0];
		out.dir_y = ray.direction()[1];
		out.dir_z = ray.direction()[2];
		out.flags = 0;
	}

	Hit Raycaster::intersect(const Ray& ray, float near, float far) const
	{
		checkScene();

		RTCRayHit rayHit;
		initRayHit(rayHit, ray, near, far);

		rtcIntersect1(scene.get(), context.get(), &rayHit);

		return Hit(rayHit);
	}

	bool Raycaster::occlusion(const Ray& ray, float near, float far) const
	{
		checkScene();

		RTCRay eRay;
		initRay(eRay, ray, near, far);

		rtcOccluded1(scene.get(), context.get(), &eRay);

		return eRay.tfar < 0;
	}

}

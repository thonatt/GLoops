#pragma once

#include "config.hpp"
#include "Mesh.hpp"

#include <array>
#include <memory>
#include <utility>
#include <embree3/rtcore.h>

namespace gloops {

	class Mesh;

	template<uint size>
	struct RayPack;

	template <> struct RayPack<4> {
		using RayHitType = RTCRayHit4;

		static const auto& rtcIntersectFunc() {
			return rtcIntersect4;
		}
	};

	template <> struct RayPack<8> {
		using RayHitType = RTCRayHit8;

		static const auto& rtcIntersectFunc() {
			return rtcIntersect8;
		}
	};

	template <> struct RayPack<16> {
		using RayHitType = RTCRayHit16;

		static const auto& rtcIntersectFunc() {
			return rtcIntersect16;
		}
	};

	template<size_t... Is>
	constexpr std::array<int32_t, sizeof...(Is)> allValidRaysImpl(std::index_sequence<Is...>) {
		return {((void)Is, -1)...};
	}

	template<int N>
	constexpr std::array<int32_t, N> allValidRays() {
		return allValidRaysImpl(std::make_index_sequence<N>{});
	}

	class Hit {

	public:
		Hit() = default;
		Hit(const RTCRayHit& rayHit);
		
		template<uint N>
		static std::array<Hit, N> fromPack(const typename RayPack<N>::RayHitType& rayHits);

		bool successful() const;

		uint triangleId() const;
		uint geometryId() const;
		uint instanceId() const;

		float distance() const;

		const v3f& getNormal() const;
		const v3f& getCoords() const;

	protected:
		
		v3f coords, normal;
		float dist = -1;
		uint geomId = RTC_INVALID_GEOMETRY_ID;
		uint triId = -1;
		uint instId = -1;
	};

	class Raycaster {

		using Ray = RayT<float>;

	public:
		using DevicePtr = std::shared_ptr<RTCDeviceTy>;
		using ScenePtr = std::shared_ptr<RTCSceneTy>;
		using GeometryPtr = std::shared_ptr<RTCGeometryTy>;
		using BufferPtr = std::shared_ptr<RTCBufferTy>;
		using ContextPtr = std::shared_ptr<RTCIntersectContext>;

		struct MeshRaycastingData {
			Mesh mesh;
			GeometryPtr instance;
		};

		Raycaster();
		Raycaster& operator=(const Raycaster& other);

		Hit intersect(const Ray& ray, float near = 0.0f, float far = std::numeric_limits<float>::infinity()) const;

		bool occlusion(const Ray& ray, float near = 0.0f, float far = std::numeric_limits<float>::infinity()) const;

		template<uint N>
		std::array<Hit, N> intersect(
			const std::array<Ray, N>& rays,
			const std::array<int32_t, N>& valids = allValidRays<N>(),
			float near = 0.0f, 
			float far = std::numeric_limits<float>::infinity()
		) const;

		template<typename ...Meshes, typename Mesh> 
		void addMesh(const Mesh& mesh, const Meshes& ...meshes);
	
		template<typename F, typename ... Args>
		auto interpolate(const Hit& hit, F&& meshMember, Args&& ... args) const;

		void checkScene() const;

	private:
		void initRayHit(RTCRayHit& out, const Ray& ray, float near, float far) const;

		void initRay(RTCRay& out, const Ray& ray, float near, float far) const;

		template<uint N>
		void initRayHitPack(typename RayPack<N>::RayHitType& out, const std::array<Ray, N>& rays, float near, float far) const;

		static void errorCallback(void* userPtr, RTCError code, const char* str);

		static DevicePtr device;

		void checkDevice() const;

		void addMeshInternal(const Mesh& mesh);
		void addMesh(){}

		void setupMeshCallbacks(const Mesh& mesh);

		ScenePtr scene;
		ContextPtr context;
		std::map<size_t, MeshRaycastingData> meshes;

		std::shared_ptr<bool> sceneReady;
	};

	template<uint N>
	inline std::array<Hit, N> Hit::fromPack(const typename RayPack<N>::RayHitType& rayHits)
	{
		const auto& hits = rayHits.hit;
		std::array<Hit, N> out;
		for (uint i = 0; i < N; ++i) {
			out[i].geomId = hits.geomID[i];
			if (out[i].successful()) {
				out[i].triId = hits.primID[i];
				out[i].dist = rayHits.ray.tfar[i];
				out[i].normal = v3f(hits.Ng_x[i], hits.Ng_y[i], hits.Ng_z[i]).normalized();
				out[i].coords = v3f(hits.u[i], hits.v[i], std::clamp(1.0f - hits.u[i] - hits.v[i], 0.0f, 1.0f));
			}
		}
		return out;
	}

	template<uint N>
	inline void Raycaster::initRayHitPack(
		typename RayPack<N>::RayHitType& out, const std::array<Ray, N>& rays, float near, float far) const
	{
		auto& eRay = out.ray;
		auto& hit = out.hit;
		for (uint i = 0; i < N; ++i) {
			eRay.tnear[i] = near;
			eRay.tfar[i] = far;
			eRay.org_x[i] = rays[i].origin()[0];
			eRay.org_y[i] = rays[i].origin()[1];
			eRay.org_z[i] = rays[i].origin()[2];
			eRay.dir_x[i] = rays[i].direction()[0];
			eRay.dir_y[i] = rays[i].direction()[1];
			eRay.dir_z[i] = rays[i].direction()[2];
			eRay.flags[i] = 0;

			hit.geomID[i] = RTC_INVALID_GEOMETRY_ID;
		}
	}

	template<uint N>
	inline std::array<Hit, N> Raycaster::intersect(
		const std::array<Ray, N>& rays, const std::array<int32_t, N>& valids, float near, float far) const
	{
		checkScene();

		typename RayPack<N>::RayHitType rayHits;
		initRayHitPack(rayHits, rays, near, far);

		RayPack<N>::rtcIntersectFunc()(valids.data(), scene.get(), context.get(), &rayHits);

		return Hit::fromPack<N>(rayHits);
	}

	template<typename ...Meshes, typename Mesh>
	inline void Raycaster::addMesh(const Mesh& mesh, const Meshes& ...meshes)
	{
		addMeshInternal(mesh);
		addMesh(meshes...);
	}

	template<typename F, typename ... Args>
	inline auto Raycaster::interpolate(const Hit& hit, F&& meshMember, Args&& ... args) const
	{
		const v3f& uvs = hit.getCoords(); 
		const Mesh& mesh = meshes.at(hit.instanceId()).mesh;
		const Mesh::Tri& tri = mesh.getTriangles()[hit.triangleId()];
		const auto& data = (mesh.*meshMember)(std::forward<Args>(args)...);
		return uvs[0] * data[tri[0]] + uvs[1] * data[tri[1]] + uvs[2] * data[tri[1]];
	}
}
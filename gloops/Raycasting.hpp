#pragma once

#include "config.hpp"

#include <array>
#include <memory>
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

	class Hit {

	public:
		Hit() = default;
		Hit(const RTCRayHit& rayHit);
		
		template<uint N>
		static std::array<Hit, N> fromPack(const typename RayPack<N>::RayHitType& rayHits);

		bool successful() const;

		uint triangleId() const;

		float distance() const;

	protected:
		
		v3f coords, normal;
		float dist = -1;
		uint geomId = RTC_INVALID_GEOMETRY_ID;
		uint triId = -1;
	};

	class Raycaster {

		using Ray = Ray<float>;

	public:
		using DevicePtr = std::shared_ptr<RTCDeviceTy>;
		using ScenePtr = std::shared_ptr<RTCSceneTy>;
		using GeometryPtr = std::shared_ptr<RTCGeometryTy>;
		using BufferPtr = std::shared_ptr<RTCBufferTy>;
		using ContextPtr = std::shared_ptr<RTCIntersectContext>;

		Raycaster();

		Hit intersect(const Ray& ray, float near = 0.0f, float far = std::numeric_limits<float>::infinity()) const;

		template<uint N>
		std::array<Hit, N> intersectPack(const std::array<Ray, N>& rays, const std::array<int32_t, N>& valids,
				float near = 0.0f, float far = std::numeric_limits<float>::infinity()) const;

		void addMesh(const Mesh& mesh);

	private:
		static void initRayHit(RTCRayHit& out, const Ray& ray, float near, float far);

		template<uint N>
		static void initRayHitPack(typename RayPack<N>::RayHitType& out, const std::array<Ray, N>& rays, float near, float far);

		static void errorCallback(void* userPtr, RTCError code, const char* str);

		static DevicePtr device;

		void checkDevice() const;

		void checkScene() const;

		ScenePtr scene;
		ContextPtr context;
		mutable bool sceneReady = false;
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
		typename RayPack<N>::RayHitType& out, const std::array<Ray, N>& rays, float near, float far)
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
	inline std::array<Hit, N> Raycaster::intersectPack(
		const std::array<Ray, N>& rays, const std::array<int32_t, N>& valids, float near, float far) const
	{
		checkScene();

		typename RayPack<N>::RayHitType rayHits;
		initRayHitPack(rayHits, rays, near, far);

		RayPack<N>::rtcIntersectFunc()(valids.data(), scene.get(), context.get(), &rayHits);

		return Hit::fromPack<N>(rayHits);
	}

}
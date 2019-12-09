#pragma once

#include "Config.hpp"
#include "Input.hpp"
#include "Mesh.hpp"
#include "Raycasting.hpp"

namespace gloops {

	template<typename T>
	class Camera {

	public:
		using v2 = Eigen::Matrix<T, 2, 1>;
		using v3 = Eigen::Matrix<T, 3, 1>;
		using v4 = Eigen::Matrix<T, 4, 1>;
		using m3 = Eigen::Matrix<T, 3, 3>;
		using m4 = Eigen::Matrix<T, 4, 4>;
		using Line3 = Eigen::ParametrizedLine<T, 3>;
		using Quat = Eigen::Quaternion<T>;

	public:
		
		Camera() : 
			rotation(1,0,0,0), pos(0, 0, 0), _fovy(T(0.5)), _aspect(T(1)), _zNear(T(0.01)), _zFar(T(100)) {}

		void setPerspective(T fovy, T aspect, T near = T(0.01), T far = T(100.0))
		{
			_fovy = fovy;
			_aspect = aspect;
			_zNear = near;
			_zFar = far;
			dirty = true;
		}

		void setLookAt(const v3 & eye, const v3 & target, const v3 & up = { 0,1,0 })
		{
			const v3 z = (eye - target).normalized();
			const v3 x = (up.normalized().cross(z)).normalized();
			const v3 y = z.cross(x).normalized();

			pos = eye;

			m3 rot;
			rot << x, y, z;
			
			rotation = rot;
			dirty = true;
		}

		const v3 & position() const
		{
			return pos;
		}
		
		const m4 & viewProj() const
		{
			update();
			return viewproj;
		}

		const m4 & invViewProj() const
		{
			update();
			return inv_viewproj;
		}

		m4 view() const
		{
			//m3 rot = getRotation().conjugate().toRotationMatrix();
			//return transformation<T>(-rot * position(), rot);

			return transformation<T>(position(), getRotation().toRotationMatrix()).inverse();
		}

		m4 proj() const
		{
			return perspective(_fovy, _aspect, _zNear, _zFar);
		}

		//pt3d to xyd in [-1,1]
		v3 project(const v3 & pt) const
		{
			v4 p = viewProj() * v4(pt[0], pt[1], pt[2], 1.0);
			return v3(p[0], p[1], p[2]) / p[3];
		}

		//xyd in [-1,1] to pt3d
		v3 unproject(const v3 & px) const
		{
			v4 p = invViewProj()* v4(px[0], px[1], px[2], 1.0);
			return v3(p[0], p[1], p[2]) / p[3];
		}

		v3 dir() const
		{
			return rotation.toRotationMatrix()*(-v3::UnitZ());
		}

		v3 right() const
		{
			return rotation.toRotationMatrix()*v3::UnitX();
		}

		v3 up() const
		{
			return rotation.toRotationMatrix()*v3::UnitY();
		}

		Line3 line(const v2 & uv) const 
		{
			return Line3::Through(position(), unproject(v3(uv[0], uv[1], 1)));
		}

		const Quat & getRotation() const
		{
			return rotation;
		}

		T zNear() const
		{
			return _zNear;
		}

		T zFar() const
		{
			return _zFar;
		}

		T aspect() const {
			return _aspect;
		}

		T fovy() const {
			return _fovy;
		}

		static m4 perspective(T fovy, T aspect, T zNear, T zFar)
		{
			T y = T(1) / std::tan(fovy / T(2));
			T x = y / aspect;

			T a = (zNear + zFar) / (zNear - zFar);
			T b = T(2) * zNear * zFar / (zNear - zFar);
			T c = T(-1);

			m4 out;
			out <<
				x, 0, 0, 0,
				0, y, 0, 0,
				0, 0, a, b,
				0, 0, c, 0;

			return out;
		}

		//static m4 perspective(const v2& fovs, T zNear, T zFar)
		//{
		//	T aspect = std::tan(fovs.x() / T(2)) / std::tan(fovs.y() / T(2));
		//	return perspective(fovs.y(), aspect, zNear, zFar);
		//}

		void setFovy(T fov)
		{
			_fovy = fov;
			dirty = true;
		}

		void setNear(T near)
		{
			_zNear = near;
			dirty = true;
		}

		void setFar(T far)
		{
			_zFar = far;
			dirty = true;
		}

		void translate(const v3 & t)
		{
			pos += t;
			dirty = true;
		}

		void setPosition(const v3 & p)
		{
			pos = p;
			dirty = true;
		}

		void setRotation(const Quat & q)
		{
			rotation = q;
			dirty = true;
		}

		void setAspect(T aspect)
		{
			_aspect = aspect;
			dirty = true;
		}

		template<typename U> Camera<U> cast() const
		{
			Camera<U> out;
			out.setAspect(static_cast<U>(_aspect));
			out.setFovy(static_cast<U>(_fovy));
			out.setNear(static_cast<U>(_zNear));
			out.setFar(static_cast<U>(_zFar));
			out.setRotation(rotation.template cast<U>());
			out.setPosition(pos.template cast<U>());
			return out;
		}

		MeshGL getAxisMesh(T scale) const
		{
			static const Mesh::Triangles tris = {
				{ 0,0,1 }, { 0,0,2 }, { 0,0,3 }
			};

			static const Mesh::Colors cols = {
				v3f::Ones(), v3f::UnitX(), v3f::UnitY(), v3f::UnitZ()
			};

			static const Mesh::Vertices verts = {
				v3f::Zero(), v3f::UnitX(), v3f::UnitY(), v3f::UnitZ()
			};

			Mesh::Vertices vs = {
				position(), position() + scale * right(), position() + scale * up(), position() + scale * dir()
			};

			MeshGL out;
			out.setTriangles(tris);
			out.setVertices(vs);
			out.setColors(cols);
			out.mode = GL_LINE;

			return out;
		}

	private:

		void update() const
		{
			if (dirty) {
				viewproj = proj()*view();
				inv_viewproj = viewproj.inverse();
				dirty = false;
			}
		}
		
		Quat rotation;
		v3 pos;
		T _fovy, _aspect, _zNear, _zFar;

		mutable m4 viewproj, inv_viewproj;
		mutable bool dirty = true;
	};

	using Cameraf = Camera<float>;
	using Camerad = Camera<double>;

	template<typename T>
	class RaycastingCamera : public Camera<T> {
		using Cam = Camera<T>;
		using Ray = Ray<T>;
		using v2 = Cam::v2;
		using v3 = Cam::v3;
		using Quat = Cam::Quat;


	public:

		RaycastingCamera() : Cam() {}


		RaycastingCamera(const Cam& cam, int w, int h)
			: Cam(cam), _w(w), _h(h) 
		{
			setupDerivatives();
		}

		RaycastingCamera(const Quat& rotation, const v3& position, T focal_pix, int w, int h, 
			T near = T(0.01), T far = T(100.0)) 
			: _w(w), _h(h)
		{		
			setRotation(rotation);
			setPosition(position);
			T fovy = T(2)* std::atan(h / (T(2)* focal_pix));
			T aspect = T(w) / T(h);
			setPerspective(fovy, aspect, near, far);
			setupDerivatives();
		}

		RaycastingCamera(const Quat& rotation, const v3& position, const v2& focals_pix, int w, int h,
			T near = T(0.01), T far = T(100.0))
			: _w(w), _h(h)
		{
			setRotation(rotation);
			setPosition(position);
			T fovx = T(2) * std::atan(w / (T(2) * focals_pix.x()));
			T fovy = T(2) * std::atan(h / (T(2) * focals_pix.y()));
		
			T aspect_focals = std::tan(fovx / T(2)) / std::tan(fovy / T(2));

			//T aspect = T(w) / T(h);
			//std::cout << "aspect check : " << aspect << " " << aspect_focals << std::endl;
			
			setPerspective(fovy, aspect_focals, near, far);
			setupDerivatives();
		}

		RaycastingCamera(const Cam& cam, const v2i& size)
			: RaycastingCamera(cam, size[0], size[1]) { }


		v3 rayDirNotNormalized(const v2& pix) const {
			return pix[0] * dx + pix[1] * dy + offset;
		}

		v3 rayDir(const v2& pix) const {
			return rayDirNotNormalized(pix).normalized();
		}

		Ray getRay(const v2& pix) const {
			return Ray(position(), rayDir(pix));
		}

		int w() const {
			return _w;
		}

		int h() const {
			return _h;
		}

		//pt3d to xyd in [0,w]x[0,h]x[-1,1]
		v3 projectImgInvY(const v3& pt) const
		{
			v3 p = Camera<T>::project(pt);
			return v3(w() * (p[0] + T(1)) / T(2), h() * (-p[1] + T(1)) / T(2), p[2]);
		}

		MeshGL getQuad(T dist) const
		{
			static const Mesh::Triangles quadTriangles = {
				{ 0,1,2 }, { 0,2,3 }
			};
			static const Mesh::UVs quadUVs = {
				{ 0,0 }, { 0,1 }, { 1,1 }, { 1,0 }
			};

			static std::vector<v3f> dirs(4);

			static Mesh::Vertices vertices(4);
			auto corners_rays = getCornersRays();
			for (int c = 0; c < 4; ++c) {
				vertices[c] = corners_rays[c].pointAt(dist);
			}

			MeshGL out;
			out.setVertices(vertices);
			out.setTriangles(quadTriangles);
			out.setUVs(quadUVs);
			return out;
		}

		MeshGL getCamStub(T near, T far) const
		{
			static const Mesh::Triangles tris = {
				{0,0,1},{1,1,2},{2,2,3},{3,3,0},
				{4,4,5},{5,5,6},{6,6,7},{7,7,4},
				{0,0,4},{1,1,5},{2,2,6},{3,3,7},
			};

			static Mesh::Vertices vertices(8);
			auto corners_rays = getCornersRays();
			for (int k = 0; k < 2; k++) {
				float dist = (k == 0 ? near : far);
				for (int c = 0; c < 4; ++c ) {
					vertices[4 * k + c] = corners_rays[c].pointAt(dist);
				}
			}

			MeshGL out;
			out.setTriangles(tris);
			out.setVertices(vertices);
			out.mode = GL_LINE;
			return out;
		}

	private:

		std::vector<v2> getCorners() const {
			return { v2{ 0, 0 }, v2{ 0, h() }, v2{ w(), h() }, v2{ w(), 0 } };
		}
		
		std::vector<Ray> getCornersRays() const {
			auto corners = getCorners();
			std::vector<Ray> corners_dirs(4);
			for (uint i = 0; i < 4; ++i) {
				corners_dirs[i] = getRay(corners[i]);
			}
			return corners_dirs;
		}

		void setupDerivatives() {
			T h_world = static_cast<T>(2)* std::tan(fovy() / static_cast<T>(2));
			T w_world = h_world * aspect();

			v3 col = w_world * right();
			v3 row = -h_world * up();
			dx = col / static_cast<T>(w());
			dy = row / static_cast<T>(h());
			offset = dir() - (col + row) / T(2);
		}

		v3 dx, dy, offset;
		int _w = 0, _h = 0;
	};

	using RaycastingCameraf = RaycastingCamera<float>;
	using RaycastingCamerad = RaycastingCamera<double>;

	template<typename T>
	class Trackball {

		using Cam = Camera<T>;
		using RCam = RaycastingCamera<T>;
		using v2 = Eigen::Matrix<T, 2, 1>;
		using v3 = Eigen::Matrix<T, 3, 1>;
		using Quat = Eigen::Quaternion<T>;
		using Line3 = Eigen::ParametrizedLine<T, 3>;
		using Plane3 = Eigen::Hyperplane<T, 3>;
		using Rot = Eigen::AngleAxis<T>;

	public:

		enum Status { IDLE, ROTATION, TRANSLATION, ROLL };

		Trackball() = default;

		Trackball(const Cam& cam, T r)
		{
			eye = cam.position();
			center = eye + r * cam.dir();
			up = cam.up();
			_camera = cam;
			dirty = true;
		}

		void setRaycaster(const Raycaster& _raycaster)
		{
			raycaster = _raycaster;
		}

		void setExtrinsics(const Cam& cam) {
			T dist = (eye - center).norm();
			eye = cam.position();
			center = eye + dist * cam.dir();
			up = cam.up();
			dirty = true;
		}

		static Trackball fromMesh(const Mesh& mesh, bool computeRaycaster = false)
		{
			auto box = mesh.getBoundingBox();

			Camera<T> cam;
			v3 eye = box.center() + T(1) * box.diagonal();
			v3 at = box.center();
			T dist = (eye - at).norm();

			cam.setLookAt(eye, at);

			cam.setPerspective(degToRad<T>(T(60)), T(1), dist / T(50), dist * T(10));

			Trackball out(cam, dist);

			if (computeRaycaster) {
				Raycaster rc;
				rc.addMesh(mesh);
				out.setRaycaster(rc);
			}

			return out;
		}

		Cam getCamera() const
		{
			checkCam();
			if (status != IDLE) {
				v3 tmp_eye, tmp_center, tmp_up;
				getTmpTrackBall(tmp_eye, tmp_center, tmp_up);
				Cam cam = _camera;
				cam.setLookAt(tmp_eye, tmp_center, tmp_up);
				return cam;
			} else {
				return _camera;
			}
		}

		void setRadius(T radius)
		{
			eye = center + radius * (eye - center).normalized();
			dirty = true;
		}

		T radius() const
		{
			return (eye - center).norm();
		}

		void setNear(T near)
		{
			_camera.setNear(near);
			dirty = true;
		}

		void setFar(T far)
		{
			_camera.setFar(far);
			dirty = true;
		}

		void setFovy(T fov)
		{
			_camera.setFovy(fov);
			dirty = true;
		}
		
		void setAspect(T aspect)
		{
			_camera.setAspect(aspect);
			dirty = true;
		}

		void update(const Input& i)
		{
			v2d vp_diag = i.viewport().diagonal();
			setAspect(T(vp_diag[0] / vp_diag[1]));

			updateRadius(i);
			updateRotation(i);
			updateTranslation(i);
			updateCenter(i);
			updateNearFar(i);
		}

		void setCenter(const v3& pos)
		{
			center = pos;
			dirty = true;
		}

	protected:

		void checkCam() const
		{
			if (dirty) {
				_camera.setLookAt(eye, center, up);
				dirty = false;
			}
		}

		void updateRadius(const Input& i)
		{
			if (status == IDLE && i.scrollY() != 0 && !i.keyActive(GLFW_KEY_LEFT_CONTROL)) {
				eye = center + pow(1.1, -i.scrollY()) * (eye - center);
				dirty = true;
			}
		}

		void updateRotation(const Input& i)
		{
			if (status == IDLE && i.buttonClicked(GLFW_MOUSE_BUTTON_LEFT) && !i.keyActive(GLFW_KEY_LEFT_CONTROL)) {
				clickedUV = i.viewport().uv(i.mousePosition()).template cast<T>();
				if (i.keyActive(GLFW_KEY_LEFT_SHIFT)) {
					status = ROLL;
				} else {
					status = ROTATION;
				}
			}
			if (status == ROTATION || status == ROLL) {
				currentUV = i.viewport().uv(i.mousePosition()).template cast<T>();

				if (i.buttonUnclicked(GLFW_MOUSE_BUTTON_LEFT)) {
					getTmpTrackBall(eye, center, up);
					status = IDLE;
					dirty = true;
				}
			}
		}

		void updateTranslation(const Input& i)
		{
			if (status == IDLE && i.buttonClicked(GLFW_MOUSE_BUTTON_RIGHT) && !i.keyActive(GLFW_KEY_LEFT_CONTROL)) {
				clickedUV = i.viewport().uv(i.mousePosition()).template cast<T>();
				status = TRANSLATION;
			}
			if (status == TRANSLATION) {
				currentUV = i.viewport().uv(i.mousePosition()).template cast<T>();

				if (i.buttonUnclicked(GLFW_MOUSE_BUTTON_RIGHT)) {
					getTmpTrackBall(eye, center, up);
					status = IDLE;
					dirty = true;
				}
			}
		}

		void updateCenter(const Input& i)
		{
			if (status == IDLE && i.keyActive(GLFW_KEY_LEFT_CONTROL) && i.buttonClicked(GLFW_MOUSE_BUTTON_LEFT)) {

				RaycastingCamera<T> cam(getCamera(), (int)i.viewport().width(), (int)i.viewport().height());
				auto ray = cam.getRay(i.mousePosition<float>());

				Hit hit = raycaster.intersect(ray);
				if (hit.successful()) {
					v3 intersection_pt = ray.pointAt(hit.distance());
					center = intersection_pt;
				} 
			}
		}

		void updateNearFar(const Input& i) {
			if (i.keyActive(GLFW_KEY_LEFT_CONTROL) && i.scrollY() ) {
				T change = pow(T(1.25), T(i.scrollY()));
				if (i.keyActive(GLFW_KEY_LEFT_SHIFT)) {
					setFar(change * _camera.zFar());
				} else {
					setNear(change * _camera.zNear());
				}
			}
		}

		void getTmpTrackBall(v3& out_eye, v3& out_center, v3& out_up) const
		{
			v3 tmp_eye = eye, tmp_center = center, tmp_up = up;
			v2 delta_uv = currentUV - clickedUV;

			if (status == ROTATION || status == ROLL) {
				v2 uv = -delta_uv / T(2);
				v2 rads = uvToRad(uv);
				Quat rot;
				if (status == ROTATION) {
					rot = Rot(rads[0], tmp_up) * Rot(rads[1], _camera.right());
				} else {
					rot = Rot(T(2)*rads[0], _camera.dir());
				}
				out_eye = tmp_center + rot * (_camera.position() - tmp_center);
				out_center = tmp_center;
				out_up = rot * tmp_up;
			} else if (status == TRANSLATION) {
				v2 uv = T(2) * v2(-delta_uv[0], delta_uv[1]);
				Line3 line = _camera.line(uv);
				out_center = line.intersectionPoint(Plane3(_camera.dir(), tmp_center));
				out_eye = tmp_eye + out_center - tmp_center;
				out_up = tmp_up;
			}
		}


		Raycaster raycaster;
		v2 clickedUV, currentUV;
		Status status = IDLE;

		v3 center, eye, up;

		mutable Cam _camera;
		mutable bool dirty = false;
	};

	using Trackballf = Trackball<float>;
	using Trackballd = Trackball<double>;

}
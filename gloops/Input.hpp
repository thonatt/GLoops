#pragma once

#include "config.hpp"
#include <array>
#include <map>

namespace gloops {

	template<typename T>
	class ViewportT : public Eigen::AlignedBox<T, 2> {

	public:
		using Box2D = Eigen::AlignedBox<T, 2>;
		using Box2D::Box2D;
		using Box2D::max;
		using Box2D::min;

		void gl() const {
			glViewport(static_cast<GLint>(left()), static_cast<GLint>(top()),
				static_cast<GLsizei>(width()), static_cast<GLsizei>(height()));
		}

		T width() const {
			return right() - left();
		}

		T height() const {
			return bottom() - top();
		}

		T top() const {
			return min()[1];
		}

		T bottom() const {
			return max()[1];
		}

		T left() const {
			return min()[0];
		}

		T right() const {
			return max()[0];
		}

		template<typename U>
		ViewportT<U> cast() const {
			return ViewportT<U>(min().template cast<U>(), max().template cast<U>());
		}

		bool checkNan() const {
			if (min().hasNaN() || max().hasNaN()) {
				std::cout << "vp is nan " << min().transpose() << " -> " << max().transpose() << std::endl;
				return false;
			}
			return true;
		}
	};

	template<typename T>
	std::ostream& operator<<(std::ostream& s, const ViewportT<T>& vp)
	{
		return s << vp.min().transpose() << " " << vp.max().transpose();
	}

	using Viewportd = ViewportT<double>;
	using Viewportf = ViewportT<float>;
	using Viewporti = ViewportT<int>;

	enum class InputType { KEYBOARD, MOUSE };

	class Input {

		template<typename T>
		using v2 = Eigen::Matrix<T, 2, 1>;
	public:
		using Key = std::pair<InputType, int>;
		using KeyStatus = std::map<Key, int>;

		bool keyActive(int key) const;
		bool keyPressed(int key) const;
		bool keyReleased(int key) const;

		bool buttonActive(int button) const;
		bool buttonClicked(int button) const;
		bool buttonUnclicked(int button) const;

		bool insideViewport() const;

		//returns uv, in [0,1] when inside viewport
		template<typename T = double>
		v2<T> mousePositionUV() const;

		template<typename T = double>
		v2<T> mousePosition() const;

		const double & scrollY() const;

		const Viewportd & viewport() const;
		Viewportd& viewport();

		Input subInput(const Viewportd& vp, bool forceEmpty = false) const;

		void guiInputDebug() const;

	protected:
		
		using KeysStatus = std::array<int, GLFW_KEY_LAST>;
		using MouseStatus = std::array<int, GLFW_MOUSE_BUTTON_LAST>;

		KeysStatus keyStatus = {}, keyStatusPrevious = {};
		MouseStatus mouseStatus = {}, mouseStatusPrevious = {};
		v2d _mousePosition, _mouseScroll;
		Viewportd _viewport;
	};

	template<typename T>
	Input::v2<T> Input::mousePosition() const
	{
		return _mousePosition.template cast<T>();
	}

	template<typename T>
	Input::v2<T> Input::mousePositionUV() const
	{
		return mousePosition().cwiseQuotient(viewport().diagonal()).template cast<T>();
	}
}
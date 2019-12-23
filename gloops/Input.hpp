#pragma once

#include "config.hpp"
#include <array>
#include <map>

namespace gloops {

	class Viewport : public BBox2d {
		
	public:
		using BBox2d::BBox2d;

		void gl() const;

		double width() const;
		double height() const;

		double top() const;
		double bottom() const;
		double left() const;
		double right() const;

		bool checkNan() const;
	};

	std::ostream& operator<<(std::ostream& s, const Viewport& vp);

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

		bool buttonClicked(int button) const;
		bool buttonUnclicked(int button) const;

		bool insideViewport() const;

		//returns uv, in [0,1] when inside viewport
		template<typename T = double>
		v2<T> mousePositionUV() const;

		template<typename T = double>
		v2<T> mousePosition() const;

		const double & scrollY() const;

		const Viewport & viewport() const;
		Viewport& viewport();

		Input subInput(const Viewport& vp, bool forceEmpty = false) const;

		void guiInputDebug() const;

	protected:
		
		using KeysStatus = std::array<int, GLFW_KEY_LAST>;
		using MouseStatus = std::array<int, GLFW_MOUSE_BUTTON_LAST>;

		KeysStatus keyStatus = {}, keyStatusPrevious = {};
		MouseStatus mouseStatus = {}, mouseStatusPrevious = {};
		v2d _mousePosition, _mouseScroll;
		Viewport _viewport;
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
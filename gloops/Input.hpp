#pragma once

#include "config.hpp"
#include <array>
#include <map>

namespace gloops {

	class Viewport : public BBox2d {
		
	public:
		using BBox2d::BBox2d;

		//returns uv in [-1,1] if inside viewport
		v2d normalizedUV(const v2d & pos) const;

		//returns uv in [0,1] if inside viewport
		v2d uv(const v2d & pos) const;

		double width() const;
		double height() const;
	};

	enum class InputType { KEYBOARD, MOUSE };

	class Input {

	public:
		using Key = std::pair<InputType, int>;
		using KeyStatus = std::map<Key, int>;

		bool keyActive(int key) const;
		bool keyPressed(int key) const;
		bool keyReleased(int key) const;

		bool buttonClicked(int button) const;
		bool buttonUnclicked(int button) const;

		bool insideViewport() const;

		template<typename T = double>
		Eigen::Matrix<T, 2, 1> mousePosition() const {
			return _mousePosition.template cast<T>();
		}

		const double & scrollY() const;

		const Viewport & viewport() const;

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

}
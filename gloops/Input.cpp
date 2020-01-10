#include "Input.hpp"

namespace gloops {

	bool Input::keyActive(int key) const
	{
		return keyStatus[key] == GLFW_PRESS || keyStatus[key] == GLFW_REPEAT;
	}

	bool Input::keyPressed(int key) const
	{
		return keyStatusPrevious[key] == GLFW_RELEASE && keyStatus[key] == GLFW_PRESS;
	}

	bool Input::keyReleased(int key) const
	{
		return keyStatus[key] == GLFW_RELEASE && (keyStatusPrevious[key] == GLFW_PRESS || keyStatusPrevious[key] == GLFW_REPEAT);
	}

	bool Input::buttonActive(int button) const
	{
		return mouseStatus[button] == GLFW_PRESS || mouseStatus[button] == GLFW_REPEAT;
	}

	bool Input::buttonClicked(int button) const
	{
		return mouseStatus[button] == GLFW_PRESS && mouseStatusPrevious[button] == GLFW_RELEASE;
	}

	bool Input::buttonUnclicked(int button) const
	{
		return (mouseStatusPrevious[button] == GLFW_PRESS || mouseStatusPrevious[button] == GLFW_REPEAT) && mouseStatus[button] == GLFW_RELEASE;
	}

	bool Input::insideViewport() const
	{
		return (_mousePosition.array() >= 0).all() && (_mousePosition.array() < viewport().diagonal().array()).all();
	}

	const double& Input::scrollY() const
	{
		return _mouseScroll[1];
	}

	const Viewportd& Input::viewport() const
	{
		return _viewport;
	}

	Viewportd& Input::viewport()
	{
		return _viewport;
	}

	Input Input::subInput(const Viewportd& vp, bool forceEmpty) const
	{
		Input sub = *this;
		sub._viewport = vp;
		sub._mousePosition -= vp.corner(BBox2d::BottomLeft);
		if (forceEmpty || !vp.contains(mousePosition())) {
			sub.mouseStatus = {};
			sub._mouseScroll = v2d(0, 0);
			sub.keyStatus = {};
		}
		return sub;
	}

	void Input::guiInputDebug() const
	{	
		if (ImGui::CollapsingHeader("mousep & vp")) {
			std::stringstream sa, sb;
			sa << "vp : " << viewport().min().transpose() << " " << viewport().max().transpose() << "\n";
			ImGui::Text(sa.str());
			sb << "mouse pos " << mousePosition().transpose();
			ImGui::TextColored(sb.str(), viewport().contains(mousePosition() + viewport().min()) ? v4f(1, 1, 1, 1) : v4f(1, 0, 0, 1));		
		}

		if (ImGui::CollapsingHeader("mousek")) {
			std::stringstream ss;
			ss << "scroll : " << _mouseScroll.transpose() << "\n";
			for (uint i = 0; i < mouseStatus.size(); ++i ) {
				ss << std::fixed << i << " : " << mouseStatus[i] << "\n";
			}
			ImGui::Text(ss.str());
		}
		if (ImGui::CollapsingHeader("key")) {
			std::stringstream ss;
			for (uint i = 0; i < keyStatus.size(); ++i) {
				ss << std::fixed << i << " : " << keyStatus[i] << "\n";
			}
			ImGui::Text(ss.str());
		}
	}

	//v2d Viewport::normalizedUV(const v2d& pos) const
	//{
	//	return 2.0 * (pos - center()).cwiseQuotient(diagonal());
	//}

	//v2d Viewport::uv(const v2d& pos) const
	//{
	//	return pos.cwiseQuotient(diagonal());
	//}

}
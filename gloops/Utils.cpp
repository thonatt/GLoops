#include "Utils.hpp"

namespace gloops {
	std::string loadFile(const std::string& path)
	{
		std::ifstream ifs(path);
		std::stringstream sst;
		sst << ifs.rdbuf();
		return sst.str();
	}
	void v3fgui(const v3f& v)
	{
		ImGui::Text(std::to_string(v[0]) + " " + std::to_string(v[1]) + " " + std::to_string(v[2]));
	}
}
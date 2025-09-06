#pragma once

#include <string>

struct Controller
{
	Controller()
	{}
	Controller(std::string name_,std::string ip_,uint64_t sc, uint64_t chan )
		: name(std::move(name_)), ip(std::move(ip_)), start_channel(sc), channels(chan)
	{}
	std::string name;
	std::string ip;
	uint64_t start_channel{0};
	uint64_t channels{0};
};
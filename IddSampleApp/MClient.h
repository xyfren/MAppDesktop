#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

using boost::asio::ip::tcp;

struct MClient
{
	enum class State {Unconnected,Connected,Authorized};
	
	shared_ptr<tcp::socket> socket;
	uint16_t udpPort = 0;
	State state = State::Unconnected;

	MClient(){}
	MClient(shared_ptr<tcp::socket> pSocket) : socket(pSocket), state(State::Connected)
	{ }
};


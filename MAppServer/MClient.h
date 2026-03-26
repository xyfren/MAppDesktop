#pragma once

#pragma warning(push, 0)
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#pragma warning(pop)

struct MClient
{
	enum class State {Unconnected,Connected,Authorized};
	
	std::shared_ptr<boost::asio::ip::tcp::socket> socket;
	boost::asio::ip::udp::endpoint targetEndpoint;
	State state = State::Unconnected;

	MClient() {

	}

	explicit MClient(std::shared_ptr<tcp::socket> pSocket) : socket(pSocket), state(State::Connected)
	{ }
	//bool operator<(const MClient& other) const {
	//	return socket->native_handle() < other.socket->native_handle();

	//}
};


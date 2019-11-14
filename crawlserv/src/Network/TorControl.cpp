/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * TorControl.cpp
 *
 * Connecting to a TOR control server to request a new identity.
 *
 *  Created on: Nov 13, 2019
 *      Author: ans
 */

#include "TorControl.hpp"

namespace crawlservpp::Network {

	// constructor: set values, create context and socket
	TorControl::TorControl(
			const std::string& controlServer,
			unsigned short controlPort,
			const std::string& controlPassword
	)	: server(controlServer), port(controlPort), password(controlPassword), socket(this->context) {}

	// destructor: shutdown remaining connection if necessary
	TorControl::~TorControl() {
		asio::error_code error;

		this->socket.shutdown(asio::ip::tcp::socket::shutdown_both, error);

		if(error)
			std::cerr << "TorControl::~TorControl(): " << error.message() << std::endl;
	}

	// request new identitiy, throws TorControl::Exception
	void TorControl::newIdentity() {
		if(this->server.empty())
			throw Exception("TorControl::newIdentity(): No control server specified");

		// create context and resolver
		asio::ip::tcp::resolver resolver(this->context);

		try {
			// resolve address of control server
			asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(
					this->server,
					std::to_string(this->port),
					asio::ip::tcp::resolver::numeric_service
			);

			// connect to control server
			asio::connect(this->socket, endpoints);

			// send authentification
			const std::string auth("AUTHENTICATE \"" + this->password + "\"\n");

			this->socket.write_some(asio::buffer(auth.data(), auth.size()));

			// read response code (response should be "250 OK" or "515 Bad authentication")
			std::string response(4, 0);

			this->socket.read_some(asio::buffer(response.data(), 3));

			// check response code
			if(response != "250")
				throw Exception("TorControl::newIdentity(): Authentication failed");

			// asynchronosly send command to request a new identity
			const std::string command("SIGNAL NEWNYM\r\n");

			this->socket.async_write_some(
					asio::buffer(
							command.data(),
							command.size()
					),
					[&command](const asio::error_code& error, std::size_t bytes_transferred) {
						// data has been sent: check for error and whether data has been completely sent off
						if(error)
							std::cerr << "TorControl::newIdentity(): " << error.message() << std::endl;
						else if(bytes_transferred < command.size())
							std::cerr
									<< "TorControl::newIdentity(): TOR command not fully sent ("
									<< bytes_transferred
									<< "/"
									<< command.size()
									<< "B)"
									<< std::endl;
					}
			);
		}
		catch(const asio::system_error& e) {
			throw Exception("TorControl::newIdentity(): " + std::string(e.what()));
		}
	}

	// return whether TOR control server is specified
	TorControl::operator bool() const {
		return !(this->server.empty());
	}

} /* crawlservpp::Network */

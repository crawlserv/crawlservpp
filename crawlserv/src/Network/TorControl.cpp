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
	)		:	active(!controlServer.empty()),
				server(controlServer),
				port(controlPort),
				password(controlPassword),
				newIdentityAfter(0),
				socket(this->context),
				elapsed(0) {}

	// destructor: shutdown remaining connection if necessary
	TorControl::~TorControl() {
		if(this->socket.is_open()) {
			asio::error_code error;

			this->socket.shutdown(asio::ip::tcp::socket::shutdown_both, error);

			if(error)
				std::cerr << "TorControl::~TorControl(): " << error.message() << std::endl;
		}
	}

	// set time in seconds after which to request a new identity (or 0 for disabled)
	void TorControl::setNewIdentityTimer(unsigned long newIdentityAfterSeconds) {
		this->newIdentityAfter = newIdentityAfterSeconds;

		// reset timer
		this->elapsed = 0;

		this->timer.tick();
	}

	// request new identitiy, throws TorControl::Exception
	void TorControl::newIdentity() {
		if(!(this->active))
			throw Exception("TorControl::newIdentity(): No TOR control server set");

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

			asio::write(this->socket, asio::buffer(auth.data(), auth.size()));

			// read response code (response should be "250 OK" or "515 Bad authentication")
			char response[3]{ 0 };

			this->socket.read_some(asio::mutable_buffer(response, 3));

			// check response code
			if(response[0] != '2' || response[1] != '5' || response[2] != '0')
				throw Exception("TorControl::newIdentity(): Authentication failed");

			// send command to request a new identity
			const std::string command("SIGNAL NEWNYM\r\n");

			asio::write(this->socket, asio::buffer(command.data(), command.size()));

			// reset timer if necessary
			if(this->newIdentityAfter) {
				this->elapsed = 0;

				this->timer.tick();
			}
		}
		catch(const asio::system_error& e) {
			throw Exception("TorControl::newIdentity(): " + std::string(e.what()));
		}
	}

	// check whether to get a new identity
	void TorControl::tick() {
		// check whether enabled
		if(this->active && this->newIdentityAfter) {
			// get elapsed time (in ms)
			this->elapsed += this->timer.tick();

			// check elapsed time (in s)
			if(this->elapsed / 1000 > this->newIdentityAfter) {
				// request new identity
				this->newIdentity();

				// reset timer
				this->elapsed = 0;

				this->timer.tick();
			}
		}
	}

} /* crawlservpp::Network */

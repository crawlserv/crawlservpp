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
 * TorControl.hpp
 *
 * Connecting to a TOR control server to request a new identity.
 *
 *  Created on: Nov 13, 2019
 *      Author: ans
 */

#ifndef NETWORK_TORCONTROL_HPP_
#define NETWORK_TORCONTROL_HPP_

#include "Config.hpp"

#include "../Main/Exception.hpp"
#include "../Timer/Simple.hpp"

#include "asio.hpp"

#include <iostream>	// std::cerr, std::endl
#include <string>	// std::string, std::to_string

namespace crawlservpp::Network {

	class TorControl {
	public:
		// constructor and destructor
		TorControl(
				const std::string& controlServer,
				unsigned short controlPort,
				const std::string& controlPassword
		);
		~TorControl();

		// setter
		void setNewIdentityTimer(size_t newIdentityAfterSeconds);

		// request new identity
		void newIdentity();

		// check whether to request new identity
		void tick();

		// operator
		inline operator bool() const {
			return this->active;
		}

		// class for TorControl exceptions
		MAIN_EXCEPTION_CLASS();

		// not moveable, not copyable
		TorControl(TorControl&) = delete;
		TorControl(TorControl&&) = delete;
		TorControl& operator=(TorControl&) = delete;
		TorControl& operator=(TorControl&&) = delete;

	private:
		// settings
		const bool active;
		const std::string server;
		const unsigned short port;
		const std::string password;
		size_t newIdentityAfter;

		// asio context and socket
		asio::io_context context;
		asio::ip::tcp::socket socket;

		// identity time and timer
		Timer::Simple timer;
		unsigned long long elapsed;
	};

} /* crawlservpp::Network */

#endif /* NETWORK_TORCONTROL_HPP_ */

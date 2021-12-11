/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * SignalHandler.cpp
 *
 * Handles interrupting signals.
 *
 *  Created on: Dec 11, 2021
 *      Author: ans
 */

#include "SignalHandler.hpp"

namespace crawlservpp::Main {

	volatile std::sig_atomic_t SignalHandler::interruptionSignal{};

	//! Constructor.
	/*!
	 * Initializes signal handling.
	 */
	SignalHandler::SignalHandler() {
#ifdef _WIN32
		signal(SIGINT, SignalHandler::signal);
		signal(SIGTERM, App::signal);
#else
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
		struct sigaction sigIntHandler;

		sigIntHandler.sa_handler = SignalHandler::signal;

		sigemptyset(&sigIntHandler.sa_mask);

		sigIntHandler.sa_flags = 0;

		sigaction(SIGINT, &sigIntHandler, nullptr);
		sigaction(SIGTERM, &sigIntHandler, nullptr);
#endif
	}

	//! Destructor.
	SignalHandler::~SignalHandler() {}

	//! Checks for interrupting signal.
	/*!
	 * Shuts the program down if an interrupting signal
	 *  has occurred.
	 */
	void SignalHandler::tick() {
		if(SignalHandler::interruptionSignal != 0) {
			this->shutdown(SignalHandler::interruptionSignal);
		}
	}

	//! Static signal handler.
	/*!
	 * Forwards a signal to the class.
	 */
	void SignalHandler::signal(const int signalNumber) {
		SignalHandler::interruptionSignal = signalNumber;
	}

} /* namespace crawlservpp::Main */

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
 * SignalHandler.hpp
 *
 * Handles interrupting signals.
 *
 *  Created on: Dec 11, 2021
 *      Author: ans
 */

#ifndef MAIN_SIGNALHANDLER_HPP_
#define MAIN_SIGNALHANDLER_HPP_

#include <csignal>	// sigaction, sigemptyset/signal, SIGINT, SIGTERM, std::sig_atomic_t

namespace crawlservpp::Main {

	class SignalHandler {
	public:
		///@name Construction and Destruction
		///@{

		SignalHandler();
		virtual ~SignalHandler();

		///@}

	protected:
		///@name Tick
		///@{

		void tick();

		///@}
		///@name Signal Handling
		///@{

		static void signal(int signalNumber);

		//! In-class signal handler shutting down the application.
		virtual void shutdown(std::sig_atomic_t signal) = 0;

		///@}

	private:
		static volatile std::sig_atomic_t interruptionSignal;
	};

} /* namespace crawlservpp::Main */

#endif /* MAIN_SIGNALHANDLER_HPP_ */

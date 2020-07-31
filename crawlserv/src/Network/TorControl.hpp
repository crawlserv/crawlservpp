/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * Connecting to the TOR control server/port to request a new identity.
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

#include <array>		// std::array
#include <cstdint>		// std::uint16_t, std::uint64_t
#include <iostream>		// std::cerr, std::endl
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view

namespace crawlservpp::Network {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The length of a HTTP response code.
	constexpr auto responseCodeLength{3};

	//! The number of milliseconds per second.
	constexpr auto millisecondsPerSecond{1000};

	///@}

	/*
	 * DECLARATION
	 */

	//! Controls a TOR service via a TOR control server/port, if available.
	/*!
	 * Allows crawlserv++ to automatically request a new TOR identity
	 *  when needed if the TOR control server/port has been set in the configuration.
	 *
	 * This class is used both by crawler and by extractor threads.
	 *
	 * It uses the asio library for connecting to the TOR control server/port.
	 *  For more information, see its
	 *  <a href="https://github.com/chriskohlhoff/asio/">GitHub repository</a>.
	 *
	 * \sa Network::Config
	 */
	class TorControl {
	public:
		///@name Construction and Destruction
		///@{

		TorControl(
				std::string_view controlServer,
				std::uint16_t controlPort,
				std::string_view controlPassword
		);
		virtual ~TorControl();

		///@}
		///@name Getter
		///@{

		[[nodiscard]] bool active() const noexcept;

		///@}
		///@name Setters
		///@{

		void setNewIdentityMin(std::uint64_t seconds);
		void setNewIdentityMax(std::uint64_t seconds);

		///@}
		///@name Identity
		///@{

		bool newIdentity();

		///@}
		///@name Tick
		///@{

		void tick();

		///@}

		//! Class for TOR control exceptions.
		/*!
		 * This exception is being thrown when:
		 * - a new TOR identity has been requested, but no TOR control server/port is set
		 * - authentification with the given password to the TOR control server/port failed
		 * - an error occured while connecting to the TOR control server/port
		 *
		 * \sa newIdentity
		 */
		MAIN_EXCEPTION_CLASS();

		/**@name Copy and Move
		 * The class is not copyable and not moveable.
		 */
		///@{

		//! Deleted copy constructor.
		TorControl(TorControl&) = delete;

		//! Deleted copy assignment operator.
		TorControl& operator=(TorControl&) = delete;

		//! Deleted move constructor.
		TorControl(TorControl&&) = delete;

		//! Deleted move assignment operator.
		TorControl& operator=(TorControl&&) = delete;

		///@}

	private:
		// settings
		const bool isActive;
		const std::string server;
		const std::uint16_t port;
		const std::string password;
		std::uint64_t newIdentityNotBefore;
		std::uint64_t newIdentityAfter;

		// asio context and socket
		asio::io_context context;
		asio::ip::tcp::socket socket;

		// identity time and timers
		Timer::Simple minTimer;
		Timer::Simple maxTimer;
		std::uint64_t elapsedMin;
		std::uint64_t elapsedMax;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor creating context and socket for the connection to the TOR control server/port.
	/*!
	 * \param controlServer A string view containing the address of the TOR control server.
	 *   It will be copied into the instance of the class for later use.
	 *
	 * \param controlPort The port used for controlling the TOR service.
	 *
	 * \param controlPassword A string view containing the password with which to
	 *   authentificate to the TOR control server/port. It will be copied into the
	 *   instance of the class for later use.
	 *
	 */
	inline TorControl::TorControl(
			std::string_view controlServer,
			std::uint16_t controlPort,
			std::string_view controlPassword
	)		:	isActive(!controlServer.empty()),
				server(controlServer),
				port(controlPort),
				password(controlPassword),
				newIdentityNotBefore(0),
				newIdentityAfter(0),
				socket(this->context),
				elapsedMin(0),
				elapsedMax(0) {}

	//! Destructor shutting down remaining connections to the TOR control server/port if necessary.
	/*!
	 * \note Errors during shutdown will be written to stderr.
	 */
	inline TorControl::~TorControl() {
		if(this->socket.is_open()) {
			asio::error_code error;

			this->socket.shutdown(asio::ip::tcp::socket::shutdown_both, error);

			if(error) {
				std::cerr	<< "TorControl::~TorControl(): "
							<< error.message()
							<< std::endl;
			}
		}
	}

	//! Gets whether a TOR control server/port is set.
	/*!
	 * \returns True, if a TOR control server/port is set.
	 *   False otherwise.
	 */
	inline bool TorControl::active() const noexcept {
		return this->isActive;
	}

	//! Sets the time (in seconds) in which to ignore requests for a new identity.
	/*!
	 * After having already requestes a new TOR identity (or having started
	 *  this instance of the TOR controller) all requests for a new TOR identity
	 *  will be discarded for the given amount of time.
	 *
	 * \param seconds Time (in seconds) in which to ignore requests for a new identity.
	 *   Set it to zero (default) if every request for a new TOR identity should
	 *   be sent to the TOR control server/port.
	 */
	inline void TorControl::setNewIdentityMin(std::uint64_t seconds) {
		this->newIdentityNotBefore = seconds;

		// reset timer
		this->elapsedMin = 0;

		this->minTimer.tick();
	}

	//! Sets the time (in seconds) after which to automatically request a new TOR identity.
	/*!
	 * After the time has passed (and tick() is executed), a new TOR identity will be
	 *  automatically requested.
	 *
	 * \param seconds Time (in seconds) after which to automatically request a new TOR identity.
	 *   Set it to zero (default) for no automatic request of new TOR identities.
	 */
	inline void TorControl::setNewIdentityMax(std::uint64_t seconds) {
		this->newIdentityAfter = seconds;

		// reset timer
		this->elapsedMax = 0;

		this->maxTimer.tick();
	}

	//! Requests a new TOR identity via the set TOR control server/port.
	/*!
	 * The request will be ignored if not enough time (set via setNewIdentityMin)
	 *  has been passed. Sends the NEWNYM signal to the TOR control server/port,
	 *  requesting a new circuit.
	 *
	 * \note The TOR service itself does not allow too many requests for new
	 *   circuits during a specific period of time.
	 *
	 * \returns True if a new identity has been requested. False otherwise.
	 *
	 * \throws TorControl::Exception if no TOR control server/port has been set,
	 *   authentification with the given password to the TOR control server/port
	 *   failed, or an error occured while connecting to the TOR control server/port.
	 */
	inline bool TorControl::newIdentity() {
		// check whether a TOR control server/port has been set
		if(!(this->isActive)) {
			throw Exception("No TOR control server/port set");
		}

		// check whether a sufficient amount of time has passed since the last request
		if(this->newIdentityNotBefore > 0) {
			this->elapsedMin += this->minTimer.tick();

			if(this->elapsedMin / millisecondsPerSecond < this->newIdentityNotBefore) {
				return false;
			}

			this->elapsedMin = 0;
		}

		// create asio resolver
		asio::ip::tcp::resolver resolver(this->context);

		try {
			// resolve the address of the control server
			asio::ip::tcp::resolver::results_type endpoints{
				resolver.resolve(
						this->server,
						std::to_string(this->port),
						asio::ip::tcp::resolver::numeric_service
				)
			};

			// connect to control server
			asio::connect(this->socket, endpoints);

			// send authentification
			const std::string auth("AUTHENTICATE \"" + this->password + "\"\n");

			asio::write(this->socket, asio::buffer(auth.data(), auth.size()));

			// read response code (response should be "250 OK" or "515 Bad authentication")
			std::array<char, responseCodeLength> response{};

			this->socket.read_some(asio::mutable_buffer(response.data(), responseCodeLength));

			// check response code
			if(response[0] != '2' || response[1] != '5' || response[2] != '0') {
				throw Exception("Authentification failed");
			}

			// send command to request a new identity
			const std::string command("SIGNAL NEWNYM\r\n");

			asio::write(this->socket, asio::buffer(command.data(), command.size()));

			// reset timer if necessary
			if(this->newIdentityAfter > 0) {
				this->elapsedMax = 0;

				this->maxTimer.tick();
			}
		}
		catch(const asio::system_error& e) {
			throw Exception(e.what());
		}

		return true;
	}

	//! Checks whether to request a new TOR identity.
	/*!
	 * This function will be called every server tick
	 *  and will request a new TOR identity if necessary.
	 */
	inline void TorControl::tick() {
		// check whether timer is enabled
		if(this->isActive && this->newIdentityAfter > 0) {
			// get elapsed time (in ms)
			this->elapsedMin += this->minTimer.tick();
			this->elapsedMax += this->maxTimer.tick();

			// check elapsed time (in s)
			if(this->elapsedMax / millisecondsPerSecond > this->newIdentityAfter) {
				// request new identity
				this->newIdentity();

				// reset timer
				this->elapsedMax = 0;

				this->maxTimer.tick();
			}
		}
	}

} /* namespace crawlservpp::Network */

#endif /* NETWORK_TORCONTROL_HPP_ */

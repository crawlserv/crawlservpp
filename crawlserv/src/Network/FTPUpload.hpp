/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
 * FTPUpload.hpp
 *
 *  Created on: Jan 25, 2022
 *      Author: ans
 */

#ifndef NETWORK_FTPUPLOAD_HPP_
#define NETWORK_FTPUPLOAD_HPP_

#include "../Wrapper/Curl.hpp"

#ifndef CRAWLSERVPP_TESTING

#include "../Helper/Portability/curl.h"

#else

#include "FakeCurl/FakeCurl.hpp"

#endif

#include <cstddef>		// std::size_t
#include <cstring>		// std::memcpy
#include <stdexcept>	// std::runtime_error
#include <string>		// std::string

namespace crawlservpp::Network::FTPUpload {

	/*
	 * DECLARATION
	 */

	//! Writes data into a FTP file using the @c libcurl library.
	/*!
	 * \param content Constant reference to the content
	 *   that should be written to the new file.
	 * \param url Constant reference to a string
	 *   containing the URL including protocol. If the
	 *   FTP server requires authentification, it needs
	 *   to be included into the URL as follows:
	 *   @c ftp[s]://username:password@example.com
	 * \param proxy Constant reference to a string
	 *   containing the URL of the proxy server to be
	 *   used. If the string is empty, no proxy server
	 *   will be used.
	 */
	void write(
			const std::string& content,
			const std::string& url,
			const std::string& proxy = std::string{},
			bool verbose = false
	);

	//! Custom reader function for FTP transfers.
	/*!
	 * \param bufptr Pointer to the buffer to write to.
	 * \param size Size of each item to be written.
	 * \param nitems Number of items to be written.
	 * \param userp Pointer to the state of the FTP
	 *   operation.
	 *
	 * \sa State
	 */
	std::size_t read(char * bufptr, std::size_t size, std::size_t nitems, void * userp);

	//! Checks the result of a @c libcurl operation and throws an exception if an error occured.
	/*!
	 * \param code The result code from @libcurl to be
	 *   checked.
	 *
	 * \throws @c std::runtime_error If the given
	 *   result code indicates an error.
	 */
	void check(CURLcode code);

	/*
	 * STRUCTURE
	 */

	//! Stores content and status of a FTP upload.
	struct State {
		//! Default constructor.
		State() = default;

		//! Constant pointer to the content to be uploaded.
		const char * content{nullptr};

		//! Size of the content to be uploaded.
		std::size_t size{};

		//! Number of bytes that have already been uploaded.
		std::size_t transferred{};

		//! Deleted copy constructor.
		State(State&) = delete;

		//! Deleted move constructor.
		State(State&&) = delete;

		//! Deleted copy operator.
		State& operator=(State&) = delete;

		//! Deleted move operator.
		State& operator=(State&&) = delete;
	};

	/*
	 * IMPLEMENTATION
	 */

	void inline write(
			const std::string& content,
			const std::string& url,
			const std::string& proxy,
			bool verbose
	) {
		Wrapper::Curl curl;
		State state;

		state.content = content.c_str();
		state.size = content.size();

		if(!curl.valid()) {
			throw std::runtime_error("Could not initialize libcurl wrapper");
		}

		FTPUpload::check(curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str()));
		FTPUpload::check(curl_easy_setopt(curl.get(), CURLOPT_USE_SSL, CURLUSESSL_TRY));
		FTPUpload::check(curl_easy_setopt(curl.get(), CURLOPT_UPLOAD, 1L));
		FTPUpload::check(curl_easy_setopt(curl.get(), CURLOPT_READFUNCTION, FTPUpload::read));
		FTPUpload::check(curl_easy_setopt(curl.get(), CURLOPT_READDATA, &state));
		FTPUpload::check(
				curl_easy_setopt(
						curl.get(),
						CURLOPT_INFILESIZE_LARGE,
						static_cast<curl_off_t>(state.size)
				)
		);

		if(!proxy.empty()) {
			FTPUpload::check(curl_easy_setopt(curl.get(), CURLOPT_PROXY, proxy.c_str()));
		}

		if(verbose) {
			FTPUpload::check(curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1L));
		}

		FTPUpload::check(curl_easy_perform(curl.get()));
	}

	inline std::size_t read(char * bufptr, std::size_t size, std::size_t nitems, void * userp) {
		auto toRead{size * nitems};
		auto * state{static_cast<State *>(userp)};
		const auto restSize{state->size - state->transferred};

		if(toRead > restSize) {
			toRead = restSize;
		}

		if(toRead == 0) {
			return 0;
		}

		std::memcpy(bufptr, state->content + state->transferred, toRead);

		state->transferred += toRead;

		return toRead;
	}

	inline void check(CURLcode code) {
		if(code != CURLE_OK) {
			throw std::runtime_error(curl_easy_strerror(code));
		}
	}

} /* namespace crawlservpp::Network::FTPUpload */

#endif /* NETWORK_FTPUPLOAD_HPP_ */

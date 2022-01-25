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
 * Downloader.hpp
 *
 * Using the libcurl library to provide simple download functionality.
 *  NOT THREAD-SAFE! Use multiple instances for multiple threads.
 *
 *  Created on: Feb 15, 2021
 *      Author: ans
 */

#ifndef NETWORK_DOWNLOADER_HPP_
#define NETWORK_DOWNLOADER_HPP_

#include "../Wrapper/Curl.hpp"

#ifndef CRAWLSERVPP_TESTING

#include "../Helper/Portability/curl.h"

#else

#include "FakeCurl/FakeCurl.hpp"

#endif

#include <atomic>		// std::atomic
#include <cstddef>		// std::size_t
#include <memory>		// std::unique_ptr
#include <stdexcept>	// std::runtime_error
#include <string>		// std::string
#include <thread>		// std::thread

namespace crawlservpp::Network {

	/*
	 * DECLARATION
	 */

	//! Downloader using the @c libcurl library to download a URL in an extra thread.
	class Downloader {
	public:
		///@name Construction and Destruction
		///@{

		Downloader(const std::string& url, const std::string& proxy = std::string{});
		~Downloader();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] bool isRunning() const noexcept;
		[[nodiscard]] std::string getContent() const;
		[[nodiscard]] std::string getError() const;

		///@}

	private:
		std::thread thread;
		std::atomic<bool> running{true};
		std::string content;
		std::string error;

		// thread function
		void threadFunction(const std::string& url, const std::string& proxy);

		// internal helper functions
		void configure(Wrapper::Curl& curl, const std::string& url, const std::string& proxy);
		void download(Wrapper::Curl& curl);
		void check(CURLcode result);

		// static internal helper function
		static std::size_t writer(void * data, std::size_t size, std::size_t nmemb, void * ptr);
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Constructor starting to download a URL using a specific proxy server.
	/*!
	 * \param url The URL to download.
	 * \param proxy The proxy server used for
	 *   the download. No specific proxy server
	 *   is used if the string is empty.
	 */
	inline Downloader::Downloader(const std::string& url, const std::string& proxy)
		: thread(&Downloader::threadFunction, this, url, proxy) {}

	//! Destructor.
	/*!
	 * Joins the download with the main thread.
	 */
	inline Downloader::~Downloader() {
		if(this->thread.joinable()) {
			this->thread.join();
		}
	}

	/*
	 * GETTERS
	 */

	//! Returns whether the download is still in progress.
	/*!
	 * \note This function is thread-safe.
	 *
	 * \returns True, if the download is still in
	 *   progress. False otherwise.
	 */
	inline bool Downloader::isRunning() const noexcept {
		return this->running;
	}

	//! Returns the downloaded content, if successfully downloaded.
	/*!
	 * \note This function is thread-safe.
	 *
	 * \returns The content of the downloaded
	 *   file or an empty string, if the download
	 *   is still in progress or has failed.
	 *
	 * \sa getError
	 */
	inline std::string Downloader::getContent() const {
		if(this->running) {
			return std::string{};
		}

		return this->content;
	}

	//! Returns the download error, if one occured.
	/*!
	 * \note This function is thread-safe.
	 *
	 * \returns The error, if one occured, or an
	 *   empty string if the download is still in
	 *   progress or was successful.
	 *
	 * \sa getContent
	 */
	inline std::string Downloader::getError() const {
		if(this->running) {
			return std::string{};
		}

		return this->error;
	}

	/*
	 * THREAD FUNCTION (private)
	 */

	// downloads the specified URL using the specified proxy, blocks until download was complete or failed
	inline void Downloader::threadFunction(const std::string& url, const std::string& proxy) {
		Wrapper::Curl curl;

		if(curl.valid()) {
			try {
				this->configure(curl, url, proxy);
				this->download(curl);
			}
			catch(const std::runtime_error& e) {
				this->error = e.what();
			}
		}

		this->running = false;
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// set download options
	inline void Downloader::configure(Wrapper::Curl& curl, const std::string& url, const std::string& proxy) {
		this->check(curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str()));
		this->check(curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, Downloader::writer));
		this->check(curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, static_cast<void *>(&(this->content))));

		if(!proxy.empty()) {
			this->check(curl_easy_setopt(curl.get(), CURLOPT_PROXY, proxy.c_str()));
		}
	}

	// download file
	inline void Downloader::download(Wrapper::Curl& curl) {
		this->check(curl_easy_perform(curl.get()));
	}

	// check result and throw exception if an error occured
	inline void Downloader::check(CURLcode code) {
		if(code != CURLE_OK) {
			throw std::runtime_error(curl_easy_strerror(code));
		}
	}

	/*
	 * STATIC INTERNAL HELPER FUNCTION (private)
	 */

	// downloads the given URL in an extra thread
	inline std::size_t Downloader::writer(
			void * data,
			std::size_t size,
			std::size_t nmemb,
			void * ptr
	) {
		// check arguments
		if(
				content == nullptr
				|| data == nullptr
				|| size == 0
				|| nmemb == 0
		) {
			return 0;
		}

		const auto bytes{size * nmemb};

		// append data to buffer
		static_cast<std::string *>(content)->append(static_cast<const char *>(data), size * nmemb);

		// return written (i.e. all received) bytes
		return bytes;
	}

} /* namespace crawlservpp::Network */

#endif /* NETWORK_DOWNLOADER_HPP_ */

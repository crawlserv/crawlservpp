/*
 * WebServer.h
 *
 * Embedded web server using mongoose.
 *
 *  NOTE:	The web server does one final poll on destruction.
 *  		When used inside classes it should therefore declared last/destroyed first, in case it uses other member data.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#ifndef MAIN_WEBSERVER_HPP_
#define MAIN_WEBSERVER_HPP_

#define MG_ENABLE_HTTP_STREAMING_MULTIPART 1

// hard-coded constant
#define MAIN_WEBSERVER_FILE_LENGTH 64

#include "../Helper/FileSystem.hpp"
#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"

#include "../_extern/mongoose/mongoose.h"

#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <iostream> // DEBUG

namespace crawlservpp::Main {

	class WebServer final {
		// for convenience
		typedef mg_connection * ConnectionPtr;
		typedef std::function<void(ConnectionPtr)> AcceptCallback;
		typedef std::function<void(
						ConnectionPtr,
						const std::string&,
						const std::string&,
						void *
				)> RequestCallback;

	public:
		WebServer(const std::string& fileCacheDirectory);
		virtual ~WebServer();

		// initializer
		void initHTTP(const std::string& port);

		// setters
		void setCORS(const std::string& allowed);
		void setAcceptCallback(AcceptCallback callback);
		void setRequestCallback(RequestCallback callback);

		// server functions
		void poll(int timeOut);
		void send(
				ConnectionPtr connection,
				unsigned short code,
				const std::string& type,
				const std::string& content
		);

		void sendFile(
				ConnectionPtr connection,
				const std::string& fileName,
				void * data
		);

		static void sendError(
				ConnectionPtr connection,
				const std::string& error
		);

		static void close(ConnectionPtr connection);

		// static helper function
		static std::string getIP(ConnectionPtr connection);

		// sub-class for web server exception
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
		};

		// not moveable, not copyable
		WebServer(WebServer&) = delete;
		WebServer(WebServer&&) = delete;
		WebServer& operator=(WebServer&) = delete;
		WebServer& operator=(WebServer&&) = delete;

	private:
		const std::string fileCache;

		mg_mgr eventManager;
		std::string cors;
		std::string oldFileName;

		// callback functions
		AcceptCallback onAccept;
		RequestCallback onRequest;

		// event handlers
		static void eventHandler(ConnectionPtr connection, int event, void * data);
		void eventHandlerInClass(ConnectionPtr connection, int event, void * data);

		// private helper functions
		std::string getCorsHeaders();
		mg_str generateFileNameInClass();
		static mg_str generateFileName(ConnectionPtr connection, mg_str fileName);

		// struct for user data while uploading file
		struct FileUploadData {
			std::string name;
			std::ofstream stream;

			FileUploadData(const std::string& fileName, const std::string& fullName) : name(fileName) {
				this->stream.open(fullName.c_str(), std::ios_base::binary);
			}
		};
	};

} /* crawlservpp::Main */

#endif /* MAIN_WEBSERVER_HPP_ */

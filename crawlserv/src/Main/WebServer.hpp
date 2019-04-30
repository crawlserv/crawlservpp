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

#include "../Helper/FileSystem.hpp"
#include "../Helper/Strings.hpp"

#define MG_ENABLE_HTTP_STREAMING_MULTIPART 1

#include "../_extern/mongoose/mongoose.h"

#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace crawlservpp::Main {

	class WebServer final {
		// for convenience
		typedef struct mg_connection * ConnectionPtr;
		typedef std::function<void(ConnectionPtr)> AcceptCallback;
		typedef std::function<void(ConnectionPtr, const std::string&, const std::string&)> RequestCallback;

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
		static void sendError(
				ConnectionPtr connection,
				const std::string& error
		);
		static void close(ConnectionPtr connection);

		// static helper function
		static std::string getIP(ConnectionPtr connection);

		// not moveable, not copyable
		WebServer(WebServer&) = delete;
		WebServer(WebServer&&) = delete;
		WebServer& operator=(WebServer&) = delete;
		WebServer& operator=(WebServer&&) = delete;

	private:
		const std::string fileCache;

		mg_mgr eventManager;
		std::string cors;

		// callback functions
		AcceptCallback onAccept;
		RequestCallback onRequest;

		// event handler
		static void eventHandler(ConnectionPtr connection, int event, void * data);
		void eventHandlerInClass(ConnectionPtr connection, int event, void * data);

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

/*
 * WebServer.h
 *
 * Embedded web server using mongoose.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#ifndef MAIN_WEBSERVER_H_
#define MAIN_WEBSERVER_H_

#include "../_extern/mongoose/mongoose.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace crawlservpp::Main {

class WebServer final {
public:
	// for convenience
	typedef struct mg_connection * Connection;
	typedef std::function<void(Connection)> AcceptCallback;
	typedef std::function<void(Connection, const std::string&, const std::string&)> RequestCallback;

	WebServer();
	virtual ~WebServer();

	// initializer
	void initHTTP(const std::string& port);

	// setters
	void setCORS(const std::string& allowed);
	void setAcceptCallback(AcceptCallback callback);
	void setRequestCallback(RequestCallback callback);

	// server functions
	void poll(int timeOut);
	void send(Connection connection, unsigned short code, const std::string& type, const std::string& content);
	void close(Connection connection);

	// static helper function
	static std::string getIP(Connection connection);

	// not moveable, not copyable
	WebServer(WebServer&) = delete;
	WebServer(WebServer&&) = delete;
	WebServer& operator=(WebServer&) = delete;
	WebServer& operator=(WebServer&&) = delete;

private:
	mg_mgr eventManager;
	std::string cors;

	// callback functions
	AcceptCallback onAccept;
	RequestCallback onRequest;

	// event handler
	static void eventHandler(Connection connection, int event, void * data);
	void eventHandlerInClass(Connection connection, int event, void * data);
};

}

#endif /* MAIN_WEBSERVER_H_ */

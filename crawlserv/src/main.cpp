/*
 * main.cpp
 *
 * crawlserv++ command-and-control server
 *
 * https://github.com/crawlserv/crawlservpp/
 *
 * Entry point of the application. One line only! Use the App class for basic application functionality instead (OOP).
 *
 *  Created on: Sep 29, 2018
 *      Author: ans
 */

#include "Main/App.hpp"

int main(int argc, char * argv[]) { return crawlservpp::Main::App(argc, argv).run(); }

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
 * crawlserv++ command-and-control server
 *
 * See https://github.com/crawlserv/crawlservpp/ for the latest version of the application.
 *
 * * See 'Main/Version.hpp' for version information.
 *
 * ---
 *
 * main.cpp
 *
 * Entry point of the application. One line only! Use the App class for basic application functionality instead (OOP).
 *
 *  Created on: Sep 29, 2018
 *      Author: ans
 */

#include "Main/App.hpp"

int main(int argc, char * argv[]) { return crawlservpp::Main::App(argc, argv).run(); }

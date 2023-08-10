/*
* Nirvana IDL to C++ compiler.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2021 Igor Popov.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*
* Send comments and/or bug reports to:
*  popov.nirvana@gmail.com
*/
#ifndef NIDL2CPP_OPTIONS_H_
#define NIDL2CPP_OPTIONS_H_
#pragma once

struct Options
{
public:
	Options () :
		servant_suffix ("_s"),
		proxy_suffix ("_p"),
		client (true),
		server (true),
		proxy (true),
		legacy (false),
		no_servant (false),
		no_client_cpp (false)
	{}

	std::filesystem::path out_h, out_cpp;
	std::string client_suffix;
	std::string servant_suffix;
	std::string proxy_suffix;
	std::string inc_cpp;
	bool client, server, proxy;
	bool legacy;
	bool no_servant;
	bool no_client_cpp;
};

#endif

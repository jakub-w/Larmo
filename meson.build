# Copyright (C) 2019 by Jakub Wojciech

# This file is part of Lelo Remote Music Player.

# Lelo Remote Music Player is free software: you can redistribute it
# and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.

# Lelo Remote Music Player is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with Lelo Remote Music Player. If not, see
# <https://www.gnu.org/licenses/>.

project('remote-music-control', 'cpp',
	default_options: ['cpp_std=c++17'])

# if get_option('buildtype').startswith('debug')
#   add_project_arguments('-Wall', '-Wextra', '-Werror', '-DFORTIFY_SOURCE=1',
# 			'-ggdb3',
# 			language : 'cpp')
# endif
# elif get_option('buildtype') == 'release'
#   add_project_arguments('-DNDEBUG', '-O3', '-s',
#   			language : 'cpp')
# endif

mpv_dep = dependency('mpv')
boost_dep = dependency('boost', modules: ['asio'])

include = include_directories('include')

sources = ['main.cpp']

protobuf_dep = dependency('protobuf')
grpc_dep = dependency('grpc++')
grpc_sources = ['player_service.pb.cc', 'player_service.grpc.pb.cc']

executable('remote-control',
	   sources: ['remote-control.cpp',
		     'Config.cpp',
		     'PlayerClient.cpp',
		     'TempFile.cpp',
		     'Util.cpp',
		     grpc_sources],
	   link_args: '-lstdc++fs',
	   dependencies: [grpc_dep, protobuf_dep],
	   include_directories: include)

# TODO: protoc -I . --cpp_out=. player_service.proto
# 	protoc -I . --grpc_out=. --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin player_service.proto
# 	/usr/bin/... can be replaced with $(which grpc_cpp_plugin)

executable('remote-player',
	   sources: ['remote-player.cpp',
		     'Config.cpp',
		     'Player.cpp',
		     'PlayerServiceImpl.cpp',
		     'Util.cpp',
		     grpc_sources],
	   include_directories: include,
	   dependencies: [mpv_dep, grpc_dep, protobuf_dep])


executable('temp',
	   sources: ['temp.cpp', 'Config.cpp', 'Util.cpp'],
	   include_directories: include)

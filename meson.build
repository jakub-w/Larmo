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

# Build types
if get_option('buildtype').startswith('debug')
  add_project_arguments('-Wall', '-Wextra', '-Werror', '-DFORTIFY_SOURCE=1',
			# TODO: this should be temporary, protobuff is
			#       generating a lot of those warnings
			'-Wno-unused-parameter',
			'-ggdb3',
			language : 'cpp')
elif get_option('buildtype') == 'release'
  add_project_arguments('-DNDEBUG', '-O3', '-s',
  			language : 'cpp')
endif


# Dependencies
mpv_dep = dependency('mpv')
boost_dep = dependency('boost', modules: ['asio'])


# Protobuff
protobuf_dep = dependency('protobuf')
grpc_dep = dependency('grpc++')

protoc = find_program('protoc')
grpc_cpp_plugin = find_program('grpc_cpp_plugin')

gen_protobuf = generator(protoc,
			 output: ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
			 arguments: ['--proto_path=@CURRENT_SOURCE_DIR@',
				     # '--cpp_out=@BUILD_DIR@',
				     '--cpp_out=@CURRENT_SOURCE_DIR@',
				     '@INPUT@'])
gen_protobuf_grpc = generator(protoc,
			      output: ['@BASENAME@.grpc.pb.cc',
				       '@BASENAME@.grpc.pb.h'],
			      arguments: ['--proto_path=@CURRENT_SOURCE_DIR@',
					  # '--grpc_out=@BUILD_DIR@',
					  '--grpc_out=@CURRENT_SOURCE_DIR@',
					  '--plugin=protoc-gen-grpc=' +
					  grpc_cpp_plugin.path(),
					  '@INPUT@'])

protobuf_files = [gen_protobuf_grpc.process('player_service.proto'),
		  gen_protobuf.process('player_service.proto')]


# Includes
include = include_directories('include')


# Executables
executable('remote-control',
	   sources: ['remote-control.cpp',
		     'Config.cpp',
		     'PlayerClient.cpp',
		     'TempFile.cpp',
		     'Util.cpp',
		     protobuf_files],
	   link_args: '-lstdc++fs',
	   dependencies: [grpc_dep, protobuf_dep],
	   include_directories: include)

executable('remote-player',
	   sources: ['remote-player.cpp',
		     'Config.cpp',
		     'Player.cpp',
		     'PlayerServiceImpl.cpp',
		     'Util.cpp',
		     protobuf_files],
	   include_directories: include,
	   dependencies: [mpv_dep, grpc_dep, protobuf_dep])


executable('temp',
	   sources: ['temp.cpp', 'Config.cpp', 'Util.cpp'],
	   include_directories: include)

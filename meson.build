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

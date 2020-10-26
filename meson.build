# Copyright (C) 2019-2020 by Jakub Wojciech

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

project('lelo-remote-music', 'cpp',
	default_options: ['cpp_std=c++17'])

# Build types
if get_option('buildtype').startswith('debug')
  add_project_arguments('-Wall', '-Wextra', '-Werror', '-DFORTIFY_SOURCE=1',
			'-ggdb3',
			'-Og',
			# '-fsanitize=address',
			# '-fsanitize=leak',
			# '-fsanitize=undefined',
 			language : 'cpp')
  add_project_link_arguments(
    # '-lasan',
    # '-ltsan',
    # '-lubsan',
    language : 'cpp')

elif get_option('buildtype') == 'release'
  add_project_arguments('-DNDEBUG', '-O2',
  			language : 'cpp')
endif


# Check if the compiler uses 'filesystem' or 'experimental/filesystem'.
# Define an appropriate macro if necessary.
cpp_compiler = meson.get_compiler('cpp')
if not cpp_compiler.has_header('filesystem')
  if cpp_compiler.has_header('experimental/filesystem', required: true)
    add_global_arguments('-DINCLUDE_STD_EXPERIMENTAL_FILESYSTEM',
			 language : 'cpp')
  endif
endif

if not cpp_compiler.has_header('grpcpp/grpcpp.h')
  if cpp_compiler.has_header('grpc++/grpc++.h', required: true)
    add_global_arguments('-DINCLUDE_GRPCPLUSPLUS', language : 'cpp')
  endif
endif


# Dependencies
mpv_dep = dependency('mpv')
if cpp_compiler.has_header('asio.hpp', required: true)
  boost_dep = dependency('boost')
endif

openssl_dep = dependency('openssl')


# Protobuff
protobuf_dep = dependency('protobuf')
grpc_dep = dependency('grpc++', version: '>=1.3.2')

protoc = find_program('protoc')
grpc_cpp_plugin = find_program('grpc_cpp_plugin')

gen_protobuf = generator(protoc,
			 output: ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
			 arguments: ['--proto_path=@CURRENT_SOURCE_DIR@',
				     '--cpp_out=@BUILD_DIR@',
				     # '--cpp_out=@CURRENT_SOURCE_DIR@',
				     '@INPUT@'])
gen_protobuf_grpc = generator(protoc,
			      output: ['@BASENAME@.grpc.pb.cc',
				       '@BASENAME@.grpc.pb.h'],
			      arguments: ['--proto_path=@CURRENT_SOURCE_DIR@',
					  '--grpc_out=@BUILD_DIR@',
					  # '--grpc_out=@CURRENT_SOURCE_DIR@',
					  '--plugin=protoc-gen-grpc=' +
					  grpc_cpp_plugin.path(),
					  '@INPUT@'])

protobuf_files = [gen_protobuf_grpc.process('player_service.proto'),
		  gen_protobuf.process('player_service.proto')]

protobuf_daemon_files = gen_protobuf.process('daemon_arguments.proto')


# spdlog
spdlog_dep = declare_dependency(
  sources: 'spdlog/src/spdlog.cpp',
  compile_args: '-DSPDLOG_COMPILED_LIB',
  include_directories : include_directories('spdlog/include'))


crypto_sources = ['crypto/BigNum.cpp',
		  'crypto/CryptoUtil.cpp',
		  'crypto/SslUtil.cpp',
		  'crypto/certs/CertsUtil.cpp',
		  'crypto/certs/KeyPair.cpp',
		  'crypto/certs/Certificate.cpp',
		  'crypto/certs/CertificateAuthority.cpp',
		  'crypto/certs/CertificateRequest.cpp']


# Executables
executable('remote-control',
	   sources: ['remote-control.cpp',
		     'Config.cpp',
		     'Daemon.cpp',
		     'PlaybackState.cpp',
		     'PlaybackSynchronizer.cpp',
		     'PlayerClient.cpp',
		     'Util.cpp',
		     'crypto/CryptoUtil.cpp',
		     'crypto/ZkpSerialization.cpp',
		     'crypto/SslUtil.cpp',
		     protobuf_files,
		     protobuf_daemon_files],
	   link_args: ['-lstdc++fs', '-lgpr', '-lpthread'],
	   dependencies: [grpc_dep, protobuf_dep, boost_dep, spdlog_dep,
			  openssl_dep])

executable('remote-player',
	   sources: ['remote-player.cpp',
		     'Config.cpp',
		     'PlaybackState.cpp',
		     'Player.cpp',
		     'crypto/CryptoUtil.cpp',
		     'crypto/ZkpSerialization.cpp',
		     'crypto/SslUtil.cpp',
		     'PlayerServiceImpl.cpp',
		     'Util.cpp',
		     protobuf_files],
	   link_args: ['-lstdc++fs', '-lpthread'],
	   dependencies: [mpv_dep, grpc_dep, protobuf_dep, spdlog_dep,
			  openssl_dep])


# Tests
gtest = meson.get_compiler('cpp').find_library('gtest', required: false)
if gtest.found()
  test_all = executable('test_all',
			sources: ['test/main.cpp',
				  'test/test-BigNum.cpp',
				  'test/test-certs.cpp',
				  'test/test-KeyPair.cpp',
				  'test/test-CryptoUtil.cpp',
				  'Util.cpp',
				  crypto_sources],
			link_args: ['-lpthread'],
			dependencies: [gtest, openssl_dep,
				       boost_dep, spdlog_dep])

  test('all', test_all)
  test('repeat', test_all,
       args: ['--gtest_repeat=1000',
	      '--gtest_filter=REPEAT_*'])
endif
# cppcheck = find_program('cppcheck', required: false)
# if cppcheck.found()
#   test('cppcheck', cppcheck, args: ['--project=compile_commands.json',
# 				    '--error-exitcode=1',
# 				    '--enable=all',
# 				    '--quiet'])
# endif

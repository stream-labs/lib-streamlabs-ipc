cmake_minimum_required(VERSION 3.5)
project(lib-streamlabs-ipc-sample-nodejs)

ADD_SUBDIRECTORY("../../" "${PROJECT_BUILD_DIR}lib-streamlabs-ipc")
ADD_SUBDIRECTORY(streamlabs-ipc-node)
ADD_SUBDIRECTORY(streamlabs-ipc-node-server)

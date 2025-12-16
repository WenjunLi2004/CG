// Project-local forwarder for system GLAD header
// This file helps IDEs resolve includes without altering build behavior.
#pragma once

#if defined(__has_include)
#  if __has_include_next(<glad/glad.h>)
#    include_next <glad/glad.h>
#  else
// Minimal fallback for Apple environments lacking include_next discovery.
// Provide core GL includes and GLAD loader declarations to avoid editor errors.
#    ifdef __APPLE__
#      include <OpenGL/gl3.h>
#    else
#      include <GL/gl.h>
#    endif
typedef void* (*GLADloadproc)(const char* name);
int gladLoadGLLoader(GLADloadproc load);
#  endif
#else
#  include_next <glad/glad.h>
#endif
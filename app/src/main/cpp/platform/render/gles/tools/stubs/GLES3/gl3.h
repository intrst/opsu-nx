// Minimal stub of <GLES3/gl3.h> for desktop `g++ -fsyntax-only` checks ONLY.
#pragma once
#include <cstddef>
typedef unsigned int GLenum; typedef unsigned int GLuint; typedef int GLint; typedef int GLsizei; typedef unsigned char GLboolean;
typedef float GLfloat; typedef ptrdiff_t GLsizeiptr; typedef char GLchar; typedef unsigned int GLbitfield;
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_BLEND 0x0BE2
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_DITHER 0x0BD0
#define GL_SCISSOR_TEST 0x0C11
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_ARRAY_BUFFER 0x8892
#define GL_STREAM_DRAW 0x88E0
#define GL_TRIANGLES 0x0004
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
GLenum glGetError(); const unsigned char* glGetString(GLenum);
void glEnable(GLenum); void glDisable(GLenum); void glBlendFunc(GLenum, GLenum); void glColorMask(GLboolean, GLboolean, GLboolean, GLboolean);
void glViewport(GLint, GLint, GLsizei, GLsizei); void glScissor(GLint, GLint, GLsizei, GLsizei);
void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat); void glClear(GLbitfield); void glPixelStorei(GLenum, GLint); void glGetIntegerv(GLenum, GLint*);
void glGenTextures(GLsizei, GLuint*); void glDeleteTextures(GLsizei, const GLuint*); void glBindTexture(GLenum, GLuint); void glActiveTexture(GLenum);
void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
void glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
void glTexParameteri(GLenum, GLenum, GLint);
void glGenBuffers(GLsizei, GLuint*); void glBindBuffer(GLenum, GLuint); void glBufferData(GLenum, GLsizeiptr, const void*, GLenum);
void glBufferSubData(GLenum, GLsizeiptr, GLsizeiptr, const void*);
void glGenVertexArrays(GLsizei, GLuint*); void glBindVertexArray(GLuint); void glEnableVertexAttribArray(GLuint);
void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
GLuint glCreateShader(GLenum); void glShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*); void glCompileShader(GLuint);
void glGetShaderiv(GLuint, GLenum, GLint*); void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar*); void glDeleteShader(GLuint);
GLuint glCreateProgram(); void glAttachShader(GLuint, GLuint); void glLinkProgram(GLuint); void glGetProgramiv(GLuint, GLenum, GLint*);
void glGetProgramInfoLog(GLuint, GLsizei, GLsizei*, GLchar*); void glDeleteProgram(GLuint); void glUseProgram(GLuint);
GLint glGetUniformLocation(GLuint, const GLchar*); void glUniform1i(GLint, GLint); void glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
void glDrawArrays(GLenum, GLint, GLsizei);
void glGenFramebuffers(GLsizei, GLuint*); void glDeleteFramebuffers(GLsizei, const GLuint*); void glBindFramebuffer(GLenum, GLuint);
void glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint); GLenum glCheckFramebufferStatus(GLenum);

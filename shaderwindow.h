#ifndef SHADERWINDOW_H
#define SHADERWINDOW_H

#include <QOpenGLShaderProgram>
#include "openglwindow.h"

class ShaderWindow : public OpenGLWindow
{
public:
  ShaderWindow();
  ~ShaderWindow();

  void initialize() Q_DECL_OVERRIDE;
  void render() Q_DECL_OVERRIDE;
  void update() Q_DECL_OVERRIDE;

private:
  GLuint loadShader(GLenum type, const char *source);

  GLuint m_vtxPosAttr;
  GLuint m_vtxUVAttr;

  //GLuint m_matrixUniform;

  GLuint m_resXUniform, m_resYUniform;
  GLuint m_aspectUniform;
  GLuint m_timeUniform;

  GLuint m_center;
  GLuint m_zoom;
  GLuint m_c;

  QOpenGLShaderProgram *m_program;
  int m_frame;
};

#endif // SHADERWINDOW_H

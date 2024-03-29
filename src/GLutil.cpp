/* Copyright (C) 2015 Coos Baakman

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/


#include "GLutil.h"

#include "err.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

void GLErrorString(char *out, GLenum status)
{
    switch (status)
    {
    case GL_INVALID_ENUM:
        strcpy (out, "GL_INVALID_ENUM");
        break;
    case GL_INVALID_VALUE:
        strcpy (out, "GL_INVALID_VALUE");
        break;
    case GL_INVALID_OPERATION:
        strcpy (out, "GL_INVALID_OPERATION");
        break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        strcpy (out, "GL_INVALID_FRAMEBUFFER_OPERATION");
        break;
    case GL_OUT_OF_MEMORY:
        strcpy (out, "GL_OUT_OF_MEMORY");
        break;
    case GL_STACK_UNDERFLOW:
        strcpy (out, "GL_STACK_UNDERFLOW");
        break;
    case GL_STACK_OVERFLOW:
        strcpy (out, "GL_STACK_OVERFLOW");
        break;
    default:
        sprintf (out, "unknown error 0x%.8X", status);
        break;
    }
}
void GLFrameBufferErrorString (char *out, GLenum status)
{
    switch (status)
    {
    case GL_FRAMEBUFFER_UNDEFINED:
        strcpy (out, "GL_FRAMEBUFFER_UNDEFINED");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        strcpy (out, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        strcpy (out, "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        strcpy (out, "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        strcpy (out, "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER");
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
        strcpy (out, "GL_FRAMEBUFFER_UNSUPPORTED");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        strcpy (out, "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        strcpy (out, "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS");
        break;
    default:
        sprintf (out, "unknown error 0x%.8X", status);
        break;
    }
}

void RenderCube (const vec3 &p, const float sz)
{
    GLfloat size = sz / 2;

    glBegin (GL_QUADS);

    glNormal3f (0.0f, 0.0f, 1.0f);
    glVertex3f (p.x - size, p.y + size, p.z - size);
    glVertex3f (p.x + size, p.y + size, p.z - size);
    glVertex3f (p.x + size, p.y - size, p.z - size);
    glVertex3f (p.x - size, p.y - size, p.z - size);

    glNormal3f (0.0f, 0.0f, -1.0f);
    glVertex3f (p.x + size, p.y + size, p.z + size);
    glVertex3f (p.x - size, p.y + size, p.z + size);
    glVertex3f (p.x - size, p.y - size, p.z + size);
    glVertex3f (p.x + size, p.y - size, p.z + size);

    glNormal3f (1.0f, 0.0f, 0.0f);
    glVertex3f (p.x + size, p.y + size, p.z - size);
    glVertex3f (p.x + size, p.y + size, p.z + size);
    glVertex3f (p.x + size, p.y - size, p.z + size);
    glVertex3f (p.x + size, p.y - size, p.z - size);

    glNormal3f (-1.0f, 0.0f, 0.0f);
    glVertex3f (p.x - size, p.y + size, p.z + size);
    glVertex3f (p.x - size, p.y + size, p.z - size);
    glVertex3f (p.x - size, p.y - size, p.z - size);
    glVertex3f (p.x - size, p.y - size, p.z + size);

    glNormal3f (0.0f, 1.0f, 0.0f);
    glVertex3f (p.x - size, p.y + size, p.z + size);
    glVertex3f (p.x + size, p.y + size, p.z + size);
    glVertex3f (p.x + size, p.y + size, p.z - size);
    glVertex3f (p.x - size, p.y + size, p.z - size);

    glNormal3f (0.0f, -1.0f, 0.0f);
    glVertex3f (p.x - size, p.y - size, p.z - size);
    glVertex3f (p.x + size, p.y - size, p.z - size);
    glVertex3f (p.x + size, p.y - size, p.z + size);
    glVertex3f (p.x - size, p.y - size, p.z + size);

    glEnd();
}
// Make these inline so tha console applications can also include util.cpp
void glColorHSV (float h, float s, float v)
{
     // H [0, 360] S and V [0.0, 1.0].
     int i = (int)floor(h/60.0f) % 6;
     float f = h/60.0f - floor(h/60.0f);
     float p = v * (float)(1 - s);
     float q = v * (float)(1 - s * f);
     float t = v * (float)(1 - (1 - f) * s);

     switch (i) {
         case 0: glColor3f(v, t, p);
         break;
         case 1: glColor3f(q, v, p);
         break;
         case 2: glColor3f(p, v, t);
         break;
         case 3: glColor3f(p, q, v);
         break;
         case 4: glColor3f(t, p, v);
         break;
         case 5: glColor3f(v, p, q);
    }
}
bool CheckGLOK (const char *doing)
{
    GLenum status = glGetError();
    if (status == GL_NO_ERROR)
        return true;

    char errorString [260];
    GLErrorString (errorString, status);

    SetError ("%s was encountered during %s", errorString, doing);

    return false;
}

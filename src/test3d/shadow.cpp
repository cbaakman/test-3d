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


#include "shadow.h"
#include "app.h"
#include <math.h>
#include <cstring>
#include <string>
#include "../matrix.h"
#include <stdio.h>
#include "../io.h"
#include "../GLutil.h"
#include "../util.h"
#include "../xml.h"
#include "../random.h"
#include "../err.h"
#include "../shader.h"

#define VIEW_ANGLE 45.0f
#define NEAR_VIEW 0.1f
#define FAR_VIEW 1000.0f

#define BOX_WIDTH 20.0f
#define BOX_HEIGHT 6.0f
#define BOX_LENGTH 20.0f

#define PLAYER_RADIUS 1.2f

#define PLAYER_START vec3 (0.1f,2.0f,-5.0f)

#define PARTICLE_RADIUS 10.0f
#define PARTICLE_MIN_Y -3.0f
#define PARTICLE_MAX_Y 3.0f
#define PARTICLE_INTERVAL_MIN 0.2f
#define PARTICLE_INTERVAL_MAX 0.4f
#define PARTICLE_MAXSPEED 5.0f

const char

normal_vsh [] = R"shader(

    varying vec3 b, t, n, v;

    attribute vec3 tangent,
                   bitangent;

    void main()
    {
        v = vec3 (gl_ModelViewMatrix * gl_Vertex);

        n = normalize (gl_NormalMatrix * gl_Normal);
        t = normalize (gl_NormalMatrix * tangent);
        b = normalize (gl_NormalMatrix * bitangent);

        gl_Position = ftransform();
        gl_TexCoord[0] = gl_MultiTexCoord0;
    }

)shader",

normal_fsh [] = R"shader(

    uniform sampler2D tex_color,
                      tex_normal;

    varying vec3 b, t, n, v;

    void main()
    {
        vec4 normal_color = texture2D (tex_normal, gl_TexCoord[0].st);

        vec3 L = normalize (gl_LightSource[0].position.xyz - v),

                 texNormal = vec3 (2 * (normal_color.r - 0.5),
                                   2 * (normal_color.g - 0.5),
                                   normal_color.b);

        texNormal = normalize (texNormal.z * normalize (n) +
                               texNormal.x * normalize (t) +
                               texNormal.y * normalize (b));

        float lum = min (1.0, 0.3 + clamp (dot (texNormal, L), 0.0, 1.0));

        gl_FragColor = texture2D (tex_color, gl_TexCoord[0].st);
        gl_FragColor.rgb *= lum;
    }

)shader";

ShadowScene::ShadowScene(App *pApp) : Scene(pApp),
    frame (0),
    shadowTriangles (NULL),
    angleX (0.3f),
    angleY (0.0f),
    distCamera (7.0f),
    posPlayer (PLAYER_START),
    rotationPlayer (QUAT_ID),
    vy (0),
    onGround (false), touchDown (false),
    show_bones (false), show_normals (false), show_triangles (false),
    pMeshDummy (NULL),
    shader_normal(0)
{
    texDummyNormal.tex = texDummy.tex =
    texBox.tex = texSky.tex = texPar.tex = 0;

    Zero (&vbo_sky, sizeof (VertexBuffer));
    Zero (&vbo_box, sizeof (VertexBuffer));
    Zero (&vbo_dummy, sizeof (VertexBuffer));

    // colliders [0] = new FeetCollider (vec3 (0, -2.0f, 0));

    /*
        Decided not to use the feet collider here. it's too buggy for onGround tests.
     */
    colliders.push_back (new SphereCollider (vec3 (0, -1.999f, 0), 0.001f));
    colliders.push_back (new SphereCollider (VEC_O, PLAYER_RADIUS));

    // Place particles at random positions with random speeds and sizes
    for (int i = 0; i < N_PARTICLES; i++)
    {
        float a = RandomFloat (0, M_PI * 2),
              r = RandomFloat (0, PARTICLE_RADIUS),
              h = RandomFloat (PARTICLE_MIN_Y, PARTICLE_MAX_Y),
              s = sqrt (PARTICLE_MAXSPEED) / 3;

        particles [i].pos = vec3 (r * cos (a), h, r * sin (a));
        particles [i].vel = vec3 (RandomFloat (-s, s),
                                  RandomFloat (-s, s),
                                  RandomFloat (-s, s));
        particles [i].size = RandomFloat (0.2f, 0.6f);
    }
}
ShadowScene::~ShadowScene()
{
    glDeleteBuffer (&vbo_box);
    glDeleteBuffer (&vbo_sky);

    glDeleteTextures(1, &texDummyNormal.tex);
    glDeleteTextures(1, &texDummy.tex);
    glDeleteTextures(1, &texBox.tex);
    glDeleteTextures(1, &texSky.tex);
    glDeleteTextures(1, &texPar.tex);

    glDeleteProgram (shader_normal);

    delete pMeshDummy;
    delete [] shadowTriangles;

    for (ColliderP pCollider : colliders)
        delete pCollider;
}
void GetTangentBitangent (const int n_vertices,
                          const MeshVertex **p_vertices,
                          const MeshTexel *texels,
                          const int i,

                          vec3 &t, vec3 &b)
{

    /*
      Calculate tangent and bitangent from neighbouring
      vertices (positions and texture coords)
     */

    int j = (n_vertices + i - 1) % n_vertices,
        k = (i + 1) % n_vertices;

    float du0, dv0, du1, dv1, luv0, luv1;
    vec3 v0, v1;

    v0 = (p_vertices [j]->p - p_vertices [i]->p).Unit ();
    du0 = texels [j].u - texels [i].u;
    dv0 = texels [j].v - texels [i].v;
    luv0 = sqrt (sqr (du0) + sqr (dv0));

    v1 = (p_vertices [k]->p - p_vertices [i]->p).Unit ();
    du1 = texels [k].u - texels [i].u;
    dv1 = texels [k].v - texels [i].v;
    luv1 = sqrt (sqr (du1) + sqr (dv1));

    /*
        NOTE: There appears to be a precision error somewhere.
              Theoretically, all tangents and bitangents should
              be length 1.0 in the following 2 formulae, but
              they aren't!
     */

    // Use 'Unit' to overcome normalization errors:
    t = (luv0 * v0 * dv1 - luv1 * v1 * dv0).Unit ();
    b = (luv1 * v1 * du0 - luv0 * v0 * du1).Unit ();
}
struct NormalMapVertex
{
    vec3 p,
         n, t, b; // normal, tangent, bitangent

    MeshTexel tx;
};
void UpdateNormalMapVertices (VertexBuffer *pBuffer, const MeshState *pMesh, const std::string &subset_id)
{
    size_t i = 0,
           m;

    const MeshData *pData = pMesh->GetMeshData ();
    const int triangles [][3] = {{0, 1, 2}, {0, 2, 3}};

    /*
        We must add 3 vertices to the buffer for every triangle we render. Because
        our vertex format contains texture coordinates, which are unique per mesh face,
        not per mesh vertex.
     */

    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);

    NormalMapVertex* pbuf = (NormalMapVertex *)glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!pbuf)
    {
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        SetError ("failed to obtain buffer pointer");
        return;
    }

    pMesh->ThroughSubsetFaces (subset_id,
        [&] (const int n_vertices, const MeshVertex **p_vertices, const MeshTexel *texels)
        {
            m = 0;
            if (n_vertices >= 3)
                m++;
            if (n_vertices >= 4)
                m++;

            for (int u = 0; u < m; u++)
            {
                for (int j : triangles [u])
                {
                    pbuf [i].p = p_vertices [j]->p;
                    pbuf [i].n = p_vertices [j]->n;

                    GetTangentBitangent (n_vertices, p_vertices, texels, j,
                                         pbuf [i].t, pbuf [i].b);

                    pbuf [i].tx = texels [j];
                    i ++;
                }
            }
        }
    );

    glUnmapBuffer (GL_ARRAY_BUFFER);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
void RenderNormalMapVertices (const VertexBuffer *pBuffer, const int index_tangent,
                                                           const int index_bitangent)
{
    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);

    // Match the vertex format: position [3], normal [3], tangent [3], bitangent [3], texcoord [2]
    glEnableClientState (GL_VERTEX_ARRAY);
    glVertexPointer (3, GL_FLOAT, sizeof (NormalMapVertex), 0);

    glEnableClientState (GL_NORMAL_ARRAY);
    glNormalPointer (GL_FLOAT, sizeof (NormalMapVertex), (const GLvoid *)(sizeof (vec3)));

    glEnableVertexAttribArray (index_tangent);
    glVertexAttribPointer (index_tangent, 3, GL_FLOAT, GL_FALSE, sizeof (NormalMapVertex), (const GLvoid *)(2 * sizeof (vec3)));

    glEnableVertexAttribArray (index_bitangent);
    glVertexAttribPointer (index_bitangent, 3, GL_FLOAT, GL_FALSE, sizeof (NormalMapVertex), (const GLvoid *)(3 * sizeof (vec3)));

    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer (2, GL_FLOAT, sizeof (NormalMapVertex), (const GLvoid *)(4 * sizeof (vec3)));

    glDrawArrays (GL_TRIANGLES, 0, pBuffer->n_vertices);

    glDisableVertexAttribArray (index_tangent);
    glDisableVertexAttribArray (index_bitangent);
    glDisableClientState (GL_VERTEX_ARRAY);
    glDisableClientState (GL_NORMAL_ARRAY);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
struct TexturedVertex
{
    MeshVertex v;
    MeshTexel t;
};
bool SetTexturedVertices (VertexBuffer *pBuffer, const MeshData *pData, const std::string &subset_id)
{

    size_t i = 0,
           m;

    const int triangles [][3] = {{0, 1, 2}, {0, 2, 3}};

    /*
        We must add 3 vertices to the buffer for every triangle we render. Because
        our vertex format contains texture coordinates, which are unique per mesh face,
        not per mesh vertex.
     */

    pBuffer->n_vertices = 3 * (pData->quads.size () * 2 + pData->triangles.size ());


    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);
    glBufferData (GL_ARRAY_BUFFER, sizeof (TexturedVertex) * pBuffer->n_vertices, NULL, GL_STATIC_DRAW);

    TexturedVertex* pbuf = (TexturedVertex *)glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!pbuf)
    {
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        SetError ("failed to obtain buffer pointer");
        return false;
    }

    ThroughSubsetFaces (pData, subset_id,
        [&](const int n_vertex, const MeshVertex **p_vertices, const MeshTexel *texels)
        {
            m = 0;
            if (n_vertex >= 3)
                m++;
            if (n_vertex >= 4)
                m++;

            for (int u = 0; u < m; u++)
            {
                for (int j : triangles [u])
                {
                    pbuf [i].v = *(p_vertices [j]);
                    pbuf [i].t = texels [j];
                    i ++;
                }
            }
        }
    );

    glUnmapBuffer (GL_ARRAY_BUFFER);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    return true;
}
void RenderTexturedVertices (const VertexBuffer *pBuffer)
{
    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);

    // Match the vertex format: position [3], normal [3], texcoord [2]
    glEnableClientState (GL_VERTEX_ARRAY);
    glVertexPointer (3, GL_FLOAT, sizeof (TexturedVertex), 0);

    glEnableClientState (GL_NORMAL_ARRAY);
    glNormalPointer (GL_FLOAT, sizeof (TexturedVertex), (const GLvoid *)(sizeof (vec3)));

    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer (2, GL_FLOAT, sizeof (TexturedVertex), (const GLvoid *)(2 * sizeof (vec3)));

    glDrawArrays (GL_TRIANGLES, 0, pBuffer->n_vertices);

    glDisableClientState (GL_VERTEX_ARRAY);
    glDisableClientState (GL_NORMAL_ARRAY);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
void ShadowScene::AddAll (Loader *pLoader)
{
    /*
        Load and parse all resources.
        On failure, set the error message and return false.
     */

    // Load dummy texture:
    pLoader->Add (LoadPNGFunc (zipPath, "dummy.png", &texDummy));

    // Load dummy normal texture:
    pLoader->Add (LoadPNGFunc (zipPath, "dummy_n.png", &texDummyNormal));

    // Load dummy mesh as xml document:
    pLoader->Add (
        [zipPath, this] ()
        {
            if (!LoadMesh (zipPath, "dummy.xml", &meshDataDummy))
                return false;

            pMeshDummy = new MeshState (&meshDataDummy);

            /*
                The dummy mesh is animated and will change shape every frame. However, the number of
                vertices doesn't change, so we can allocate a vertex buffer here already.

                It will be refilled every frame.
             */

            glGenBuffer (&vbo_dummy);
            vbo_dummy.n_vertices = 3 * (meshDataDummy.quads.size () * 2 + meshDataDummy.triangles.size ());
            glBindBuffer (GL_ARRAY_BUFFER, vbo_dummy.handle);
            glBufferData (GL_ARRAY_BUFFER, sizeof (NormalMapVertex) * vbo_dummy.n_vertices, NULL, GL_DYNAMIC_DRAW);
            glBindBuffer (GL_ARRAY_BUFFER, 0);

            // Derive shadow objects from dummy mesh:
            shadowTriangles = new STriangle [meshDataDummy.triangles.size() + 2 * meshDataDummy.quads.size()];

            return true;
        }
    );

    // Load environment texture:
    pLoader->Add (LoadPNGFunc (zipPath, "box.png", &texBox));

    // Load environment mesh as xml:
    pLoader->Add (
        [zipPath, this] ()
        {
            if (!LoadMesh (zipPath, "box.xml", &meshDataBox))
                return false;

            /*
                This mesh is not animated, so we only need to fill the vertex buffer once
                and render from it every frame.
             */

            glGenBuffer (&vbo_box);
            if (!SetTexturedVertices (&vbo_box, &meshDataBox, "0"))
            {
                SetError ("error filling vertex buffer for box");
                return false;
            }

            // Derive collision objects from mesh:
            ToTriangles (&meshDataBox, collision_triangles);

            // Put the player on the ground we just generated:
            posPlayer = CollisionMove (posPlayer, vec3 (posPlayer.x, -1000.0f, posPlayer.z),
                                       colliders, collision_triangles);

            return true;
        }
    );

    // Load skybox texture:
    pLoader->Add (LoadPNGFunc (zipPath, "sky.png", &texSky));

    // Load skybox mesh as xml:
    pLoader->Add (
        [zipPath, this] ()
        {
            if (!LoadMesh (zipPath, "sky.xml", &meshDataSky))
                return false;

            /*
                This mesh is not animated, so we only need to fill the vertex buffer once
                and render from it every frame.
             */

            glGenBuffer (&vbo_sky);
            if (!SetTexturedVertices (&vbo_sky, &meshDataSky, "0"))
            {
                SetError ("error filling vertex buffer for box");
                return false;
            }

            return true;
        }
    );

    // Load texture for the particles:
    pLoader->Add (LoadPNGFunc (zipPath, "particle.png", &texPar));

    // Create shader object for player:
    pLoader->Add (
        [this] ()
        {
            shader_normal = CreateShaderProgram (GL_VERTEX_SHADER, normal_vsh,
                                                 GL_FRAGMENT_SHADER, normal_fsh);

            if (!shader_normal)
                return false;

            // Pick tangent and bitangent index:
            GLint max_attribs;
            glGetIntegerv (GL_MAX_VERTEX_ATTRIBS, &max_attribs);

            index_tangent = max_attribs - 2;
            index_bitangent = max_attribs - 1;

            /*
                Tell the shaders which attribute indices represent
                tangen and bitangent:
             */
            glBindAttribLocation (shader_normal, index_tangent, "tangent");
            if (!CheckGLOK ("setting tangent attrib location"))
                return false;

            glBindAttribLocation (shader_normal, index_bitangent, "bitangent");
            if (!CheckGLOK ("setting bitangent attrib location"))
                return false;

            return true;
        }
    );
}

void getCameraPosition( const vec3 &center,
        const GLfloat angleX, const GLfloat angleY, const GLfloat dist,
    vec3 &posCamera)
{
    // Place camera at chosen distance and angle
    posCamera = center + matRotY (angleY) * matRotX (angleX) * vec3 (0, 0, -dist);
}
#define PLAYER_ROTATEPERSEC 2*PI
void ShadowScene::Update (const float dt)
{
    vec3 posCamera,
         targetDirectionPlayer = VEC_O;
    Quaternion targetRotationPlayer = rotationPlayer;
    bool forward = false;

    // Detect movement input:
    const Uint8 *state = SDL_GetKeyboardState (NULL);

    if (state[SDL_SCANCODE_A] || state[SDL_SCANCODE_D]
        || state[SDL_SCANCODE_W] || state[SDL_SCANCODE_S])
    {
        getCameraPosition (posPlayer, angleX, angleY, distCamera, posCamera);

        // Move dummy, according to camera view and buttons pressed
        GLfloat mx, mz;

        if (state [SDL_SCANCODE_W])
            mz = 1.0f;
        else if (state [SDL_SCANCODE_S])
            mz = -1.0f;
        else
            mz = 0.0f;

        if (state [SDL_SCANCODE_A])
            mx = -1.0f;
        else if (state [SDL_SCANCODE_D])
            mx = 1.0f;
        else
            mx = 0.0f;

        // Define facing direction's X and Z axis, based on the camera direction:
        vec3 camZ = posPlayer - posCamera;
        camZ.y = 0;
        camZ = camZ.Unit ();
        vec3 camX = Cross (VEC_UP, camZ);

        targetDirectionPlayer = (mx * camX + mz * camZ).Unit ();
        targetRotationPlayer = Rotation (VEC_FORWARD, mx * camX + mz * camZ);

        forward = true;
    }
    UpdatePlayerMovement (dt, targetDirectionPlayer);

    UpdatePlayerAnimation (dt, targetRotationPlayer, forward);

    // Update the new animation pose to the vertex buffer:
    UpdateNormalMapVertices (&vbo_dummy, pMeshDummy, "0");

    // Move the particles, according to time passed.
    for (int i = 0; i < N_PARTICLES; i++)
    {
        particles [i].vel += -0.03f * particles [i].pos * dt;
        particles [i].pos += particles [i].vel * dt;
    }
}
void ShadowScene::UpdatePlayerMovement (const float dt, const vec3 &movementDirUnit)
{
    vec3 newPosPlayer = posPlayer + 5 * movementDirUnit * dt;

    // if falling, see if player hit the ground
    onGround = (vy <= 0.0f) && TestOnGround (posPlayer, colliders, collision_triangles);

    if (onGround)
        vy = 0.0f;
    else
        vy -= 30.0f * dt;

    newPosPlayer.y += vy * dt;

    /*
        If on the ground, the collision mechanics are a little different.
        We try to keep the feet on the ground. Unless the fall is too long
     */
    if (onGround)
        posPlayer = CollisionWalk (posPlayer, newPosPlayer, colliders, collision_triangles);
    else
        posPlayer = CollisionMove (posPlayer, newPosPlayer, colliders, collision_triangles);
}
void ShadowScene::UpdatePlayerAnimation (const float dt, const Quaternion &targetRotationPlayer, bool forward)
{
    // Rotate 3D mesh over time, to facing direction:
    float angle = Angle (rotationPlayer, targetRotationPlayer),
          fracRotate = PLAYER_ROTATEPERSEC * dt / angle;
    if (fracRotate > 1.0f || angle <= 0.0)
        fracRotate = 1.0f;

    rotationPlayer = Slerp (rotationPlayer, targetRotationPlayer, fracRotate);

    // animate the mesh:
    if (onGround && forward) // walking / running animation
    {
        frame += 15 * dt;
        pMeshDummy->SetAnimationState ("run", frame);
        touchDown = false;
    }
    else if (vy > 0) // going up animation
    {
        frame = 1.0f;
        pMeshDummy->SetAnimationState ("jump", frame);
    }
    else if (!onGround) // falling animation
    {
        frame += 30 * dt;
        if (frame > 10.0f)
            frame = 10.0f;

        pMeshDummy->SetAnimationState ("jump", frame);
    }
    else // onGround and not moving forward
    {
        frame += 15 * dt;
        if (frame > 20.0f)
            touchDown = false;

        if (touchDown) // touchdown animation

            pMeshDummy->SetAnimationState ("jump", frame);

        else // stationary animation

            pMeshDummy->SetAnimationState (NULL, 0);
    }

    if (!onGround)
        touchDown = true; //must play this animation again when landing
}
const GLfloat ambient [] = {0.3f, 0.3f, 0.3f, 1.0f},
              diffuse [] = {1.0f, 1.0f, 1.0f, 1.0f};

const vec3 posLight (0.0f, 10.0f, 0.0f);

/*
 * Extract STriangle objects from a mesh to compute drop shadows from.
 */
int GetTriangles (const MeshState *pMesh, STriangle *triangles)
{
    const int triangle_indices [][3] = {{0, 1, 2}, {0, 2, 3}};

    int i = 0,
        m;
    pMesh->ThroughFaces (
        [&](const int n_vertex, const MeshVertex **p_vertices, const MeshTexel *texels)
        {
            m = 0;
            if (n_vertex >= 3)
                m++;
            if (n_vertex >= 4)
                m++;

            /*
                Splitting up quads into triangles might not be exactly right,
                unless the quads are completely flat of coarse.
             */

            for (int u = 0; u < m; u++)
            {
                for (int j = 0; j < 3; j++)
                {
                    triangles[i].p [j] = &p_vertices [triangle_indices [u][j]]->p;
                }
                i ++;
            }
        }
    );

    return i;
}

/*
 * Determines which triangles are pointing towards eye and sets their visibility flag.
 */
void SetVisibilities(const vec3 &posEye, const int n_triangles, STriangle *triangles)
{
    for(int i = 0; i < n_triangles; i++)
    {
        STriangle* t = &triangles[i];

        /*
           Calculate normal from triangle,
           assume clockwise is front.
         */

        vec3 n = Cross ((*t->p[1] - *t->p[0]), (*t->p[2] - *t->p[0])).Unit();

        float d = -Dot (*t->p[0], n), // plane's distance from origin

              side = Dot(n, posEye) + d;

        // If the normal points towards the eye, then the triangle is visible.

        t->visible = (side > 0);
    }
}

struct Edge { const vec3 *p[2]; };

/**
 * Determines which triangle Edges should make up the shadow.
 * Precondition is that the 'visible' fields have been set on the triangles beforehand !!
 */
void GetShadowEdges(const int n_triangles, const STriangle *triangles, std::list<Edge> &result)
{

    // We're looking for edges that are not between two visible triangles,
    // but ARE part of a visible triangle

    /*
       one bool per edge of the triangle with points (0, 1, 2),
       0 between triangle point 0 and 1,
       1 between 1 and 2
       and 2 between 2 and 0
    */
    bool use_edge [3];

    int i, j, x, y, x2, y2;
    for (i = 0; i < n_triangles; i++)
    {
        // no need to check the edges of invisible triangles:
        if (!triangles[i].visible)
            continue;

        /*
            Include all edges by default.
            Iteration must rule out edges that are shared by two visible triangles.
         */
        for (x = 0; x < 3; x++)
            use_edge [x] = true;

        /*
            Iterate to find visible triangles (j)
            that share edges with visible triangles. (i)
         */
        for (j = 0; j < n_triangles; j++)
        {
            /*
               Make sure not to compare triangles with themselves
               and be sure to skip other invisible triangles.
            */

            if (i == j || !triangles [j].visible)
                continue;

            // compare the three edges of both triangles with each other:

            for (x = 0; x < 3; x++) // iterate over the edges of triangle i
            {
                x2 = (x + 1) % 3; // second point of edge x

                for (y = 0; y < 3; y++) // iterate over the edges of triangle j
                {
                    y2 = (y + 1) % 3; // second point of edge y

                    if (triangles[i].p[x] == triangles[j].p[y] && triangles[i].p[x2] == triangles[j].p[y2] ||
                        triangles[i].p[x2] == triangles[j].p[y] && triangles[i].p[x] == triangles[j].p[y2])
                    {
                        // edge x on triangle i is equal to edge y on triangle j

                        use_edge [x] = false;
                        break;
                    }
                }
            }

            /*
               If all three edges (0, 1, 2) of triangle (i) were found shared with other triangles,
               then search no more other triangles (j) for this particular triangle (i).
            */
            if (!use_edge [0] && !use_edge [1] && !use_edge [2])
                break;
        }

        // Add the edges (x) of triangle (i) that were found needed:
        for (x = 0; x < 3; x++)
        {
            if (use_edge [x])
            {
                x2 = (x + 1) % 3; // second point of edge x

                Edge edge;
                edge.p [0] = triangles [i].p [x];
                edge.p [1] = triangles [i].p [x2];
                result.push_back (edge);
            }
        }
    }
}

/**
 * Renders squares from every edge to infinity, using the light position as origin.
 */
#define SHADOW_INFINITY 1000.0f
void ShadowPass(const std::list<Edge> &edges, const vec3 &posLight)
{
    for (std::list<Edge>::const_iterator it = edges.begin(); it != edges.end(); it++)
    {
        const Edge *pEdge = &(*it);

        // These are the four vertices of the shadow quad:
        vec3 d1 = (*pEdge->p[0] - posLight).Unit(),
             d2 = (*pEdge->p[1] - posLight).Unit(),

             v0 = *pEdge->p[0] + d1 * SHADOW_INFINITY,
             v1 = *pEdge->p[1] + d2 * SHADOW_INFINITY;

        glBegin(GL_QUADS);
        glVertex3f(pEdge->p[1]->x, pEdge->p[1]->y, pEdge->p[1]->z);
        glVertex3f(pEdge->p[0]->x, pEdge->p[0]->y, pEdge->p[0]->z);
        glVertex3f(v0.x, v0.y, v0.z);
        glVertex3f(v1.x, v1.y, v1.z);
        glEnd();
    }
}
void RenderBones (const MeshState *pMesh)
{
    const MeshData *pData = pMesh->GetMeshData ();

    for (auto it = pData->bones.begin(); it != pData->bones.end(); it++)
    {
        // Draw a line from head to tail pos in the bone's current state

        const std::string id = it->first;
        const MeshBoneState *pState = pMesh->GetBoneState (id);

        const vec3 *pHead = &pState->posHead,
                   *pTail = &pState->posTail;

        glBegin (GL_LINES);
        glVertex3f (pHead->x, pHead->y, pHead->z);
        glVertex3f (pTail->x, pTail->y, pTail->z);
        glEnd ();
    }
}
void RenderSprite (const vec3 &pos, const Texture *pTex,
                   const float tx1, const float ty1, const float tx2, const float ty2, // texture coordinates (in pixels)
                   const float size = 1.0f)
{
    matrix4 modelView;
    glGetFloatv (GL_MODELVIEW_MATRIX, modelView.m);

    /*
       Extract the axes system from the current modelview matrix and normalize them,
       then render the quad on the normalized axes system, so that it's always pointed towards the camera,
       but it's zoom is still variable.
    */

    const float sz = size / 2;
    vec3 modelViewRight = vec3 (modelView.m11, modelView.m12, modelView.m13).Unit(),
         modelViewUp = vec3 (modelView.m21, modelView.m22, modelView.m23).Unit(),

         p1 = pos + sz * modelViewUp - sz * modelViewRight,
         p2 = pos + sz * modelViewUp + sz * modelViewRight,
         p3 = pos - sz * modelViewUp + sz * modelViewRight,
         p4 = pos - sz * modelViewUp - sz * modelViewRight;

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, pTex->tex);

    glBegin (GL_QUADS);
    glTexCoord2f (tx1 / pTex->w, ty1 / pTex->h);
    glVertex3f (p1.x, p1.y, p1.z);
    glTexCoord2f (tx2 / pTex->w, ty1 / pTex->h);
    glVertex3f (p2.x, p2.y, p2.z);
    glTexCoord2f (tx2 / pTex->w, ty2 / pTex->h);
    glVertex3f (p3.x, p3.y, p3.z);
    glTexCoord2f (tx1 / pTex->w, ty2 / pTex->h);
    glVertex3f (p4.x, p4.y, p4.z);
    glEnd ();
}

enum RenderMode {RENDER_FACE, RENDER_NBT};

#define NBT_SIZE 0.3f

/**
 * Renders Normal, Tangent and Bitangent for debugging
 */
void RenderNBT (const int n_vertices, const MeshVertex **p_vertices, const MeshTexel *texels)
{
    glBegin (GL_LINES);

    vec3 t, b, v0, v1;

    size_t i;

    for (i = 0; i < n_vertices; i++)
    {
        GetTangentBitangent (n_vertices, p_vertices, texels, i,
                             t, b);


        // pn, pt, pb are p, moved in the direction of the normal, tangent, and bitangent
        const vec3 p = p_vertices [i]->p,
                   pn = p + NBT_SIZE * p_vertices [i]->n,
                   pt = p + NBT_SIZE * t,
                   pb = p + NBT_SIZE * b;

        // Draw a line from p to pn:
        glColor4f (0.0f, 0.0f, 1.0f, 0.0f);
        glVertex3f (p.x, p.y, p.z);
        glVertex3f (pn.x, pn.y, pn.z);

        // Draw a line from p to pt:
        glColor4f (1.0f, 0.0f, 0.0f, 0.0f);
        glVertex3f (p.x, p.y, p.z);
        glVertex3f (pt.x, pt.y, pt.z);

        // Draw a line from p to pb:
        glColor4f (0.0f, 1.0f, 0.0f, 0.0f);
        glVertex3f (p.x, p.y, p.z);
        glVertex3f (pb.x, pb.y, pb.z);
    }

    glEnd ();
}
void RenderCollisionTriangles (const std::list <Triangle> &collision_triangles)
{
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_LIGHTING);
    glDisable (GL_TEXTURE_2D);
    glColor4f (0.8f, 0.0f, 0.0f, 1.0f);
    for (const Triangle &triangle : collision_triangles)
    {
        glBegin (GL_LINES);
        for (int j=0; j < 3; j++)
        {
            int k = (j + 1) % 3;
            glVertex3f (triangle.p[j].x, triangle.p[j].y, triangle.p[j].z);
            glVertex3f (triangle.p[k].x, triangle.p[k].y, triangle.p[k].z);
        }
        glEnd ();
    }

    glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
    glEnable (GL_LIGHTING);
    glEnable (GL_DEPTH_TEST);
}
void RenderQuadCoverScreen (const float distance)
{
    glBegin (GL_QUADS);
    glVertex3f (-1.0e+15f, 1.0e+15f, distance);
    glVertex3f ( 1.0e+15f, 1.0e+15f, distance);
    glVertex3f ( 1.0e+15f,-1.0e+15f, distance);
    glVertex3f (-1.0e+15f,-1.0e+15f, distance);
    glEnd ();
}
#define SHADOW_STENCIL_MASK 0xFFFFFFFFL
void ShadowScene::Render ()
{
    GLint texLoc;

    int w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);

    vec3 posCamera;
    getCameraPosition (posPlayer, angleX, angleY, distCamera, posCamera);

    // We want the camera to stay at some minimal distance from the walls:
    vec3 shift = 0.5f * (posCamera - posPlayer).Unit(),
         intersection;
    bool hit;
    Triangle tr;
    std::tie (hit, tr, intersection) = CollisionTraceBeam (posPlayer, posCamera + shift, collision_triangles);
    if (hit)
    {
        posCamera = intersection - shift;
    }

    // Set the 3d projection matrix:
    glMatrixMode(GL_PROJECTION);
    matrix4 matCamera = matPerspec(VIEW_ANGLE, (GLfloat) w / (GLfloat) h, NEAR_VIEW, FAR_VIEW);
    glLoadMatrixf(matCamera.m);

    // Set the model matrix to camera view:
    glMatrixMode(GL_MODELVIEW);
    const matrix4 matCameraView = matLookAt(posCamera, posPlayer, VEC_UP),
                  matSkyBox = matLookAt(VEC_O, posPlayer - posCamera, VEC_UP);

    glDepthMask (GL_TRUE);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc (GL_ALWAYS, 0, SHADOW_STENCIL_MASK);
    glClearDepth (1.0f);
    glClearColor (0, 0, 0, 1.0);
    glClearStencil (0);
    glClear (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glDisable (GL_STENCIL_TEST);

    glDisable (GL_BLEND);
    glColor4f (1.0f, 1.0f, 1.0f, 1.0f);

    glEnable (GL_CULL_FACE);
    glFrontFace (GL_CW);
    glCullFace (GL_BACK);

    glActiveTexture (GL_TEXTURE0);
    glEnable (GL_TEXTURE_2D);
    glDisable (GL_DEPTH_TEST);
    glDepthMask (GL_FALSE);

    // Render skybox around camera
    glLoadMatrixf(matSkyBox.m);

    glBindTexture (GL_TEXTURE_2D, texSky.tex);
    // Render two half spheres, one is mirror image
    glDisable (GL_CULL_FACE);
    RenderTexturedVertices (&vbo_sky);
    glScalef(1.0f, -1.0f, 1.0f);
    RenderTexturedVertices (&vbo_sky);

    glLoadMatrixf(matCameraView.m);

    // Light settings, position will be set later
    glEnable (GL_COLOR_MATERIAL);
    glEnable (GL_LIGHTING);
    glEnable (GL_LIGHT0);
    glLightfv (GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv (GL_LIGHT0, GL_DIFFUSE, diffuse);
    GLfloat vLightPos [4] = {posLight.x, posLight.y, posLight.z, 1.0f};// 0 is directional
    glLightfv (GL_LIGHT0, GL_POSITION, vLightPos);

    glEnable (GL_CULL_FACE);

    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glDepthFunc (GL_LEQUAL);

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render the world, shadows will be drawn on it later.
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texBox.tex);

    RenderTexturedVertices (&vbo_box);

    glDisable (GL_BLEND);
    glDisable (GL_TEXTURE_2D);

    if (show_triangles)
        RenderCollisionTriangles (collision_triangles);

    // Render player
    glEnable (GL_TEXTURE_2D);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texDummy.tex);
    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_2D, texDummyNormal.tex);

    matrix4 transformPlayer = matTranslation (posPlayer) * matQuat (rotationPlayer);

    glMultMatrixf (transformPlayer.m);


    glUseProgram (shader_normal);

    texLoc = glGetUniformLocation (shader_normal, "tex_color");
    glUniform1i (texLoc, 0);

    texLoc = glGetUniformLocation (shader_normal, "tex_normal");
    glUniform1i (texLoc, 1);

    // Send the vertex buffer contents to the shader:
    RenderNormalMapVertices (&vbo_dummy, index_tangent, index_bitangent);

    glUseProgram (0);
    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_2D, 0);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, 0);


    // Render bones (if switched on) and normals (if switched on)
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_LIGHTING);
    glDisable (GL_TEXTURE_2D);
    if (show_bones)
    {
        glColor4f(0.0f, 1.0f, 1.0f, 1.0f);
        RenderBones (pMeshDummy);
    }
    glEnable (GL_DEPTH_TEST);
    if (show_normals)
    {
        pMeshDummy->ThroughSubsetFaces ("0", RenderNBT);
    }

    // We need to have the light position in mesh space, because that's where the triangles are.

    const vec3 invPosLight = matInverse (transformPlayer) * posLight;
    const int n_triangles = GetTriangles (pMeshDummy, shadowTriangles);
    SetVisibilities (invPosLight, n_triangles, shadowTriangles);
    std::list<Edge> edges;
    GetShadowEdges (n_triangles, shadowTriangles, edges);

    glDisable (GL_LIGHTING); // Turn Off Lighting
    glDepthMask (GL_FALSE); // Turn Off Writing To The Depth-Buffer
    glDepthFunc (GL_LEQUAL);
    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);// Don't Draw Into The Colour Buffer
    glEnable (GL_STENCIL_TEST); // Turn On Stencil Buffer Testing
    glStencilFunc (GL_ALWAYS, 1, SHADOW_STENCIL_MASK);

    // first pass, increase stencil value on the shadow's outside
    glFrontFace (GL_CW);
    glStencilOp (GL_KEEP, GL_KEEP, GL_INCR);
    ShadowPass (edges, invPosLight);

    // second pass, decrease stencil value on the shadow's inside
    glFrontFace (GL_CCW);
    glStencilOp (GL_KEEP, GL_KEEP, GL_DECR);
    ShadowPass (edges, invPosLight);

    /*
        Set the stencil pixels to zero where the dummy is drawn and where the
        sky is drawn.
     */
    glFrontFace (GL_CW);
    glStencilOp (GL_KEEP, GL_KEEP, GL_ZERO);
    RenderNormalMapVertices (&vbo_dummy, index_tangent, index_bitangent);

    glLoadIdentity ();

    // Set every stencil pixel behind the walls/floor to zero
    RenderQuadCoverScreen (0.9f * SHADOW_INFINITY);

    // Turn color rendering back on and draw to nonzero stencil pixels
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc (GL_NOTEQUAL, 0, SHADOW_STENCIL_MASK);
    glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);

    // Draw a shadowing rectangle, covering the entire screen
    glEnable (GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glColor4f (0.0f, 0.0f, 0.0f, 0.4f);
    RenderQuadCoverScreen (NEAR_VIEW + 0.1f);

    /*
       Render the particles after the shadows, because they're transparent
       and that doesn't go well with the depth buffer.
    */

    glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
    glLoadMatrixf (matCameraView.m);
    glEnable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    glDisable (GL_STENCIL_TEST);
    glEnable (GL_TEXTURE_2D);

    for (int i = 0; i < N_PARTICLES; i++)
    {
        RenderSprite (particles [i].pos, &texPar, 0, 0, 64, 64, particles [i].size);
    }
}
void ShadowScene::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    // zoom in or out, but not closer than 0.5

    distCamera -= 0.3f * event->y;

    if (distCamera < 0.5f)
        distCamera = 0.5f;
}
void ShadowScene::OnKeyPress (const SDL_KeyboardEvent *event)
{
    if (event->type == SDL_KEYDOWN)
    {
        if (event->keysym.sym == SDLK_SPACE && onGround)
        {
            // jump
            vy = 15.0f;
        }

        /*
            The following keys toggle settings:
         */
        else if (event->keysym.sym == SDLK_b)
        {
            show_bones = !show_bones;
        }
        else if (event->keysym.sym == SDLK_n)
        {
            show_normals = !show_normals;
        }
        else if (event->keysym.sym == SDLK_z)
        {
            show_triangles = !show_triangles;
        }
    }
}
void ShadowScene::OnMouseMove(const SDL_MouseMotionEvent *event)
{
    if (SDL_GetRelativeMouseMode ())
        SDL_SetRelativeMouseMode(SDL_FALSE);

    if(event -> state & SDL_BUTTON_LMASK)
    {
        // Change camera angles, if mouse key is pressed

        angleY += 0.01f * event -> xrel;
        angleX += 0.01f * event -> yrel;

        if (angleX > 1.5f)
            angleX = 1.5f;
        if (angleX < -1.5f)
            angleX = -1.5f;
    }
}

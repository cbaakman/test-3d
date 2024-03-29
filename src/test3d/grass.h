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

#ifndef PLAY_H
#define PLAY_H

#include "app.h"
#include "../texture.h"
#include "../matrix.h"
#include "terragen.h"
#include "collision.h"

#include "../random.h"
#include "chunk.h"
#include <map>

#define GRID_SIZE 20
#define N_PLAYER_COLLIDERS 2

class GrassScene : public App::Scene
{
private:
    vec3 playerVelocity;
    bool playerOnGround;
    vec3 playerPos;
    float playerLookAngleX,
          playerLookAngleY;

    // wind parameters, makes the grass move
    vec3 wind;
    float t;

    /*
        GrassMode enum type cannot be incremented.
        So treat it as an int.
     */
    int mode;
    enum GrassMode
    {
        GRASSMODE_LAYER,
        GRASSMODE_POLYGON,

        N_GRASSMODES
    };

    Texture texDots,
            texGrass;

    GLuint layerShader,
           polyShader,
           groundShader;

    TerraGen <TerrainType> *pTerraGen;

    std::map <ChunkID, GrassChunk*> chunks;

    std::list <ColliderP> playerColliders;
public:
    GrassScene (App *);
    ~GrassScene ();

    void AddAll (Loader *);

    void OnMouseMove (const SDL_MouseMotionEvent *);
    void OnKeyPress (const SDL_KeyboardEvent *);

    void Update (const float dt);
    void Render (void);
};

#endif // PLAY_H

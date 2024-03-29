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


#ifndef HUB_H
#define HUB_H

#include "water.h"
#include "shadow.h"
#include "toon.h"
#include "mapper.h"
#include "vecs.h"
#include "grass.h"

#include "../font.h"

/*
    The hub scene manager switching between scenes.
    It's also responsible for showing help text.
 */
class HubScene : public App::Scene{

private:
    WaterScene *pWaterScene;
    ShadowScene *pShadowScene;
    ToonScene *pToonScene;
    MapperScene *pMapperScene;
    VecScene *pVecScene;
    GrassScene *pGrassScene;

    // Currently rendered scene:
    Scene *pCurrent;

    // Font for the help text shown:
    Font font;

    // text alpha value
    GLfloat alphaH, alphaBlurInfo;

    // current help string index:
    int help;

    // help strings:
    std::string helpText [6];

    // Used for calculating motion blur
    float blurTimeStep;
    int nBlurFrames;
public:
    void OnEvent (const SDL_Event *event);

    HubScene (App*);
    ~HubScene ();

    void AddAll (Loader *);
    void Update (float dt);
    void Render (void);
    void OnKeyPress (const SDL_KeyboardEvent *event);
};

#endif // HUB_H

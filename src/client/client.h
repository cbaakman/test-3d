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


#ifndef CLIENT_H
#define CLIENT_H

#include <GL/glew.h>
#include <GL/gl.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <SDL2/SDL_opengl.h>

#define WINDOW_TITLE "client"

#include <stdio.h>
#include <string>

class EventListener
{
public:
    virtual void OnEvent (const SDL_Event *event);
    virtual void OnShutDown () {}

protected:
    virtual void OnMouseClick (const SDL_MouseButtonEvent *) {}
    virtual void OnMouseMove (const SDL_MouseMotionEvent *) {}
    virtual void OnKeyPress (const SDL_KeyboardEvent *) {}
    virtual void OnMouseWheel (const SDL_MouseWheelEvent *) {}
    virtual void OnTextEdit (const SDL_TextEditingEvent *) {}
    virtual void OnTextInput (const SDL_TextInputEvent *) {}
};

class Client
{
private:

    bool fullscreen, has_audio;
    std::string settingsPath;

    SDL_Window *mainWindow;
    SDL_GLContext mainGLContext;
    bool done;

    IPaddress serverAddress,
              *pUDPAddress;
    UDPsocket udp_socket;

    UDPpacket   *toServer,
                *fromServer,
                **udpPackets;

#ifdef _WIN32
    HICON icon;
#endif

public:

    class Scene : public EventListener
    {
    protected:
        Client *pClient;

    public:
        Scene (Client *);
        virtual void OnServerMessage(const Uint8* data, const int len) {}

        virtual bool Init(void) { return true; }
        virtual void Update(float dt) {}
        virtual void Render(void) = 0;
    };

    // Constructor and destructor
    Client (void);
    virtual ~Client (void);

    bool Init (void);

    void MainLoop (void);

    // Cleanup functions
    void CleanUp (void);

    void ShutDown (void);

    SDL_Window *GetMainWindow () { return mainWindow; }

    bool SendToServer (const Uint8* data, const int len);

    TCPsocket Server_TCP_Connect ();

    IPaddress *GetUDPAddress ();

    void SwitchScene (Scene* scene); // NULL to do nothing

    bool WindowFocus () const;

    bool AudioEnabled () { return has_audio; }

    bool SetFullScreen (const bool fullscreen);
    bool SetResolution (const int w, const int h);
private:

    Scene *pScene;

    // Event functions
    void HandleEvent (const SDL_Event* event);
};

#endif // CLIENT_H

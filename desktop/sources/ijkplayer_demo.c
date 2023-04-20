/*****************************************************************************
 * ijkplayer_demo.c
 *****************************************************************************
 *
 * copyright (c) 2019 befovy <befovy@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ijkplayer_desktop.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <Windows.h>
#else

#include <unistd.h>

#endif

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <SDL.h>
#include "ijksdl.h"

#define SCR_WIDTH 800
#define SCR_HEIGHT 600

typedef struct IjkDemoInfo
{
    IjkFFMediaPlayer *fp;
    int state;
    int w, h;
    int isSeeking;
    int64_t duration;
} IjkDemoInfo;

void demo_event_cb(void *userdata, int what, int arg1, int arg2, void *extra)
{
    printf("demo event cb what %5d:(%5d %5d)\n", what, arg1, arg2);

    IjkDemoInfo *info = userdata;
    switch (what)
    {
    case IJK_MSG_PLAYBACK_STATE_CHANGED:
        info->state = arg1;
        break;
    case IJK_MSG_PREPARED:
        info->duration = ijkff_get_duration(info->fp);
        break;
    case IJK_MSG_VIDEO_SIZE_CHANGED:
        info->w = arg1;
        info->h = arg2;
        break;
    case IJK_MSG_SEEK_COMPLETE:
        info->isSeeking = 0;
        break;
    default:
        break;
    }
}

static void error_callback(int error, const char *description)
{
    fprintf(stderr, "Error: %s\n", description);
}

// void processInput(GLFWwindow *window) {
//     if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
//         glfwSetWindowShouldClose(window, true);
// }

// static void key_callback(GLFWwindow *window, int key, int scancode, int
// action, int mods) {
//     if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
//         glfwSetWindowShouldClose(window, GLFW_TRUE);
// }

int processEvent(IjkFFMediaPlayer *fp, IjkDemoInfo *info,
                 const SDL_Event *event)
{
    if (!fp || !event)
        return SDL_QUIT;

    int ret = 0;
    switch (event->type)
    {
    case SDL_QUIT:
        ret = SDL_QUIT;
        break;
    case SDL_KEYDOWN:
    {
        switch (event->key.keysym.sym)
        {
        case SDLK_SPACE:
        {
            if (info->state == IJK_STATE_STARTED)
                ijkff_pause(fp);
            else if (info->state == IJK_STATE_PAUSED)
                ijkff_start(fp);
            break;
        }
        case SDLK_UP:
        case SDLK_DOWN:
        {
            float volume = ijkff_get_playback_volume(fp);
            volume += event->key.keysym.sym == SDLK_DOWN ? -0.1f : 0.1f;
            if (volume < 0.0f)
                volume = 0.0f;
            if (volume > 1.0f)
                volume = 1.0f;
            ijkff_set_playback_volume(fp, volume);
            break;
        }
        case SDLK_LEFT:
        case SDLK_RIGHT:
        {
            if (ijkff_is_playing(fp))
            {
                int64_t position = ijkff_get_current_position(fp);
                int64_t delta = info->duration / 40;
                int64_t target =
                    position + (event->key.keysym.sym == SDLK_LEFT ? -delta : delta);
                if (target < 0)
                    target = 0;
                if (target > info->duration)
                    target = info->duration;
                ijkff_seek_to(fp, target);
            }
            break;
        }
        case SDLK_ESCAPE:
            ret = SDL_QUIT;
            break;
        default:
            break;
        }
    default:
        break;
    }
    }
    return ret;
}

typedef struct CbData
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Uint32 format;
    int width;
    int height;
} CbData;

void ijk_overlay_cb(void *userdata, IjkFFOverlay *overlay)
{
    CbData *opaque = (CbData*)userdata;
     if (opaque->renderer == NULL) {
        SDL_Window *window = opaque->window;
        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            renderer = SDL_CreateRenderer(window, -1, 0);
        }
        if (renderer == NULL) {
            //ALOGE("SDL2 2 Error : %s", SDL_GetError());
        }
        opaque->renderer = renderer;
    }

    SDL_Renderer *renderer = opaque->renderer;

    int dest_sdl2_format = SDL_PIXELFORMAT_UNKNOWN;
    bool is_yuv = false;
    switch (overlay->format) {
        case SDL_FCC_I420:
        case SDL_FCC_IYUV:
            dest_sdl2_format = SDL_PIXELFORMAT_IYUV;
            is_yuv = true;
            break;
        case SDL_FCC_RV16:
            dest_sdl2_format = SDL_PIXELFORMAT_RGB565;
            break;
        case SDL_FCC_RV24:
            dest_sdl2_format = SDL_PIXELFORMAT_RGB888;
            break;
        case SDL_FCC_BGRA:
            dest_sdl2_format = SDL_PIXELFORMAT_BGRA32;
            break;
        case SDL_FCC_RGBA:
            dest_sdl2_format = SDL_PIXELFORMAT_RGBA32;
            break;
        default:
            break;
    }
    if(dest_sdl2_format == SDL_PIXELFORMAT_UNKNOWN)
        return;
    //assert(dest_sdl2_format != SDL_PIXELFORMAT_UNKNOWN);

    Uint32 format = 0;
    int access = 0, w = 0, h = 0, ret = 0;
    if (opaque->texture != NULL)
        ret = SDL_QueryTexture(opaque->texture, &format, &access, &w, &h);
    if (ret < 0 || format != opaque->format || w != overlay->w || h != overlay->h) {
        if (opaque->texture)
            SDL_DestroyTexture(opaque->texture);
        opaque->texture = SDL_CreateTexture(renderer, dest_sdl2_format, SDL_TEXTUREACCESS_STREAMING, overlay->w,
                                            overlay->h);
        opaque->format = dest_sdl2_format;
    }
    if (opaque->texture == NULL) {
        return;
    }
    SDL_Texture *texture = opaque->texture;
    if (is_yuv) {
        SDL_UpdateYUVTexture(texture, NULL,
                             overlay->pixels[0], overlay->pitches[0],
                             overlay->pixels[1], overlay->pitches[1],
                             overlay->pixels[2], overlay->pitches[2]);
    } else {
        SDL_UpdateTexture(texture, NULL, overlay->pixels[0], overlay->pitches[0]);
    }
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    return;
}

int main(int argc, char *argv[])
{
#if GLFW
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
    {
        exit(EXIT_FAILURE);
    }
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if APPLE
//     glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow *window =
        glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "OpenGL", NULL, NULL);
    if (window == NULL)
    {
        // cout << "Failed to create window" << endl;
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowPos(window, 200, 200);
    glfwShowWindow(window);

    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    // gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwMakeContextCurrent(NULL);
#endif

    IjkFFMediaPlayer *fp = ijkff_create(IJK_VOUT_CALLBACK);
    // IjkFFMediaPlayer *fp = ijkff_create(IJK_VOUT_SDL2);

    IjkDemoInfo info;
    memset(&info, 0, sizeof(info));
    info.fp = fp;
    ijkff_set_event_cb(fp, &info, demo_event_cb);

    ijkff_set_option(fp, "fcc-bgra", "overlay-format", IJK_OPT_CATEGORY_PLAYER);
    ijkff_set_int_option(fp, 1, "start-on-prepared", IJK_OPT_CATEGORY_PLAYER);
    ijkff_set_data_source(fp, "https://player.alicdn.com/video/aliyunmedia.mp4");

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO);

    SDL_Window *window =
        SDL_CreateWindow("Video", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         640, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

    CbData* cb = (CbData*)calloc(1,sizeof(CbData));
    memset(cb,0,sizeof(CbData));
    cb->window = window;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    //vout->opaque->context = SDL_GL_GetCurrentContext();
    //SDL_GL_MakeCurrent(window, NULL);
    CbData* opaque = cb;
    if (opaque->renderer != NULL) {
        SDL_DestroyRenderer(opaque->renderer);
        opaque->renderer = NULL;
    }
    if (opaque->renderer == NULL && window != NULL) {
        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            renderer = SDL_CreateRenderer(window, -1, 0);
        }
        if (renderer == NULL) {
            // ALOGE("SDL2 create renderer failed: %s", SDL_GetError());
        }
        opaque->renderer = renderer;
    }
    ijkff_set_overlay_cb(fp, cb, ijk_overlay_cb);
    // ijkff_set_window(fp, window);
    ijkff_prepare_async(fp);
    // ijkff_start(fp);

#if GLFW
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);
        glfwPollEvents();
    }
#endif

    SDL_Event event;
    while (1)
    {
        SDL_WaitEvent(&event);
        if (processEvent(fp, &info, &event) == SDL_QUIT)
            break;
    }
    SDL_Quit();

    ijkff_stop(fp);
    ijkff_shutdown(fp);

    return 0;
}

// vulkan-test.cpp : Defines the entry point for the application.
//

#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL.h>
#undef main

#include <iostream>


#include "../src/vulkan_wrapper.hpp"
#include "vulkan-test.h"

int main(int argc, char ** argv) {

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_Window *window = SDL_CreateWindow("Hello World!", 100, 100, 512, 512, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (window == nullptr) {
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}

	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	HWND hwnd = wmInfo.info.win.window;

	HINSTANCE hi = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);

	vulkan::wrapper vk(true);
	vk.init(hwnd, hi);

	bool is_quit = false;
	while (!is_quit) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			//

			if (event.type == SDL_WINDOWEVENT) {
				if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
					vk.demo_resize();
				}

			} else if (event.type == SDL_QUIT) {
				is_quit = true;
			} else if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_ESCAPE) {
					is_quit = true;
				}
			}
		}
		vk.demo_tick();
		SDL_Delay(3);

	}
	SDL_Quit();
	return 1;
}

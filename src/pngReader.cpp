//
//  pngReader.cpp
//  space
//
//  Created by Josh McNamee on 29/09/2015.
//  Copyright © 2015 josh04. All rights reserved.
//

#include "pngReader.hpp"


#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <stdint.h>
#include <memory>
#include <png.h>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace load_image {
    
    
    std::string get_path(const char * file_name) {
#ifdef __APPLE__
        CFBundleRef mainBundle = CFBundleGetMainBundle();
        CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
        char path[PATH_MAX];
        if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
        {
            // error!
        }
        CFRelease(resourcesURL);
#elif defined(_WIN32)
		char path[MAX_PATH] = "./resources";
#endif

        auto png_path = std::string(path) + "/" + std::string(file_name);
        return png_path;
    }
    
    std::shared_ptr<uint8_t> png(const char * file_name, unsigned int &width, unsigned int &height) {
        char header[8];    // 8 is the maximum size that can be checked
        
        /* open file and test for it being a png */
        FILE *fp = fopen(file_name, "rb");
        if (!fp)
            throw std::runtime_error("[read_png_file] File %s could not be opened for reading");
        fread(header, 1, 8, fp);
        if (png_sig_cmp((png_const_bytep)header, 0, 8))
            throw std::runtime_error("[read_png_file] File %s is not recognized as a PNG file");
        
        
        /* initialize stuff */
        auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        
        if (!png_ptr)
            throw std::runtime_error("[read_png_file] png_create_read_struct failed");
        
        auto info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr)
            throw std::runtime_error("[read_png_file] png_create_info_struct failed");
        
        if (setjmp(png_jmpbuf(png_ptr)))
            throw std::runtime_error("[read_png_file] Error during init_io");
        
        png_init_io(png_ptr, fp);
        png_set_sig_bytes(png_ptr, 8);
        
        png_read_info(png_ptr, info_ptr);
        
        width = png_get_image_width(png_ptr, info_ptr);
        height = png_get_image_height(png_ptr, info_ptr);
        auto color_type = png_get_color_type(png_ptr, info_ptr);
        auto bit_depth = png_get_bit_depth(png_ptr, info_ptr);
        
        auto number_of_passes = png_set_interlace_handling(png_ptr);
        png_read_update_info(png_ptr, info_ptr);
        
        
        /* read file */
        if (setjmp(png_jmpbuf(png_ptr)))
            throw std::runtime_error("[read_png_file] Error during read_image");
        
        auto final_image = std::shared_ptr<uint8_t>((uint8_t *) malloc(sizeof(uint8_t) * height * png_get_rowbytes(png_ptr,info_ptr)), [](uint8_t * ptr){ if (ptr != NULL) {free(ptr);} } );
        
        auto row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
        
        for (int y=0; y<height; y++) {
            row_pointers[y] = &(final_image.get()[y*png_get_rowbytes(png_ptr,info_ptr)]);
        }
        
        png_read_image(png_ptr, row_pointers);
        
        free(row_pointers);
        fclose(fp);
        
        return final_image;
    };
    
}


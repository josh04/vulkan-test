//
//  pngReader.hpp
//  space
//
//  Created by Josh McNamee on 29/09/2015.
//  Copyright © 2015 josh04. All rights reserved.
//

#ifndef pngReader_hpp
#define pngReader_hpp

#include <memory>
#include <string>

namespace load_image {
    
    std::string get_path(const char * file_name);
    
    std::shared_ptr<uint8_t> png(const char * file_name, unsigned int &width, unsigned int &height);
    
}

#endif /* pngReader_hpp */

//
// Created by MrGrim on 10/4/2022.
//

#ifndef MELON_MEM_CUTILS_H
#define MELON_MEM_CUTILS_H

#include <cstdlib>

namespace melon::mem
{
    template<class T>
    struct free_deleter
    {
        void operator()(T *ptr) const
        {
            std::free(static_cast<void *>(ptr));
        }
    };
}

#endif //MELON_MEM_CUTILS_H

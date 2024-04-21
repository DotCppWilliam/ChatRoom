#pragma once

#include "utf8_converter.h"

namespace logging 
{
    template <typename NextConverter = UTF8Converter>
    class NativeEOLConverter : public NextConverter
    {
    
    };
}

#pragma once
#include "renderer.h"

InputLayout &InputLayout::addElement(InputLayoutElement element)
{
    elements.push_back(element);
    return *this;
}
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <sal.h>
#include <new>
#include <warning.h>
#include <DirectXMath.h>
#include <stdio.h>

typedef _Return_type_success_(return == DUPL_RETURN_SUCCESS) enum
{
    DUPL_RETURN_SUCCESS = 0,
    DUPL_RETURN_ERROR_EXPECTED = 1,
    DUPL_RETURN_ERROR_UNEXPECTED = 2
}DUPL_RETURN;

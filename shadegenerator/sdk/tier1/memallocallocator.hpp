#ifndef RAWALLOCATOR_H
#define RAWALLOCATOR_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.hpp"
#include "tier0/basetypes.hpp"

#include <limits>

#include "tier0/memdbgon.hpp"

class CMemAllocAllocator
{
public:
	template<typename T, typename I = int>
	static T* Alloc( I nCount, I &nAdjustedCount )
	{
		size_t byte_size = nCount * sizeof( T );
		T *ptr = (T *)malloc( byte_size );
		nAdjustedCount = MIN( (I)(MAX( _msize( ptr ), byte_size ) / sizeof( T )), (std::numeric_limits<I>::max)() );
		return ptr;
	}

	template<typename T, typename I = int>
	static T* Realloc( T *base, I nCount, I &nAdjustedCount )
	{
		size_t byte_size = nCount * sizeof( T );
		T *ptr = (T *)realloc( base, byte_size );
		nAdjustedCount = MIN( (I)(MAX( _msize( ptr ), byte_size ) / sizeof( T )), (std::numeric_limits<I>::max)() );
		return ptr;
	}

	static void Free( void* pMem )
	{
		//if(pMem)
		//	free( pMem );
	}
};

#include "tier0/memdbgoff.hpp"

#endif // RAWALLOCATOR_H

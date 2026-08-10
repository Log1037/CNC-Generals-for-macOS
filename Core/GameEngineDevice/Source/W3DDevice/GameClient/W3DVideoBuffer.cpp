/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//----------------------------------------------------------------------------
//
// Project:   Generals
//
// Module:    Video
//
// File name: W3DDevice/GameClient/W3DVideoBuffer.cpp
//
// Created:   10/23/01 TR
//
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//         Includes
//----------------------------------------------------------------------------

#include "Common/GameMemory.h"
#include "WW3D2/texture.h"
#include "WW3D2/textureloader.h"
#include "W3DDevice/GameClient/W3DVideoBuffer.h"

#include <cstdio>

//----------------------------------------------------------------------------
//         Externals
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Defines
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Types
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Data
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Data
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Prototypes
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Functions
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Functions
//----------------------------------------------------------------------------


//============================================================================
// W3DVideoBuffer::W3DVideoBuffer
//============================================================================

W3DVideoBuffer::W3DVideoBuffer( VideoBuffer::Type format )
: VideoBuffer(format),
	m_texture(nullptr),
	m_surface(nullptr)
{

}


//============================================================================
// W3DVideoBuffer::SetBuffer
//============================================================================

Bool W3DVideoBuffer::allocate( UnsignedInt width, UnsignedInt height )
{
	free();

	UnsignedInt textureWidth = width;
	UnsignedInt textureHeight = height;
	unsigned int temp_depth=1;
	TextureLoader::Validate_Texture_Size( textureWidth, textureHeight, temp_depth);

	// Direct3D 8-era format capability reporting is not completely reliable
	// through DXVK/MoltenVK. In particular, a format can be reported as usable
	// yet fail when the managed texture is locked for FFmpeg output. Try the
	// preferred format first (32-bit XRGB first on Apple), then the other video
	// formats supported by the decoder. This fixes every VideoBuffer consumer,
	// rather than special-casing individual loading screens.
#if defined(__APPLE__)
	const VideoBuffer::Type candidates[] = {
		VideoBuffer::TYPE_X8R8G8B8,
		m_format,
		VideoBuffer::TYPE_R5G6B5,
		VideoBuffer::TYPE_X1R5G5B5,
		VideoBuffer::TYPE_R8G8B8
	};
#else
	const VideoBuffer::Type candidates[] = {
		m_format,
		VideoBuffer::TYPE_X8R8G8B8,
		VideoBuffer::TYPE_R8G8B8,
		VideoBuffer::TYPE_R5G6B5,
		VideoBuffer::TYPE_X1R5G5B5
	};
#endif

	VideoBuffer::Type attempted[NUM_TYPES] = {};
	Int attemptedCount = 0;
	for (VideoBuffer::Type candidate : candidates)
	{
		if (candidate <= VideoBuffer::TYPE_UNKNOWN || candidate >= VideoBuffer::NUM_TYPES)
			continue;

		Bool duplicate = FALSE;
		for (Int i = 0; i < attemptedCount; ++i)
		{
			if (attempted[i] == candidate)
			{
				duplicate = TRUE;
				break;
			}
		}
		if (duplicate)
			continue;
		attempted[attemptedCount++] = candidate;

		m_format = candidate;
		m_width = width;
		m_height = height;
		m_textureWidth = textureWidth;
		m_textureHeight = textureHeight;

		WW3DFormat w3dFormat = TypeToW3DFormat(m_format);
		if (w3dFormat == WW3D_FORMAT_UNKNOWN)
			continue;

		m_texture = MSGNEW("TextureClass") TextureClass(
			m_textureWidth,
			m_textureHeight,
			w3dFormat,
			MIP_LEVELS_1,
			TextureClass::POOL_MANAGED,
			false,
			false);

		if (m_texture == nullptr || m_texture->Peek_D3D_Texture() == nullptr)
		{
			fprintf(stderr,
				"WARN: GX video buffer texture creation failed type=%d size=%ux%u texture=%ux%u\n",
				(int)m_format,
				width,
				height,
				m_textureWidth,
				m_textureHeight);
			free();
			continue;
		}

		void *bits = lock();
		if (bits == nullptr)
		{
			fprintf(stderr,
				"WARN: GX video buffer lock failed type=%d size=%ux%u texture=%ux%u\n",
				(int)m_format,
				width,
				height,
				m_textureWidth,
				m_textureHeight);
			free();
			continue;
		}

		fprintf(stderr,
			"INFO: GX video buffer allocated type=%d size=%ux%u texture=%ux%u pitch=%u\n",
			(int)m_format,
			width,
			height,
			m_textureWidth,
			m_textureHeight,
			m_pitch);
		unlock();
		return TRUE;
	}

	fprintf(stderr,
		"ERROR: GX video buffer allocation exhausted all formats size=%ux%u texture=%ux%u\n",
		width,
		height,
		textureWidth,
		textureHeight);
	return FALSE;
}

//============================================================================
// W3DVideoBuffer::~W3DVideoBuffer
//============================================================================

W3DVideoBuffer::~W3DVideoBuffer()
{
	free();
}

//============================================================================
// W3DVideoBuffer::lock
//============================================================================

void*		W3DVideoBuffer::lock()
{
	void *mem = nullptr;
	if (m_texture == nullptr || m_texture->Peek_D3D_Texture() == nullptr)
	{
		return nullptr;
	}

	if ( m_surface != nullptr )
	{
		unlock();
	}

	m_surface = m_texture->Get_Surface_Level();

	if ( m_surface )
	{
		mem = m_surface->Lock( (Int*) &m_pitch );
	}

	return mem;
}

//============================================================================
// W3DVideoBuffer::unlock
//============================================================================

void		W3DVideoBuffer::unlock()
{
	if ( m_surface != nullptr )
	{
		m_surface->Unlock();
		m_surface->Release_Ref();
		m_surface = nullptr;
	}
}

//============================================================================
// W3DVideoBuffer::valid
//============================================================================

Bool		W3DVideoBuffer::valid()
{
	return m_texture != nullptr && m_texture->Peek_D3D_Texture() != nullptr;
}

//============================================================================
// W3DVideoBuffer::reset
//============================================================================

void	W3DVideoBuffer::free()
{
	unlock();

	if ( m_texture )
	{
		unlock();
		m_texture->Release_Ref();
		m_texture = nullptr;
	}
	m_surface = nullptr;

	VideoBuffer::free();
}


//============================================================================
// W3DVideoBuffer::TypeToW3DFormat
//============================================================================

WW3DFormat W3DVideoBuffer::TypeToW3DFormat( VideoBuffer::Type format )
{
	WW3DFormat w3dFormat = WW3D_FORMAT_UNKNOWN;
	switch ( format )
	{
		case TYPE_X8R8G8B8:
			w3dFormat = WW3D_FORMAT_X8R8G8B8;
			break;

 		case TYPE_R8G8B8:
			w3dFormat = WW3D_FORMAT_R8G8B8;
			break;

 		case TYPE_R5G6B5:
			w3dFormat = WW3D_FORMAT_R5G6B5;
			break;

 		case TYPE_X1R5G5B5:
			w3dFormat = WW3D_FORMAT_X1R5G5B5;
			break;
	}

	return w3dFormat;
}

//============================================================================
// W3DFormatToType
//============================================================================

VideoBuffer::Type W3DVideoBuffer::W3DFormatToType( WW3DFormat w3dFormat )
{
	Type format = TYPE_UNKNOWN;
	switch ( w3dFormat )
	{
		case WW3D_FORMAT_X8R8G8B8:
				format = VideoBuffer::TYPE_X8R8G8B8;
				break;
		case WW3D_FORMAT_R8G8B8:
				format = VideoBuffer::TYPE_R8G8B8;
				break;
		case WW3D_FORMAT_R5G6B5:
				format = VideoBuffer::TYPE_R5G6B5;
				break;
		case WW3D_FORMAT_X1R5G5B5:
				format = VideoBuffer::TYPE_X1R5G5B5;
				break;
	}

	return format;
}

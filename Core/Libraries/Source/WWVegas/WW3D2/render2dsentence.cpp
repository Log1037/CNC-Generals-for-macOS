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

 /***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/render2dsentence.cpp                   $*
 *                                                                                             *
 *                       $Author:: Patrick                  $*
 *                                                                                             *
 *								$Modtime:: 8/29/01 11:16a                                             $*
 *                                                                                             *
 *                    $Revision:: 13                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "render2dsentence.h"
// GeneralsX @feature 10/08/2026 OS/2 PANOSE access, used to tell a Song face from a Hei face.
// Kept in the .cpp so the SFNT table types do not leak into every translation unit.
#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
	#include FT_TRUETYPE_TABLES_H
	#include <unistd.h>
	#include <limits.h>
#endif
#include "surfaceclass.h"
#include "texture.h"
#include "wwprofile.h"
#include "wwmemlog.h"
#include "dx8wrapper.h"


////////////////////////////////////////////////////////////////////////////////////
//	Local constants
////////////////////////////////////////////////////////////////////////////////////
#define no_TEST_PLACEMENT 1	 // Shows alignment markers for text.

#define TEXTURE_OFFSET 2
////////////////////////////////////////////////////////////////////////////////////
//
//	Render2DSentenceClass
//
////////////////////////////////////////////////////////////////////////////////////
Render2DSentenceClass::Render2DSentenceClass () :
	Font (nullptr),
	Location (0.0F,0.0F),
	Cursor (0.0F,0.0F),
	TextureOffset (0, 0),
	TextureStartX (0),
	CurSurface (nullptr),
	CurrTextureSize (0),
	MonoSpaced (false),
	IsClippedEnabled (false),
	ClipRect (0, 0, 0, 0),
	BaseLocation (0, 0),
	LockedPtr (nullptr),
	LockedStride (0),
	TextureSizeHint (0),
	WrapWidth (0),
	Centered (false),
	DrawExtents (0, 0, 0, 0),
	ParseHotKey( false ),
	useHardWordWrap( false)
{
	Shader = Render2DClass::Get_Default_Shader ();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	~Render2DSentenceClass
//
////////////////////////////////////////////////////////////////////////////////////
Render2DSentenceClass::~Render2DSentenceClass ()
{
	REF_PTR_RELEASE (Font);
	Reset ();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Set_Font
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Font (FontCharsClass *font)
{
	Reset ();
	REF_PTR_SET (Font, font);
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Reset_Polys
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Reset_Polys ()
{
	for (int index = 0; index < Renderers.Count (); index ++) {
		Renderers[index].Renderer->Reset ();
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Reset
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Reset ()
{
	//
	//	Make sure we unlock the current surface (if necessary)
	//
	if (LockedPtr != nullptr) {
		CurSurface->Unlock ();
		LockedPtr = nullptr;
	}

	//
	//	Release our hold on the current surface
	//
	REF_PTR_RELEASE (CurSurface);

	//
	//	Free each renderer
	//
	while (Renderers.Count () > 0) {
		delete Renderers[0].Renderer;
		Renderers.Delete(0);
	}

	Cursor.Set (0, 0);
	MonoSpaced = false;
	ParseHotKey = false;

	Release_Pending_Surfaces ();
	Reset_Sentence_Data ();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Make_Additive
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Make_Additive ()
{
	Shader.Set_Dst_Blend_Func (ShaderClass::DSTBLEND_ONE);
	Shader.Set_Src_Blend_Func (ShaderClass::SRCBLEND_ONE);
	Shader.Set_Primary_Gradient (ShaderClass::GRADIENT_MODULATE);
	Shader.Set_Secondary_Gradient (ShaderClass::SECONDARY_GRADIENT_DISABLE);

	Set_Shader (Shader);
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Make_Additive
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Shader (ShaderClass shader)
{
	Shader = shader;

	//
	//	Change each renderer's shader
	//
	for (int i = 0; i < Renderers.Count (); i ++) {
		ShaderClass *curr_shader = Renderers[i].Renderer->Get_Shader ();
		(*curr_shader) = Shader;
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Render
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Render ()
{
	//
	//	Build any textures that are pending
	//
	Build_Textures ();

	//
	//	Ask each renderer to draw its contents
	//
	for (int i = 0; i < Renderers.Count (); i ++) {
		Renderers[i].Renderer->Render ();
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Set_Base_Location
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Base_Location (const Vector2 &loc)
{
	Vector2 dif		= loc - BaseLocation;
	BaseLocation	= loc;
	for (int i = 0; i < Renderers.Count (); i ++) {
		Renderers[i].Renderer->Move (dif);
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Set_Location
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Location (const Vector2 &loc)
{
	Location	= loc;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Text_Extents
//
////////////////////////////////////////////////////////////////////////////////////
Vector2
Render2DSentenceClass::Get_Text_Extents (const WCHAR *text)
{
	Vector2 extent (0, Font->Get_Char_Height());

	while (*text) {
		WCHAR ch = *text++;

		if ( ch != (WCHAR)'\n' ) {
			extent.X += Font->Get_Char_Spacing( ch );
		}
	}

	return extent;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Formatted_Text_Extents
//
////////////////////////////////////////////////////////////////////////////////////
Vector2
Render2DSentenceClass::Get_Formatted_Text_Extents (const WCHAR *text)
{
	return Build_Sentence_Not_Centered(text, nullptr, nullptr, true);
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Reset_Sentence_Data
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Reset_Sentence_Data ()
{
	//
	//	Release our hold on each texture used in the sentence
	//
	for (int index = 0; index < SentenceData.Count (); index ++) {
		REF_PTR_RELEASE (SentenceData[index].Surface);
	}

	if (SentenceData.Count()>0) {
		SentenceData.Delete_All ();
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Release_Pending_Surfaces
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Release_Pending_Surfaces ()
{
	//
	//	Release our hold on each pending surface
	//
	for (int index = 0; index < PendingSurfaces.Count (); index ++) {
		SurfaceClass *curr_surface = PendingSurfaces[index].Surface;
		REF_PTR_RELEASE (curr_surface);
	}

	if (PendingSurfaces.Count()>0) PendingSurfaces.Delete_All ();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Textures
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Build_Textures ()
{
	WWMEMLOG(MEM_TEXTURE);

	//
	//	Make sure we unlock the current surface
	//
	if (LockedPtr != nullptr) {
		CurSurface->Unlock ();
		LockedPtr = nullptr;
	}

	//
	//	Release our hold on the current surface
	//
	REF_PTR_RELEASE (CurSurface);
	TextureOffset.Set (0, 0);
	TextureStartX = 0;

	//
	//	Convert all pending surfaces to textures
	//
	for (int index = 0; index < PendingSurfaces.Count (); index ++) {
		PendingSurfaceStruct &surface_info = PendingSurfaces[index];
		SurfaceClass *curr_surface = surface_info.Surface;

		//
		//	Get the dimensions of the surface
		//
		SurfaceClass::SurfaceDescription desc;
		curr_surface->Get_Description (desc);

		//
		//	Create the new texture
		//
		TextureClass *new_texture = W3DNEW TextureClass (desc.Width, desc.Width, WW3D_FORMAT_A4R4G4B4, MIP_LEVELS_1);
		SurfaceClass *texture_surface = new_texture->Get_Surface_Level ();

		new_texture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		new_texture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		new_texture->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_NONE);
		new_texture->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_NONE);
		new_texture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);

		//
		//	Copy the contents of the texture from the surface
		//
		DX8Wrapper::_Copy_DX8_Rects (curr_surface->Peek_D3D_Surface (), nullptr, 0, texture_surface->Peek_D3D_Surface (), nullptr);
		REF_PTR_RELEASE (texture_surface);

		//
		//	Assign this texture to any renderers that need it
		//
		for (int renderer_index = 0; renderer_index < surface_info.Renderers.Count (); renderer_index ++) {
			Render2DClass *renderer = surface_info.Renderers[renderer_index];
			renderer->Set_Texture (new_texture);
		}

		//
		//	Release our hold on the objects
		//
		REF_PTR_RELEASE (new_texture);
		REF_PTR_RELEASE (curr_surface);
	}

	//
	//	Reset the list
	//
	if (PendingSurfaces.Count()>0) {
		PendingSurfaces.Delete_All ();
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Draw_Sentence
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Draw_Sentence (uint32 color)
{
	Render2DClass *curr_renderer	= nullptr;
	SurfaceClass *curr_surface		= nullptr;

	DrawExtents.Set (0, 0, 0, 0);

	int offset = 0;
	//
	//	Loop over all the parts of the sentence
	//
	for (int index = 0; index < SentenceData.Count (); index ++) {
		SentenceDataStruct &data = SentenceData[index];

		//
		//	Has the surface changed?
		//
		if (data.Surface != curr_surface) {
			curr_surface = data.Surface;

			//
			//	Try to find a renderer that uses the same "texture"
			//
			bool found = false;
			for (int renderer_index = 0; renderer_index < Renderers.Count (); renderer_index ++) {
				if (Renderers[renderer_index].Surface == curr_surface) {
					found = true;
					curr_renderer = Renderers[renderer_index].Renderer;
					break;
				}
			}

			//
			//	Create a new renderer if we couldn't find an appropriate one
			//
			if (found == false) {

				//
				//	Allocate a new renderer
				//
				curr_renderer = W3DNEW Render2DClass;
				curr_renderer->Set_Coordinate_Range (Render2DClass::Get_Screen_Resolution ());
				ShaderClass *curr_shader = curr_renderer->Get_Shader ();
				(*curr_shader) = Shader;

				//
				//	Add it to our list
				//
				RendererDataStruct render_info;
				render_info.Renderer	= curr_renderer;
				render_info.Surface	= curr_surface;
				Renderers.Add (render_info);

				//
				//	Now, add this renderer to the surface pending list
				//
				for (int surface_index = 0; surface_index < PendingSurfaces.Count (); surface_index ++) {
					PendingSurfaceStruct &surface_info = PendingSurfaces[surface_index];
					if (surface_info.Surface == curr_surface) {
						surface_info.Renderers.Add (curr_renderer);
					}
				}
			}
		}

		//
		//	Get the dimensions of the surface
		//
		SurfaceClass::SurfaceDescription desc;
		curr_surface->Get_Description (desc);

		//
		//	Add a quad that contains this sentence chunk
		//
		RectClass screen_rect	= data.ScreenRect;
		screen_rect					+= Location;
		RectClass uv_rect			= data.UVRect;

		//
		//	Clip the quad (as necessary)
		//
		bool add_quad = true;
		if (IsClippedEnabled) {

			//
			//	Check for completely clipped
			//
			if (	screen_rect.Right <= ClipRect.Left ||
					screen_rect.Bottom <= ClipRect.Top)
			{
				add_quad = false;
			} else {

				//
				//	Clip the polygons to the specified area
				//
				RectClass clipped_rect;
				clipped_rect.Left		= max (screen_rect.Left, ClipRect.Left);
				clipped_rect.Right	= min (screen_rect.Right, ClipRect.Right);
				clipped_rect.Top		= max (screen_rect.Top, ClipRect.Top);
				clipped_rect.Bottom	= min (screen_rect.Bottom, ClipRect.Bottom);

				//
				//	Clip the texture to the specified area
				//
				RectClass clipped_uv_rect;
				float percent				= ((clipped_rect.Left - screen_rect.Left) / screen_rect.Width ());
				clipped_uv_rect.Left		= uv_rect.Left + (uv_rect.Width () * percent);

				percent						= ((clipped_rect.Right - screen_rect.Left) / screen_rect.Width ());
				clipped_uv_rect.Right	= uv_rect.Left + (uv_rect.Width () * percent);

				percent						= ((clipped_rect.Top - screen_rect.Top) / screen_rect.Height ());
				clipped_uv_rect.Top		= uv_rect.Top + (uv_rect.Height () * percent);

				percent						= ((clipped_rect.Bottom - screen_rect.Top) / screen_rect.Height ());
				clipped_uv_rect.Bottom	= uv_rect.Top + (uv_rect.Height () * percent);

				//
				//	Use the clipped rectangles to render
				//
				screen_rect = clipped_rect;
				uv_rect		= clipped_uv_rect;

				if (screen_rect.Right <= screen_rect.Left ||
						screen_rect.Bottom <= screen_rect.Top)
				{
					add_quad = false;
				}
			}
		}

		if (add_quad) {
			//uv_rect.Bottom += 0.5f;
			uv_rect *=  1.0F / ((float)desc.Width);
#ifdef TEST_PLACEMENT
			screen_rect.Left += offset*3;
			screen_rect.Right += offset*3;
#endif
			offset++;
			curr_renderer->Add_Quad (screen_rect, uv_rect, color);

			//
			//	Add this rectangle to the total draw extents
			//
			if (DrawExtents.Width () == 0) {
				DrawExtents = screen_rect;
			} else {
				DrawExtents += screen_rect;
			}
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Record_Sentence_Chunk
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Record_Sentence_Chunk ()
{
	//
	//	Do we have anything to store?
	//
	int width = TextureOffset.I - TextureStartX;
	if (width > 0) {
		float char_height = Font->Get_Char_Height ();

		//
		//	Build a structure that contains enough information
		// to hold this portion of the sentence
		//
		SentenceDataStruct sentence_data;
		sentence_data.Surface = CurSurface;
		sentence_data.Surface->Add_Ref ();
		sentence_data.ScreenRect.Left		= Cursor.X;
		sentence_data.ScreenRect.Right	= Cursor.X + width;
		sentence_data.ScreenRect.Top		= Cursor.Y;
		sentence_data.ScreenRect.Bottom	= Cursor.Y + char_height;
		sentence_data.UVRect.Left			= TextureStartX;
		sentence_data.UVRect.Top			= TextureOffset.J;
		sentence_data.UVRect.Right			= TextureOffset.I;
		sentence_data.UVRect.Bottom		= TextureOffset.J + char_height;

		//
		//	Add this information to our list
		//
		SentenceData.Add (sentence_data);
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Allocate_New_Surface
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Allocate_New_Surface (const WCHAR *text, bool justCalcExtents)
{
	if (!justCalcExtents)
	{
		//
		//	Unlock the last surface (if necessary)
		//
		if (LockedPtr != nullptr) {
			CurSurface->Unlock ();
			LockedPtr = nullptr;
		}
	}

	//
	// Calculate the width of the text
	//
	int text_width = 0;
	for (int index = 0; text[index] != 0; index ++) {
		text_width += Font->Get_Char_Spacing (text[index]);
	}

	int char_height = Font->Get_Char_Height ();

	//
	//	Find the best texture size for the remaining text
	//
	CurrTextureSize = 256;
	int best_tex_mem_usage = 999999999;
	for (int pow2 = 6; pow2 <= 8; pow2 ++) {

		int size					= 1 << pow2;
		int row_count			= (text_width / size) + 1;
		int rows_per_texture	= size / (char_height + 1);

		//
		//	Can we even fit one character on this texture?
		//
		if (rows_per_texture > 0) {

			//
			//	How many textures (at this size) would it take to render
			// the remaining text?
			//
			int texture_count	= row_count / rows_per_texture;
			texture_count		= max (texture_count, 1);

			//
			//	Is this the best usage of texture memory we've found yet?
			//
			int texture_mem_usage = (texture_count * size * size);
			if (texture_mem_usage < best_tex_mem_usage) {
				CurrTextureSize		= size;
				best_tex_mem_usage	= texture_mem_usage;
			}
		}
	}

	//
	//	Use whichever is larger, the hint or the calculated size
	//
	CurrTextureSize = max (TextureSizeHint, CurrTextureSize);

	if (!justCalcExtents)
	{
		//
		//	Release our extra hold on the old surface
		//
		REF_PTR_RELEASE (CurSurface);

		//
		//	Create the new surface
		//
		CurSurface = NEW_REF (SurfaceClass, (CurrTextureSize, CurrTextureSize, WW3D_FORMAT_A4R4G4B4));
		WWASSERT (CurSurface != nullptr);
		CurSurface->Add_Ref ();

		//
		//	Add this surface to our list
		//
		PendingSurfaceStruct surface_info;
		surface_info.Surface = CurSurface;
		PendingSurfaces.Add (surface_info);
	}

	//
	//	Reset to the upper left corner
	//
	TextureOffset.Set (0, 0);
	TextureStartX = 0;
}

float FindStartingXPos( const WCHAR *text )
{

	return 1;
}
////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Sentence_Centered
//
////////////////////////////////////////////////////////////////////////////////////
void	Render2DSentenceClass::Build_Sentence_Centered (const WCHAR *text, int *hkX, int *hkY)
{
	float char_height = Font->Get_Char_Height ();
	int		wordWidth = 0;
	int notCenteredHotkeyX = 0;
	int notCenteredHotkeyY = 0;
	Vector2 extent = Build_Sentence_Not_Centered(text,&notCenteredHotkeyX, &notCenteredHotkeyY, TRUE); //Get_Formatted_Text_Extents(text);

	//
	//	Start fresh
	//
	Reset_Sentence_Data ();
	Cursor.Set (0, 0);

	//
	//	Ensure we have a surface to start with
	//
	if (CurSurface == nullptr) {
		Allocate_New_Surface (text);
	}



	//
	//	Loop over all the characters in the string
	//
	bool end = false;
	const WCHAR *word;
	int word_width	= 0;
	int line_width	= 0;
	int charCount = 0;
	int wordCount = 0;
	int hotKeyPosX = 0;
	int hotKeyPosY = 0;
	bool calcHotKeyX = false;
	bool dontBlit = false;
	while (!end)
	{
		//
		// Re-init everything for the next line
		//
		word	= text;
		word_width	= 0;
		line_width	= 0;
		charCount = 0;
		wordCount = 0;
		//
		//first find the length of the line till we wrap
		//
		while ( 1 )
		{
			//
			// read a word
			//
			int charWidth = 0;
			while ((*word != 0) && (*word > L' ') && (*word != L'\n')) {
				if( ParseHotKey && (*word == L'&') && (*word+1 != 0) && (*word+1 > L' ') && (*word+1 != L'\n'))
				{
					int offset = 0;
					if (word_width != 0 )
					{
						const WCHAR *word_back = word;
						*word_back--;
						if (*word_back == L' ')
						{
							line_width -= word_width;
							offset =-1;
						}
					}
					*word++;
					calcHotKeyX = true;
				}

				charWidth = Font->Get_Char_Spacing (*word++);
				word_width += charWidth;
				wordCount++;

				if (WrapWidth > 0 && word_width >= WrapWidth && useHardWordWrap)
					break;
			}
			//
			// If this word is unworthy to be on the current line, decrement the space and break
			//
			if(WrapWidth > 0 && (line_width + word_width >= WrapWidth))
			{
				//
				//Take care of the case that the word is too big for the allocated space...
				//If that's the case, drop out and process the word anyway
				//
				if(charCount == 0)
				{
					charCount +=wordCount - 1;
					line_width += word_width - charWidth;
					if(*word == 0)
						end = true;
					break;
				}
				charCount--;
				break;
			}
			//
			// if we reached the end of the text, set the values and break, also set the end flag
			//
			if( *word == 0 )
			{
				charCount +=wordCount;
				line_width += word_width;
				end = true;
				break;
			}
			//
			// otherwise, increment the counts
			//
			charCount +=wordCount + 1;
			line_width += word_width;
			//
			// We were some a new line character break and process
			//
			if(*word != L' ')
				break;
			//
			// add the space to our width
			//
			word_width = Font->Get_Char_Spacing (*word++);
			wordCount = 0;
			line_width += word_width;
		}
		//
		// we now hold the length of the line and it's width lets set our cursor position to center it
		//
		Cursor.X = (int)((extent.X - line_width) / 2);
		if(Cursor.X < 0)
			Cursor.X = 0;
		if(calcHotKeyX)
		{
			calcHotKeyX = false;
			hotKeyPosX = Cursor.X + notCenteredHotkeyX;
		}

		for(int i = 0; i <= charCount; i++) {
			WCHAR ch = *text++;
			dontBlit = false;
			//
			//	Determine how much horizontal space this character requires
			//
			if(ParseHotKey && (ch == L'&') && (*text != 0) && (*text > L' ') && (*text != L'\n'))
			{
				ch = *text++;
				dontBlit = true;
			}
			float char_spacing = Font->Get_Char_Spacing (ch);

			bool exceeded_texture_width	= ((TextureOffset.I + char_spacing) >= CurrTextureSize);
			bool encountered_break_char	= (ch == L' ' || ch == L'\n' || ch == 0);

			//
			//	Do we need to record this portion of the sentence to its own chunk?
			//
			if (exceeded_texture_width || encountered_break_char) {
				Record_Sentence_Chunk ();

				//
				//	Adjust the positions
				//
				Cursor.X			+= (TextureOffset.I - TextureStartX);
				TextureStartX	= TextureOffset.I;

				//
				//	Adjust the output coordinates
				//
				if (ch == L' ') {
					Cursor.X += char_spacing;
				} else if ((ch == 0 )|| (ch == L'\n')) {
					break;
				}

				//
				//	Did the text extend past the edge of the texture?
				//
				if (exceeded_texture_width) {
					TextureStartX		= 0;
					TextureOffset.I	= TextureStartX;
					TextureOffset.J	+= char_height;

					//
					//	Did the text extent completely off the texture?
					//
					if ((TextureOffset.J + char_height) >= CurrTextureSize) {
						Allocate_New_Surface (text);
					}
				}
			}
			//
			//	Adjust the output coordinates
			//
			if (ch != L'\n' && ch != L' ') {

				//
				//	Ensure the surface is locked
				//
				if (LockedPtr == nullptr) {
					LockedPtr = (uint16 *)CurSurface->Lock (&LockedStride);
					WWASSERT (LockedPtr != nullptr);
				}

				//
				//	Check to ensure the text will fit on this texture
				//
				WWASSERT (((TextureOffset.I + char_spacing) < CurrTextureSize) && ((TextureOffset.J + char_height) < CurrTextureSize));

				//
				//	Blit the character to the surface
				//
				if(!dontBlit)
					Font->Blit_Char (ch, LockedPtr, LockedStride, TextureOffset.I, TextureOffset.J);

				if (dontBlit) {
					// we don't blit for a hot key character.  So add extra spacing.
					char_spacing += Font->Get_Extra_Overlap();
					// Brutal hack #27 Gamma - Bolded M's are just a problem.	jba.
					if (ch=='M') {
						char_spacing++;
					}
				}

				TextureOffset.I += char_spacing;
			}
		}
		//
		// reset our cursor and add a line of text to the cursor position
		//
		Cursor.X = 0;
		Cursor.Y += char_height;
		line_width = 0;
		}

		if(hkX)
			*hkX = hotKeyPosX;
		if(hkX)
			*hkY = hotKeyPosY;
}
////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Sentence_NotCentered
//
////////////////////////////////////////////////////////////////////////////////////
Vector2	Render2DSentenceClass::Build_Sentence_Not_Centered (const WCHAR *text, int *hkX, int *hkY, bool justCalcExtents)
{
	Vector2 cursor = Cursor;
	int textureStartX = TextureStartX;
	float maxX = 0;

	int hotKeyPosX = 0;
	int hotKeyPosY = 0;
	bool calcHotKeyX = false;
	bool dontBlit = false;
	Vector2i textureOffset = TextureOffset;


	//
	//	Start fresh
	//
	if (!justCalcExtents)
	{
		Reset_Sentence_Data ();
	}
	Cursor.Set (0, 0);

	//
	//	Ensure we have a surface to start with
	//
	if (CurSurface == nullptr) {
		Allocate_New_Surface (text, justCalcExtents);
	}

	TextureOffset.Set (TEXTURE_OFFSET, 0);
	TextureStartX = TEXTURE_OFFSET;

	float char_height = Font->Get_Char_Height ();

	//
	//	Loop over all the characters in the string
	//
	while (text != nullptr) {
		WCHAR ch = *text++;
		dontBlit = false;
		//
		//	Determine how much horizontal space this character requires
		//
		if(ParseHotKey && (ch == L'&') && (*text != 0) && (*text > L' ') && (*text != L'\n'))
		{
				hotKeyPosY = Cursor.Y;
			if (calcHotKeyX)
				hotKeyPosX = 0;
			else
				hotKeyPosX = Cursor.X + TextureOffset.I -TextureStartX;//TextureOffset.I;

			ch = *text++;
			dontBlit = true;
		}
		float char_spacing = Font->Get_Char_Spacing (ch);

		bool exceeded_texture_width	= ((TextureOffset.I + char_spacing) >= CurrTextureSize);
		bool encountered_break_char	= (ch == L' ' || ch == L'\n' || ch == 0);
		bool wordBiggerThenLine = ((useHardWordWrap) && ( WrapWidth != 0 ) &&((Cursor.X + TextureOffset.I -TextureStartX + char_spacing) >= WrapWidth));
		//
		//	Do we need to record this portion of the sentence to its own chunk?
		//
		if (exceeded_texture_width || encountered_break_char|| wordBiggerThenLine) {
			if (!justCalcExtents)
			{
				Record_Sentence_Chunk ();
			}

			//
			//	Adjust the positions
			//
			Cursor.X			+= (TextureOffset.I - TextureStartX);
			maxX = max(maxX, Cursor.X);
			TextureStartX	= TextureOffset.I;

			//
			//	Adjust the output coordinates
			//
			if (ch == L' ') {
				//Cursor.X += char_spacing;
				//maxX = max(maxX, Cursor.X);

				//
				// Check to see if we need to wrap on this word-break
				//
				if (WrapWidth > 0) {

					//
					//	Find the length of the next word
					//
					const WCHAR *word	= text;
					float word_width	= char_spacing;
					while ((*word != 0) && (*word > L' ')) {
						if(ParseHotKey && (*word == L'&') && (*word+1 != 0) && (*word+1 > L' ') && (*word+1 != L'\n'))
							*word++;
						word_width += Font->Get_Char_Spacing (*word++);
					}

					//
					//	Should we wrap the next word?
					//
					if ((Cursor.X + word_width) >= WrapWidth) {
						Cursor.X = 0;
						Cursor.Y += char_height;
						calcHotKeyX = true;
					}
				}

			} else if (ch == L'\n') {
				Cursor.X = 0;
				Cursor.Y += char_height;
			} else if (ch == 0) {
				break;
			} else if (wordBiggerThenLine){ // we've entered this loop because we're greater then the wordwrap so we need to force a wordwrap
				Cursor.X = 0;
				Cursor.Y += char_height;
			}


			//
			//	Did the text extend past the edge of the texture?
			//
			if (exceeded_texture_width) {
				TextureStartX		= TEXTURE_OFFSET;
				TextureOffset.I	= TextureStartX;
				TextureOffset.J	+= char_height;

				//
				//	Did the text extent completely off the texture?
				//
				if ((TextureOffset.J + char_height) >= CurrTextureSize) {
					Allocate_New_Surface (text, justCalcExtents);
				}
			}
		}

		if (ch != L'\n' ) {

			//
			//	Ensure the surface is locked
			//
			if (!justCalcExtents)
			{
				if (LockedPtr == nullptr) {
					LockedPtr = (uint16 *)CurSurface->Lock (&LockedStride);
					WWASSERT (LockedPtr != nullptr);
				}
			}

			//
			//	Check to ensure the text will fit on this texture
			//
			WWASSERT (((TextureOffset.I + char_spacing) < CurrTextureSize) && ((TextureOffset.J + char_height) < CurrTextureSize));

			//
			//	Blit the character to the surface
			//
			if (!justCalcExtents && !dontBlit )
			{
				Font->Blit_Char (ch, LockedPtr, LockedStride, TextureOffset.I, TextureOffset.J);
			}
			TextureOffset.I += char_spacing;
		}
	}

	Vector2 extent;
	extent.X = maxX + Font->Get_Extra_Overlap();
	extent.Y = Cursor.Y + char_height;

	Cursor = cursor;
	TextureOffset = textureOffset;
	TextureStartX = textureStartX;

	if(hkX)
		*hkX = hotKeyPosX;
	if(hkX)
		*hkY = hotKeyPosY;

	return extent;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Sentence
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Build_Sentence (const WCHAR *text, int *hkX, int *hkY)
{
	if (text == nullptr) {
		return ;
	}

	if (Font == nullptr)
		return;

	if(Centered && (WrapWidth > 0 || wcschr(text,L'\n')))
		Build_Sentence_Centered(text, hkX, hkY);
	else
		Build_Sentence_Not_Centered(text, hkX, hkY);

}


////////////////////////////////////////////////////////////////////////////////////
//
//	FontCharsClass
//
////////////////////////////////////////////////////////////////////////////////////
FontCharsClass::FontCharsClass () :
#ifdef _WIN32
	OldGDIFont(	nullptr ),
	OldGDIBitmap( nullptr ),
	GDIFont( nullptr ),
	GDIBitmap( nullptr ),
	GDIBitmapBits ( nullptr ),
	MemDC( nullptr ),
#endif
#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
	FTLibrary( nullptr ),
	FTFace( nullptr ),
	FreetypeFaceIndex( 0 ),
#endif
	CurrPixelOffset( 0 ),
	PointSize( 0 ),
	CharHeight( 0 ),
	UnicodeCharArray( nullptr ),
	FirstUnicodeChar( 0xFFFF ),
	LastUnicodeChar( 0 ),
	IsBold (false)
{
	AlternateUnicodeFont = nullptr;
	::memset( ASCIICharArray, 0, sizeof (ASCIICharArray) );
}


////////////////////////////////////////////////////////////////////////////////////
//
//	~FontCharsClass
//
////////////////////////////////////////////////////////////////////////////////////
FontCharsClass::~FontCharsClass ()
{
	while ( BufferList.Count() ) {
		delete BufferList[0];
		BufferList.Delete(0);
	}

#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
	Free_Freetype_Font();
#else
	Free_GDI_Font();
#endif
	Free_Character_Arrays();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Char_Data
//
////////////////////////////////////////////////////////////////////////////////////
const FontCharsClassCharDataStruct *
FontCharsClass::Get_Char_Data (WCHAR ch)
{
	// GeneralsX @bugfix BenderAI/felipebraz 14/03/2026 Normalize to 16-bit code units to match legacy font array indexing on non-Windows wchar_t
	const uint16 normalized_char = static_cast<uint16>(ch);
	const WCHAR glyph = static_cast<WCHAR>(normalized_char);

	const FontCharsClassCharDataStruct *retval = nullptr;

	if ( normalized_char < 256 )
	{
		retval = ASCIICharArray[normalized_char];
	}
 	else if ( AlternateUnicodeFont && this != AlternateUnicodeFont )
	{
		return AlternateUnicodeFont->Get_Char_Data( glyph );
	}
	else
	{
		Grow_Unicode_Array( glyph );
		retval = UnicodeCharArray[normalized_char - FirstUnicodeChar];
	}

	//
	//	If the character wasn't found, then add it to our list
	//  TheSuperHackers @feature FreeType port 10/02/2026 Dispatch to FreeType on Linux
	//
	if ( retval == nullptr ) {
#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
		retval = Store_Freetype_Char( glyph );
#else
		retval = Store_GDI_Char( glyph );
#endif
	}

	WWASSERT( retval->Value == glyph );
	return retval;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Char_Width
//
////////////////////////////////////////////////////////////////////////////////////
int
FontCharsClass::Get_Char_Width (WCHAR ch)
{
	const FontCharsClassCharDataStruct	* data = Get_Char_Data( ch );
	if ( data != nullptr ) {
		return data->Width;
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Char_Spacing
//
////////////////////////////////////////////////////////////////////////////////////
int
FontCharsClass::Get_Char_Spacing (WCHAR ch)
{
	const FontCharsClassCharDataStruct	* data = Get_Char_Data( ch );
	if ( data != nullptr ) {
		if ( data->Width != 0 ) {
			return data->Width - PixelOverlap - CharOverhang;
		}
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Blit_Char
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Blit_Char (WCHAR ch, uint16 *dest_ptr, int dest_stride, int x, int y)
{
	const FontCharsClassCharDataStruct	* data = Get_Char_Data( ch );
	if ( data != nullptr && data->Width != 0 ) {

		//
		//	Setup the src and destination pointers
		//
		int dest_inc		= (dest_stride >> 1);
		uint16 *src_ptr	= data->Buffer;
		dest_ptr				+= (dest_inc * y) + x;

		//
		//	Simply copy the data from the src buffer to the destination
		//
		for ( int row = 0; row < CharHeight; row ++ ) {
			for ( int col = 0; col < data->Width; col ++ ) {
				uint16 curData = *src_ptr;
				if (col<PixelOverlap) {
					curData |= dest_ptr[col];
				}
				dest_ptr[col] = curData;
				src_ptr++;
			}
			dest_ptr	+= dest_inc;
		}
	}
}


#ifdef _WIN32

////////////////////////////////////////////////////////////////////////////////////
//
//	Store_GDI_Char
//
// GeneralsX @build fbraz 11/02/2026 - Windows-only GDI text rendering
////////////////////////////////////////////////////////////////////////////////////
const FontCharsClassCharDataStruct *
FontCharsClass::Store_GDI_Char (WCHAR ch)
{
	int width	= PointSize * 2;
	int height	= PointSize * 2;

	//
	//	Draw the character into the memory DC
	//
	RECT rect = { 0, 0, width, height };
	int xOrigin = 0;
	if (ch == 'W') {
		xOrigin = 1;
	}
	::ExtTextOutW( MemDC, xOrigin, 0, ETO_OPAQUE, &rect, &ch, 1, nullptr);

	//
	//	Get the size of the character we just drew
	//
	SIZE char_size = { 0 };
	::GetTextExtentPoint32W( MemDC, &ch, 1, &char_size );
	char_size.cx += PixelOverlap + xOrigin;
	//
	//	Get a pointer to the surface that this character should use
	//
	Update_Current_Buffer( char_size.cx );
	uint16* curr_buffer_p = BufferList[BufferList.Count () - 1]->Buffer;
	curr_buffer_p += CurrPixelOffset;

	//
	//	Copy the BMP contents to the buffer
	//
	int stride = (((width * 3) + 3) & ~3);
	for (int row = 0; row < char_size.cy; row ++) {

		//
		//	Compute the indices into the BMP and surface
		//
		int index = (row * stride);

		//
		//	Loop over each column
		//
		for (int col = 0; col < char_size.cx; col ++) {

			//
			//	Get the pixel color at this location
			//
			uint8 pixel_value = GDIBitmapBits[index];
			index += 3;
#ifdef TEST_PLACEMENT
 			if (row==CharHeight-1&&col==0) {
 				pixel_value = 0xff;
 			}
 			if (row==CharHeight-2&&col==1) {
 				pixel_value = 0xff;
 			}
 			if (row==0&&col==0) {
 				pixel_value = 0xff;
 			}
 			if (row==1&&col==1) {
 				pixel_value = 0xff;
 			}
 			if (row==CharHeight-1&&col==char_size.cx-1-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (row==CharHeight-2&&col==char_size.cx-2-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (row==0&&col==char_size.cx-1-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (row==1&&col==char_size.cx-2-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (pixel_value == 0x00) {
 				pixel_value = 0x40;
 			}
#endif

			uint16 pixel_color = 0;
			if (pixel_value != 0) {
				pixel_color = 0x0FFF;
			}

			//
			//	Convert the pixel intensity from 8bit to 4bit and
			// store it in our buffer
			//
			uint8 alpha_value	= ((pixel_value >> 4) & 0xF);
			*curr_buffer_p++	= pixel_color | (alpha_value << 12);
		}
	}

	//
	//	Save information about this character in our list
	//
	FontCharsClassCharDataStruct *char_data	= W3DNEW FontCharsClassCharDataStruct;
	char_data->Value				= ch;
	char_data->Width				= char_size.cx;
	char_data->Buffer				= BufferList[BufferList.Count () - 1]->Buffer + CurrPixelOffset;

	//
	//	Insert this character into our array
	//
	if ( ch < 256 ) {
		ASCIICharArray[ch] = char_data;
	} else {
		UnicodeCharArray[ch - FirstUnicodeChar] = char_data;
	}

	//
	//	Advance the character position
	//
	CurrPixelOffset += ((char_size.cx+PixelOverlap) * CharHeight);

	//
	//	Return the index of the entry we just added
	//
	return char_data;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Create_GDI_Font
//
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Create_GDI_Font (const char *font_name)
{
	HDC screen_dc = ::GetDC ((HWND)WW3D::Get_Window());

	const char *fontToUseForGenerals = "Arial";
	bool doingGenerals = false;
	if (strcmp(font_name, "Generals")==0) {
		font_name = fontToUseForGenerals;
		doingGenerals = true;
	}

	//
	//	Calculate the height of the font in logical units
	//
	const int dotsPerInch = 96; // always use 96.	jba.
	int font_height = -MulDiv (PointSize, dotsPerInch, 72);

	int fontWidth = 0; // use font default.
	if (doingGenerals) {
		//fontWidth = -font_height*0.35f; //2 pixels tighter.
		fontWidth = -font_height*0.40f; // one pixel tighter
	}
	PixelOverlap = (-font_height)/8;

	// Sanity check in case of perversion. :)
	if (PixelOverlap<0) PixelOverlap = 0;
	if (PixelOverlap>4) PixelOverlap = 4;
	//
	//	Create the Windows font
	//
	DWORD bold		= IsBold ? FW_BOLD : FW_NORMAL;
	DWORD italic	= 0;
	GDIFont			= ::CreateFont (font_height, fontWidth, 0, 0, bold, italic,
								FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
								CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
								VARIABLE_PITCH, font_name);

	//
	// Set-up the fields of the BITMAPINFOHEADER
	//	Note: Top-down DIBs use negative height in Win32.
	//
	BITMAPINFOHEADER bitmap_info = { 0 };
	bitmap_info.biSize				= sizeof (BITMAPINFOHEADER);
	bitmap_info.biWidth				= PointSize * 2;
	bitmap_info.biHeight				= -(PointSize * 2);
	bitmap_info.biPlanes				= 1;
	bitmap_info.biBitCount			= 24;
	bitmap_info.biCompression		= BI_RGB;
	bitmap_info.biSizeImage			= ((PointSize * PointSize * 4) * 3);
	bitmap_info.biXPelsPerMeter	= 0;
	bitmap_info.biYPelsPerMeter	= 0;
	bitmap_info.biClrUsed			= 0;
	bitmap_info.biClrImportant		= 0;

	//
	// Create a bitmap that we can access the bits directly of
	//
	GDIBitmap	= ::CreateDIBSection (	screen_dc,
													(const BITMAPINFO *)&bitmap_info,
													DIB_RGB_COLORS,
													(void **)&GDIBitmapBits,
													nullptr,
													0L);

	//
	//	Create a device context we can select the font and bitmap into
	//
	MemDC = ::CreateCompatibleDC (screen_dc);

	//
	// Release our temporary screen DC
	//
	::ReleaseDC ((HWND)WW3D::Get_Window(), screen_dc);

	//
	//	Now select the BMP and font into the DC
	//
	OldGDIBitmap	= (HBITMAP)::SelectObject (MemDC, GDIBitmap);
	OldGDIFont		= (HFONT)::SelectObject (MemDC, GDIFont);
	::SetBkColor (MemDC, RGB (0, 0, 0));
	::SetTextColor (MemDC, RGB (255, 255, 255));

	//
	//	Lookup the pixel height of the font
	//
	TEXTMETRIC text_metric = { 0 };
	::GetTextMetrics (MemDC, &text_metric);
	CharHeight = text_metric.tmHeight;
	CharAscent = text_metric.tmAscent;
	CharOverhang = text_metric.tmOverhang;
	if (doingGenerals) {
		CharOverhang = 0;
	}

	return GDIFont != nullptr && GDIBitmap != nullptr;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Free_GDI_Font
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Free_GDI_Font ()
{
	//
	//	Select the old font back into the DC and delete
	// our font object
	//
	if ( GDIFont != nullptr ) {
		::SelectObject( MemDC, OldGDIFont );
		::DeleteObject( GDIFont );
		GDIFont = nullptr;
	}

	//
	//	Select the old bitmap back into the DC and delete
	// our bitmap object
	//
	if ( GDIBitmap != nullptr ) {
		::SelectObject( MemDC, OldGDIBitmap );
		::DeleteObject( GDIBitmap );
		GDIBitmap = nullptr;
	}

	//
	//	Delete our memory DC
	//
	if ( MemDC != nullptr ) {
		::DeleteDC( MemDC );
		MemDC = nullptr;
	}
}

#endif // _WIN32

////////////////////////////////////////////////////////////////////////////////////
//
//	Update_Current_Buffer (Platform-independent text buffer management)
//
// GeneralsX @build fbraz 11/02/2026 - Used by both Windows GDI and Linux FreeType
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Update_Current_Buffer (int char_width)
{
	//
	//	Check to see if we need to allocate a new buffer
	//
	bool needs_new_buffer = (BufferList.Count () == 0);
	if (needs_new_buffer == false) {

		//
		//	Would we extend past this buffer?
		//
		if ( (CurrPixelOffset + (char_width * CharHeight)) > CHAR_BUFFER_LEN ) {
			needs_new_buffer = true;
		}
	}

	//
	//	Do we need to create a new surface?
	//
	if (needs_new_buffer)
	{
		FontCharsBuffer* new_buffer = W3DNEW FontCharsBuffer;
		BufferList.Add( new_buffer );
		CurrPixelOffset = 0;
	}

	return ;
}

#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE

#include <cctype>
#include <cstdio>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////////
//
//	Locate_Font_FontConfig (iOS)
//
// iOS has no fontconfig and no user-accessible system font files. Fonts are
// resolved from a "fonts" directory below the current working directory (the
// app's Documents folder, where game data also lives). The requested face name
// is normalized (lowercase, spaces stripped) and tried as <name>.ttf/.otf/.ttc;
// arial.ttf serves as the universal fallback since the game UI is Arial-based.
////////////////////////////////////////////////////////////////////////////////////
const char *
FontCharsClass::Locate_Font_FontConfig (const char *font_name)
{
	char normalized[128];
	int n = 0;
	for ( const char *p = font_name; *p != '\0' && n < (int)sizeof(normalized) - 1; ++p ) {
		if ( *p == ' ' ) {
			continue;
		}
		normalized[n++] = (char)tolower( (unsigned char)*p );
	}
	normalized[n] = '\0';

	static const char *extensions[] = { ".ttf", ".otf", ".ttc" };
	char candidate[256];
	for ( size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i ) {
		snprintf( candidate, sizeof(candidate), "fonts/%s%s", normalized, extensions[i] );
		if ( access( candidate, R_OK ) == 0 ) {
			FreetypeFontPath = candidate;
			return FreetypeFontPath;
		}
	}

	// Fall back to the Arial-equivalent face shipped with the app
	if ( access( "fonts/arial.ttf", R_OK ) == 0 ) {
		FreetypeFontPath = "fonts/arial.ttf";
		return FreetypeFontPath;
	}

	return nullptr;
}

#else // !TARGET_OS_IPHONE

////////////////////////////////////////////////////////////////////////////////////
//
//	CJK serif ("Song") font resolution
//
// GeneralsX @feature 10/08/2026 Resolve the Chinese UI font at runtime instead of relying on
// a font the project is not allowed to redistribute.
//
// THE PROBLEM. Language.ini asks for the family by its Chinese name:
//
//     UnicodeFontName = <CB CE CC E5>          ; "SongTi", GBK-encoded
//
// Three separate things then go wrong on a non-Windows host:
//
//  1. ENCODING. Language.ini is GBK (the retail Simplified Chinese file, CRLF, 2003). The
//     engine hands those bytes to FcNameParse() verbatim -- nothing in the language or font
//     path converts them, verified by grep. Fontconfig expects UTF-8, so it parses garbage.
//
//  2. NO FAILURE SIGNAL. FcFontMatch() never reports "not found": it returns the best
//     remaining candidate. With an unparseable family that is the default sans -- PingFang on
//     macOS. So the CJK UI silently rendered in a sans-serif face while the code path looked
//     like it had succeeded. Even a correct UTF-8 request misses, because the macOS family is
//     "Songti SC" and its Chinese alias is a different string than the one the INI asks for.
//
//  3. FACE INDEX. Font collections do not put Regular first. macOS Songti.ttc face 0 is
//     "Songti SC Black"; Regular is face 6. Loading face 0 renders every glyph in the
//     heaviest weight available.
//
// THE FIX. Recognize the family aliases ourselves, in both encodings, and resolve them to a
// concrete (file, face index) pair that is verified to be a serif face with CJK coverage.
//
// A candidate must pass all three checks, so a mislabelled font cannot slip through:
//   - it has a Unicode charmap;
//   - it actually contains CJK ideographs (U+6C49, U+4E2D, U+56FD);
//   - its OS/2 PANOSE serif style is not one of the sans values (11-15).
//
// That last check is load-bearing. A font file named simsun.ttc that declares the family
// "SimSun" is not necessarily SimSun -- font packs circulate in which every Windows filename
// contains the same sans-serif outlines. PANOSE serif style 11 ("Normal Sans") is how such a
// file identifies itself, whereas genuine SimSun reports 1 ("No Fit") and Songti SC reports 1
// and Noto Serif CJK SC reports 2 ("Cove"). Rejecting sans here is what keeps a Song request
// from being answered with a Hei face.
//
// Priority order, highest first:
//   0. GX_CJK_SERIF_FONT -- explicit override, a file path or a family name.
//   1. fonts/ under the runtime directory -- the Song font the deploy script fetched, or one the
//      user dropped in themselves.
//   2. The host's own Song face -- Songti SC / STSong on macOS, SimSun on Windows.
//   3. An open-licensed Song family via fontconfig -- Source Han Serif / Noto Serif CJK.
//
// Nothing is installed system-wide by this code: it only ever points FreeType at a file that is
// already on disk, whether the host shipped it, the deploy script fetched it into the runtime, or
// the user chose to drop it in. Steps 2 and 3 can both come up empty on a machine with no Song
// font at all, which is why the deploy script puts one in fonts/ rather than trusting the search.
////////////////////////////////////////////////////////////////////////////////////

namespace
{

struct GXSongFace
{
	StringClass	Path;
	int			FaceIndex;
	bool		Resolved;
	GXSongFace() : FaceIndex( 0 ), Resolved( false ) {}
};

//
//	Is this request for a "Song" family, under any of the names it goes by?
//
// The GBK spellings are the ones that actually arrive from a retail Language.ini; the UTF-8
// spellings cover a hand-edited or re-encoded file. Both are written as escapes so this file
// stays pure ASCII and cannot be corrupted by an editor guessing an encoding.
static bool GX_Is_Song_Family( const char *name )
{
	if ( name == nullptr ) {
		return false;
	}

	static const char *kSongAliases[] = {
		"SimSun",
		"NSimSun",
		"Songti SC",
		"Songti",
		"STSong",
		"\xCB\xCE\xCC\xE5",					// GBK "SongTi"
		"\xD0\xC2\xCB\xCE\xCC\xE5",			// GBK "XinSongTi" (NSimSun)
		"\xE5\xAE\x8B\xE4\xBD\x93",			// UTF-8 "SongTi"
		"\xE6\x96\xB0\xE5\xAE\x8B\xE4\xBD\x93",	// UTF-8 "XinSongTi"
	};

	for ( size_t i = 0; i < sizeof(kSongAliases) / sizeof(kSongAliases[0]); ++i ) {
		if ( strcasecmp( name, kSongAliases[i] ) == 0 ) {
			return true;
		}
	}
	return false;
}

//
//	Can this face render Chinese, and is it the kind of design we asked for?
//
// GeneralsX @tweak 10/08/2026 The serif test is now optional. Rejecting sans faces is right when
// we are guessing (it is what catches a sans face wearing the filename "simsun.ttc"), but wrong
// when the user named a font explicitly: PingFang SC is PANOSE serifStyle 11, so applying the
// serif test to an override made it impossible to select on purpose. Coverage is always required;
// a face with no ideographs is useless whoever asked for it.
static bool GX_Face_Is_Usable_CJK( FT_Face face, bool require_serif )
{
	if ( face == nullptr ) {
		return false;
	}
	if ( FT_Select_Charmap( face, FT_ENCODING_UNICODE ) != 0 ) {
		return false;
	}

	static const FT_ULong kProbe[] = { 0x6C49, 0x4E2D, 0x56FD };	// Han, Zhong, Guo
	for ( size_t i = 0; i < sizeof(kProbe) / sizeof(kProbe[0]); ++i ) {
		if ( FT_Get_Char_Index( face, kProbe[i] ) == 0 ) {
			return false;
		}
	}

	if ( require_serif ) {
		const TT_OS2 *os2 = (const TT_OS2 *)FT_Get_Sfnt_Table( face, FT_SFNT_OS2 );
		if ( os2 != nullptr && os2->version != 0xFFFF ) {
			const FT_Byte serif_style = os2->panose[1];
			if ( serif_style >= 11 && serif_style <= 15 ) {
				return false;	// Normal/Obtuse/Perp sans, Flared, Rounded
			}
		}
	}
	return true;
}

//
//	Pick the best face inside a (possibly collection) font file.
//
// Scored rather than first-match: a collection can hold several usable faces and we want the
// Regular weight of the preferred family, not merely the first face that parses.
static int GX_Best_Face_In_File( FT_Library lib, const char *path, const char *preferred_family,
	bool require_serif )
{
	FT_Face probe = nullptr;
	if ( FT_New_Face( lib, path, 0, &probe ) != 0 ) {
		return -1;
	}
	const long face_count = probe->num_faces > 0 ? probe->num_faces : 1;
	FT_Done_Face( probe );

	int best_index = -1;
	int best_score = -1;
	for ( long i = 0; i < face_count; ++i ) {
		FT_Face face = nullptr;
		if ( FT_New_Face( lib, path, i, &face ) != 0 ) {
			continue;
		}

		if ( GX_Face_Is_Usable_CJK( face, require_serif ) ) {
			int score = 0;
			const char *family = face->family_name != nullptr ? face->family_name : "";
			const char *style  = face->style_name  != nullptr ? face->style_name  : "";

			if ( preferred_family != nullptr && strcasecmp( family, preferred_family ) == 0 ) {
				score += 4;
			}
			if ( strcasecmp( style, "Regular" ) == 0 ) {
				score += 2;
			}
			// Reject nothing outright on weight, but prefer an unstyled face.
			if ( (face->style_flags & (FT_STYLE_FLAG_BOLD | FT_STYLE_FLAG_ITALIC)) == 0 ) {
				score += 1;
			}

			if ( score > best_score ) {
				best_score = score;
				best_index = (int)i;
			}
		}
		FT_Done_Face( face );
	}
	return best_index;
}

//
//	Ask fontconfig for a family, and only accept it if it really is that family.
//
// FcFontMatch() always answers, so the returned family must be compared against the request.
// Without that check every miss would look like a hit and resolve to the default sans.
static bool GX_Resolve_Family_Via_FontConfig( FT_Library lib, const char *family,
	StringClass &out_path, int &out_index, bool require_serif )
{
	FcPattern *pattern = FcNameParse( (const FcChar8 *)family );
	if ( pattern == nullptr ) {
		return false;
	}
	FcPatternAddInteger( pattern, FC_WEIGHT, FC_WEIGHT_REGULAR );
	FcPatternAddInteger( pattern, FC_SLANT, FC_SLANT_ROMAN );
	FcConfigSubstitute( nullptr, pattern, FcMatchPattern );
	FcDefaultSubstitute( pattern );

	FcResult result = FcResultNoMatch;
	FcPattern *match = FcFontMatch( nullptr, pattern, &result );
	bool ok = false;

	if ( match != nullptr && result == FcResultMatch ) {
		FcChar8 *file = nullptr;
		FcChar8 *got_family = nullptr;
		int index = 0;
		if ( FcPatternGetString( match, FC_FILE, 0, &file ) == FcResultMatch && file != nullptr ) {
			if ( FcPatternGetInteger( match, FC_INDEX, 0, &index ) != FcResultMatch ) {
				index = 0;
			}

			// Fontconfig substituted something else -- treat as a miss, not a match.
			//
			// GeneralsX @bugfix 10/08/2026 Every FC_FAMILY value has to be checked, not just the
			// first. A font carries one name per language and fontconfig returns them in the
			// system's order, so on a Chinese-locale machine index 0 of PingFang SC is the
			// localized "苹方-简" and comparing against only that rejected a perfectly good match.
			bool family_matches = false;
			for ( int n = 0; FcPatternGetString( match, FC_FAMILY, n, &got_family ) == FcResultMatch; ++n ) {
				if ( got_family != nullptr && strcasecmp( (const char *)got_family, family ) == 0 ) {
					family_matches = true;
					break;
				}
			}

			if ( family_matches ) {
				FT_Face face = nullptr;
				if ( FT_New_Face( lib, (const char *)file, index, &face ) == 0 ) {
					if ( GX_Face_Is_Usable_CJK( face, require_serif ) ) {
						out_path = (const char *)file;
						out_index = index;
						ok = true;
					}
					FT_Done_Face( face );
				}
			}
		}
		FcPatternDestroy( match );
	}
	FcPatternDestroy( pattern );
	return ok;
}

//
//	Resolve once per process and remember the answer.
//
// Scanning Songti.ttc means opening a 64 MB collection and walking eight faces; the font
// library asks for the same family once per (size, weight) pair, so this must not repeat.
static const GXSongFace & GX_Get_Song_Face( FT_Library lib )
{
	static GXSongFace s_face;
	static bool s_tried = false;
	if ( s_tried ) {
		return s_face;
	}
	s_tried = true;

	// 0. Explicit override. A path wins outright; anything else is treated as a family name.
	//
	// GeneralsX @tweak 10/08/2026 An override is not held to the serif test. Naming a font is a
	// deliberate act, so a sans choice such as "PingFang SC" is honoured; only CJK coverage is
	// enforced. Prefer a family name over a path: macOS keeps PingFang under an AssetsV2 path
	// with a content hash in it, which changes across OS updates.
	const char *override_name = getenv( "GX_CJK_SERIF_FONT" );
	if ( override_name != nullptr && override_name[0] != '\0' ) {
		if ( access( override_name, R_OK ) == 0 ) {
			const int idx = GX_Best_Face_In_File( lib, override_name, nullptr, false );
			if ( idx >= 0 ) {
				s_face.Path = override_name;
				s_face.FaceIndex = idx;
				s_face.Resolved = true;
				fprintf( stderr, "INFO: GX CJK font: override file %s (face %d)\n",
					override_name, idx );
				return s_face;
			}
			fprintf( stderr, "WARNING: GX_CJK_SERIF_FONT=%s has no face with Chinese coverage; ignoring\n",
				override_name );
		} else {
			StringClass p;
			int idx = 0;
			if ( GX_Resolve_Family_Via_FontConfig( lib, override_name, p, idx, false ) ) {
				s_face.Path = p;
				s_face.FaceIndex = idx;
				s_face.Resolved = true;
				fprintf( stderr, "INFO: GX CJK font: override family '%s' -> %s (face %d)\n",
					override_name, p.str(), idx );
				return s_face;
			}
			fprintf( stderr, "WARNING: GX_CJK_SERIF_FONT='%s' did not resolve to a font with Chinese "
				"coverage (check the exact family name with: fc-list :lang=zh family); ignoring\n",
				override_name );
		}
	}

	struct FileCandidate { const char *path; const char *family; };

	// 1. A font in the runtime's own fonts directory. run.sh sets cwd to the runtime directory.
	//
	// Two kinds of entry live here. The Song filenames are what a user drops in by hand, and their
	// case variants are listed because Linux is case-sensitive while a font copied off Windows may
	// keep either spelling; on a case-insensitive volume they collapse to one file, so results are
	// deduplicated by real path to avoid probing the same file twice.
	//
	// GeneralsX @feature 10/08/2026 The Noto entry is different: it is what deploy-macos-zh.sh
	// fetches, so a fresh install has a known Song face without depending on what the host happens
	// to own. That is the whole point of shipping it -- the search below this can find nothing on a
	// machine with no Song font installed, and Chinese text then falls back to a sans face or to
	// hollow boxes. Listing it here, at the same priority as a hand-dropped font and above anything
	// on the system, makes the deployed result identical everywhere. A user who prefers the host's
	// own Songti SC still wins by naming it in gx-font.conf, which is checked before all of this.
	//
	// The family hint is per-entry because it only scores candidate faces; it filters nothing, so a
	// wrong hint would silently cost the correct face its ranking inside a multi-face file.
	static const FileCandidate kUserPaths[] = {
		{ "fonts/NotoSerifSC-Regular.otf",	"Noto Serif SC" },
		{ "fonts/song.otf",					"Noto Serif SC" },
		{ "fonts/simsun.ttc",				"SimSun" },
		{ "fonts/simsun.ttf",				"SimSun" },
		{ "fonts/SimSun.ttc",				"SimSun" },
		{ "fonts/songti.ttc",				"Songti SC" },
		{ "fonts/Songti.ttc",				"Songti SC" },
	};
	StringClass seen_paths[sizeof(kUserPaths) / sizeof(kUserPaths[0])];
	int seen_count = 0;
	for ( size_t i = 0; i < sizeof(kUserPaths) / sizeof(kUserPaths[0]); ++i ) {
		if ( access( kUserPaths[i].path, R_OK ) != 0 ) {
			continue;
		}

		char real_buf[PATH_MAX];
		const char *canonical = realpath( kUserPaths[i].path, real_buf );
		if ( canonical != nullptr ) {
			bool already_seen = false;
			for ( int s = 0; s < seen_count; ++s ) {
				if ( strcmp( seen_paths[s].str(), canonical ) == 0 ) {
					already_seen = true;
					break;
				}
			}
			if ( already_seen ) {
				continue;
			}
			seen_paths[seen_count++] = canonical;
		}
		const int idx = GX_Best_Face_In_File( lib, kUserPaths[i].path, kUserPaths[i].family, true );
		if ( idx >= 0 ) {
			s_face.Path = kUserPaths[i].path;
			s_face.FaceIndex = idx;
			s_face.Resolved = true;
			fprintf( stderr, "INFO: GX CJK serif font: runtime fonts/ %s (face %d)\n",
				kUserPaths[i].path, idx );
			return s_face;
		}
		// Present but unusable: almost always a sans face wearing a Song filename.
		fprintf( stderr, "WARNING: GX CJK serif font: %s has no usable serif CJK face "
			"(sans-serif PANOSE or missing ideographs); falling back to a system font\n",
			kUserPaths[i].path );
	}

	// 2. The host's own Song face, by file, before asking fontconfig anything.
	static const FileCandidate kSystemFiles[] = {
		// macOS
		{ "/System/Library/Fonts/Supplemental/Songti.ttc",	"Songti SC" },
		{ "/Library/Fonts/Songti.ttc",						"Songti SC" },
		{ "/System/Library/Fonts/Songti.ttc",				"Songti SC" },
		// Windows (Wine prefixes and case-preserving mounts included)
		{ "C:/Windows/Fonts/simsun.ttc",					"SimSun" },
		{ "C:/Windows/Fonts/SimSun.ttc",					"SimSun" },
		{ "/mnt/c/Windows/Fonts/simsun.ttc",				"SimSun" },
	};
	for ( size_t i = 0; i < sizeof(kSystemFiles) / sizeof(kSystemFiles[0]); ++i ) {
		if ( access( kSystemFiles[i].path, R_OK ) != 0 ) {
			continue;
		}
		const int idx = GX_Best_Face_In_File( lib, kSystemFiles[i].path, kSystemFiles[i].family, true );
		if ( idx >= 0 ) {
			s_face.Path = kSystemFiles[i].path;
			s_face.FaceIndex = idx;
			s_face.Resolved = true;
			fprintf( stderr, "INFO: GX CJK serif font: system %s (face %d, %s)\n",
				kSystemFiles[i].path, idx, kSystemFiles[i].family );
			return s_face;
		}
	}

	// 3. Open-licensed Song families, whatever the host happens to have installed.
	static const char *kFamilies[] = {
		"Songti SC",
		"SimSun",
		"STSong",
		"Source Han Serif SC",
		"Source Han Serif CN",
		"Noto Serif CJK SC",
		"Noto Serif SC",
		"AR PL UMing CN",
		"AR PL SungtiL GB",
	};
	for ( size_t i = 0; i < sizeof(kFamilies) / sizeof(kFamilies[0]); ++i ) {
		StringClass p;
		int idx = 0;
		if ( GX_Resolve_Family_Via_FontConfig( lib, kFamilies[i], p, idx, true ) ) {
			s_face.Path = p;
			s_face.FaceIndex = idx;
			s_face.Resolved = true;
			fprintf( stderr, "INFO: GX CJK serif font: family '%s' -> %s (face %d)\n",
				kFamilies[i], p.str(), idx );
			return s_face;
		}
	}

	fprintf( stderr, "WARNING: GX CJK serif font: no serif CJK face found. Chinese text will use "
		"whatever fontconfig substitutes, probably a sans face. Re-run the deploy script to fetch "
		"Noto Serif SC into <runtime>/fonts, or drop a Song font there yourself as song.otf or "
		"simsun.ttc, or name a family in gx-font.conf / GX_CJK_SERIF_FONT.\n" );
	return s_face;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////////
//
//	Locate_Font_FontConfig
//
// TheSuperHackers @feature FreeType port 10/02/2026 Locate system font using Fontconfig
// GeneralsX @bugfix 10/08/2026 Intercept CJK "Song" requests, and honour the matched face index.
////////////////////////////////////////////////////////////////////////////////////
const char *
FontCharsClass::Locate_Font_FontConfig (const char *font_name)
{
	//
	//	Default to the first face; only a collection match overrides this.
	//
	FreetypeFaceIndex = 0;

	//
	//	A Song request cannot be answered by fontconfig alone: the name arrives GBK-encoded and
	//	fontconfig substitutes the default sans instead of reporting a miss. Resolve it here.
	//
	if ( GX_Is_Song_Family( font_name ) && FTLibrary != nullptr ) {
		const GXSongFace &song = GX_Get_Song_Face( FTLibrary );
		if ( song.Resolved ) {
			FreetypeFontPath = song.Path;
			FreetypeFaceIndex = song.FaceIndex;
			return FreetypeFontPath;
		}
		// Unresolved: fall through and let fontconfig do whatever it can.
	}

	//
	//
	//	Initialize Fontconfig library
	//
	FcConfig *config = FcInitLoadConfigAndFonts();
	if ( config == nullptr ) {
		return nullptr;
	}

	//
	//	Create a pattern for the requested font
	//
	FcPattern *pattern = FcNameParse( (const FcChar8*)font_name );
	if ( pattern == nullptr ) {
		FcConfigDestroy( config );
		return nullptr;
	}

	//
	//	Configure the pattern
	//
	FcConfigSubstitute( config, pattern, FcMatchPattern );
	FcDefaultSubstitute( pattern );

	//
	//	Find the best match
	//
	FcResult result = FcResultNoMatch;
	FcPattern *font = FcFontMatch( config, pattern, &result );

	const char *font_path = nullptr;
	if ( font != nullptr && result == FcResultMatch ) {
		//
		//	Extract the font file path
		//
		FcChar8 *file_path = nullptr;
		if ( FcPatternGetString( font, FC_FILE, 0, &file_path ) == FcResultMatch ) {
			FreetypeFontPath = (const char*)file_path;
			font_path = FreetypeFontPath;

			// GeneralsX @bugfix 10/08/2026 Carry the matched face index through.
			// Fontconfig indexes each face of a collection separately, so discarding this
			// and loading face 0 can silently pick a different weight than the one matched.
			int face_index = 0;
			if ( FcPatternGetInteger( font, FC_INDEX, 0, &face_index ) == FcResultMatch
				&& face_index > 0 ) {
				FreetypeFaceIndex = face_index;
			}
		}

		FcPatternDestroy( font );
	}

	FcPatternDestroy( pattern );
	FcConfigDestroy( config );

	return font_path;
}

#endif // !TARGET_OS_IPHONE


////////////////////////////////////////////////////////////////////////////////////
//
//	Create_Freetype_Font
//
// GeneralsX @build fbraz 11/02/2026 BenderAI -  Initialize FreeType font (fighter19 pattern)
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Create_Freetype_Font (const char *font_name)
{
	//
	//	Initialize FreeType library
	//
	FT_Error error = FT_Init_FreeType( &FTLibrary );
	if ( error != 0 ) {
		return false;
	}

	//
	//	Handle "Generals" font mapping to Arial
	//
	bool doingGenerals = false;
	if ( strcmp( font_name, "Generals" ) == 0 ) {
		font_name = "Arial";
		doingGenerals = true;
	}

	//
	//	Calculate font height in pixels (96 DPI standard)
	//
	const int dotsPerInch = 96;
	int font_height = FT_MulDiv( PointSize, dotsPerInch, 72 );

	//
	//	Locate the font file using Fontconfig
	//
	const char *font_path = Locate_Font_FontConfig( font_name );
	if ( font_path == nullptr ) {
		FT_Done_FreeType( FTLibrary );
		FTLibrary = nullptr;
		return false;
	}

	//
	//	Load the font face
	//
	// GeneralsX @bugfix 10/08/2026 Load the face Locate_Font_FontConfig actually matched.
	// Was hardcoded to 0, which in a collection is not the Regular weight: macOS Songti.ttc
	// face 0 is "Songti SC Black" and Regular is face 6.
	error = FT_New_Face( FTLibrary, font_path, FreetypeFaceIndex, &FTFace );
	if ( error != 0 ) {
		FT_Done_FreeType( FTLibrary );
		FTLibrary = nullptr;
		return false;
	}

	// Some collections expose a legacy charmap first. Select Unicode explicitly
	// so FT_Get_Char_Index receives the UTF-16 codepoints loaded from the CSF.
	error = FT_Select_Charmap( FTFace, FT_ENCODING_UNICODE );
	if ( error != 0 ) {
		fprintf(stderr, "WARNING: FreeType has no Unicode charmap for font=%s error=%d\n",
			font_name, static_cast<int>(error));
	}

	//
	//	Set the font size (using pixel sizes for simplicity)
	//
	error = FT_Set_Pixel_Sizes( FTFace, 0, font_height );
	if ( error != 0 ) {
		FT_Done_Face( FTFace );
		FT_Done_FreeType( FTLibrary );
		FTFace = nullptr;
		FTLibrary = nullptr;
		return false;
	}

	//
	//	Calculate font metrics (Wine-compatible, same as fighter19)
	//
	if ( FT_IS_SCALABLE( FTFace ) ) {
		CharAscent = FT_MulFix( FTFace->ascender, FTFace->size->metrics.y_scale ) >> 6;
		int descent = -FT_MulFix( FTFace->descender, FTFace->size->metrics.y_scale ) >> 6;

		//
		//	GeneralsX @bugfix: hhea ascender/descender are tuned for Latin glyphs and
		//	are too short for CJK ideographs, which commonly extend close to the full
		//	em box. Widen the cell to the font's declared bbox (when larger) so the
		//	bottom rows of tall glyphs aren't pushed past CharHeight and dropped --
		//	that clipping is what made CJK text look like different characters.
		//
		int bbox_ascent  = FT_MulFix( FTFace->bbox.yMax, FTFace->size->metrics.y_scale ) >> 6;
		int bbox_descent = -FT_MulFix( FTFace->bbox.yMin, FTFace->size->metrics.y_scale ) >> 6;
		if ( bbox_ascent > CharAscent ) CharAscent = bbox_ascent;
		if ( bbox_descent > descent ) descent = bbox_descent;

		CharHeight = CharAscent + descent;
		CharOverhang = 0;
	} else {
		//
		//	Non-scalable fonts not supported
		//
		FT_Done_Face( FTFace );
		FT_Done_FreeType( FTLibrary );
		FTFace = nullptr;
		FTLibrary = nullptr;
		return false;
	}

	if ( doingGenerals ) {
		CharOverhang = 0;
	}

	//
	//	Calculate pixel overlap (same logic as GDI version)
	//
	PixelOverlap = (-font_height) / 8;
	if ( PixelOverlap < 0 ) PixelOverlap = 0;
	if ( PixelOverlap > 4 ) PixelOverlap = 4;

	return true;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Store_Freetype_Char
//
// GeneralsX @build fbraz 11/02/2026 BenderAI - FreeType character rendering (fighter19 pattern)
////////////////////////////////////////////////////////////////////////////////////
const FontCharsClassCharDataStruct *
FontCharsClass::Store_Freetype_Char (WCHAR ch)
{
	//
	//	Get the glyph index for the character
	//
	FT_UInt glyph_index = FT_Get_Char_Index( FTFace, ch );

	//
	//	Load the glyph (without rendering yet)
	//
	FT_Error error = FT_Load_Glyph( FTFace, glyph_index, FT_LOAD_DEFAULT );
	if ( error != 0 ) {
		return nullptr;
	}

	//
	//	Convert to an anti-aliased bitmap
	//
	error = FT_Render_Glyph( FTFace->glyph, FT_RENDER_MODE_NORMAL );
	if ( error != 0 ) {
		return nullptr;
	}

	FT_GlyphSlot glyph = FTFace->glyph;

	//
	//	Calculate X position (special case for 'W')
	//
	int x_pos = 0;
	if ( ch == 'W' ) {
		x_pos = 1;
	}

	//
	//	Calculate character width (advance + overlap)
	//
	unsigned int char_width = glyph->advance.x >> 6;

	//
	//	Sometimes bitmap is wider than advancement (fix it)
	//
	if ( char_width < glyph->bitmap.width + glyph->bitmap_left ) {
		char_width = glyph->bitmap.width + glyph->bitmap_left;
	}
	char_width += PixelOverlap + x_pos;

	//
	//	Get a pointer to the buffer for this character (allocates if needed)
	//
	Update_Current_Buffer( char_width );
	uint16 *curr_buffer_p = BufferList[BufferList.Count() - 1]->Buffer;
	curr_buffer_p += CurrPixelOffset;

	//
	//	Calculate bitmap offsets (match GDI baseline)
	//
	int x_offset = glyph->bitmap_left;
	int descent = CharHeight - CharAscent;
	int y_offset = (CharHeight - glyph->bitmap_top) - descent;

	//
	//	Prevent invalid buffer access
	//
	if ( x_offset < 0 ) x_offset = 0;
	if ( y_offset < 0 ) y_offset = 0;

	//
	//	Copy FreeType bitmap to our buffer (convert 8-bit gray → 16-bit format)
	//
	for ( unsigned int row = 0; row < glyph->bitmap.rows; row++ ) {
		int src_index = row * glyph->bitmap.pitch;
		int dst_index = (y_offset + row) * char_width;

		for ( unsigned int col = 0; col < glyph->bitmap.width; col++ ) {
			//
			//	Get 8-bit grayscale pixel
			//
			uint8 pixel_value = glyph->bitmap.buffer[src_index + col];

			uint16 pixel_color = 0;
			if ( pixel_value != 0 ) {
				pixel_color = 0x0FFF;	// White (12-bit RGB444)
			}

			//
			//	Format: 4-bit alpha (top nibble) + 12-bit color (bottom bits)
			//	SAME FORMAT AS GDI IMPLEMENTATION
			//
			uint8 alpha_value = (pixel_value >> 4) & 0xF;
			curr_buffer_p[dst_index + x_offset + col] = pixel_color | (alpha_value << 12);
		}
	}

	//
	//	Save information about this character
	//
	FontCharsClassCharDataStruct *char_data = W3DNEW FontCharsClassCharDataStruct;
	char_data->Value = ch;
	char_data->Width = (short)char_width;
	char_data->Buffer = BufferList[BufferList.Count() - 1]->Buffer + CurrPixelOffset;

	//
	//	Insert into character array (ASCII or Unicode)
	//
	if ( ch < 256 ) {
		ASCIICharArray[ch] = char_data;
	} else {
		UnicodeCharArray[ch - FirstUnicodeChar] = char_data;
	}

	//
	//	Advance the pixel offset for next character
	//
	CurrPixelOffset += (char_width * CharHeight);

	//
	//	Return the character data
	//
	return char_data;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Free_Freetype_Font
//
// GeneralsX @build fbraz 11/02/2026 BenderAI - Cleanup FreeType resources
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Free_Freetype_Font (void)
{
	//
	//	Free the FreeType face
	//
	if ( FTFace != nullptr ) {
		FT_Done_Face( FTFace );
		FTFace = nullptr;
	}

	//
	//	Free the FreeType library
	//
	if ( FTLibrary != nullptr ) {
		FT_Done_FreeType( FTLibrary );
		FTLibrary = nullptr;
	}
}

#endif // SAGE_USE_FREETYPE && !_WIN32


////////////////////////////////////////////////////////////////////////////////////
//
//	Initialize_GDI_Font
//
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Initialize_GDI_Font (const char *font_name, int point_size, bool is_bold)
{
	//
	//	Build a unique name from the font name and its size
	//
	Name.Format ("%s%d", font_name, point_size);

	//
	//	Remember these settings
	//
	GDIFontName	= font_name;
	PointSize	= point_size;
	IsBold		= is_bold;

	//
	//	Create the actual font object (platform-specific)
	//  TheSuperHackers @feature FreeType port 10/02/2026 Dispatch to FreeType on Linux
	//
#if defined(SAGE_USE_FREETYPE) && !defined(_WIN32)
	return Create_Freetype_Font (font_name);
#else
	return Create_GDI_Font (font_name);
#endif
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Is_Font
//
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Is_Font (const char *font_name, int point_size, bool is_bold)
{
	bool retval = false;

	//
	//	Check to see if both the name and height matches...
	//
	if (	(GDIFontName.Compare_No_Case (font_name) == 0) &&
			(point_size == PointSize) &&
			(is_bold == IsBold))
	{
		retval = true;
	}

	return retval;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Grow_Unicode_Array
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Grow_Unicode_Array (WCHAR ch)
{
	//
	//	Don't do anything if character is in the ASCII range
	//
	if ( ch < 256 ) {
		return ;
	}

	//
	//	Don't do anything if character is in the currently allocated range
	//
	if ( ch >= FirstUnicodeChar && ch <= LastUnicodeChar ) {
		return ;
	}

	uint16 first_index	= min( FirstUnicodeChar, static_cast<uint16>(ch) );
	uint16 last_index		= max( LastUnicodeChar, static_cast<uint16>(ch) );
	uint16 count			= (last_index - first_index) + 1;

	//
	//	Allocate enough memory to hold the new cells
	//
	FontCharsClassCharDataStruct **new_array = W3DNEWARRAY FontCharsClassCharDataStruct *[count];
	::memset (new_array, 0, sizeof (FontCharsClassCharDataStruct *) * count);

	//
	//	Copy the contents of the old array into the new array
	//
	if ( UnicodeCharArray != nullptr ) {
		int start_offset	= (FirstUnicodeChar - first_index);
		int old_count		= (LastUnicodeChar - FirstUnicodeChar) + 1;
		::memcpy (&new_array[start_offset], UnicodeCharArray, sizeof (FontCharsClassCharDataStruct *) * old_count);

		//
		//	Delete the old array
		//
		delete [] UnicodeCharArray;
		UnicodeCharArray = nullptr;
	}

	FirstUnicodeChar	= first_index;
	LastUnicodeChar	= last_index;
	UnicodeCharArray	= new_array;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Free_Character_Arrays
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Free_Character_Arrays ()
{
	if ( UnicodeCharArray != nullptr ) {

		int count = (LastUnicodeChar - FirstUnicodeChar) + 1;

		//
		//	Delete each member of the unicode array
		//
		for (int index = 0; index < count; index ++) {
			delete UnicodeCharArray[index];
			UnicodeCharArray[index] = nullptr;
		}

		//
		//	Delete the array itself
		//
		delete [] UnicodeCharArray;
		UnicodeCharArray = nullptr;
	}

	//
	//	Delete each member of the ascii character array
	//
	for (int index = 0; index < 256; index ++) {
		delete ASCIICharArray[index];
		ASCIICharArray[index] = nullptr;
	}
}

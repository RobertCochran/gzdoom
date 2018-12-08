/*
** textures.h
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __TEXTURES_H
#define __TEXTURES_H

#include "doomtype.h"
#include "vectors.h"
#include "v_palette.h"
#include "v_colortables.h"
#include "colormatcher.h"
#include "r_data/renderstyle.h"
#include "r_data/r_translate.h"
#include <vector>

// 15 because 0th texture is our texture
#define MAX_CUSTOM_HW_SHADER_TEXTURES 15

typedef TMap<int, bool> SpriteHits;

enum MaterialShaderIndex
{
	SHADER_Default,
	SHADER_Warp1,
	SHADER_Warp2,
	SHADER_Brightmap,
	SHADER_Specular,
	SHADER_SpecularBrightmap,
	SHADER_PBR,
	SHADER_PBRBrightmap,
	SHADER_Paletted,
	SHADER_NoTexture,
	SHADER_BasicFuzz,
	SHADER_SmoothFuzz,
	SHADER_SwirlyFuzz,
	SHADER_TranslucentFuzz,
	SHADER_JaggedFuzz,
	SHADER_NoiseFuzz,
	SHADER_SmoothNoiseFuzz,
	SHADER_SoftwareFuzz,
	FIRST_USER_SHADER
};

struct UserShaderDesc
{
	FString shader;
	MaterialShaderIndex shaderType;
	FString defines;
	bool disablealphatest = false;
};

extern TArray<UserShaderDesc> usershaders;


struct FloatRect
{
	float left,top;
	float width,height;


	void Offset(float xofs,float yofs)
	{
		left+=xofs;
		top+=yofs;
	}
	void Scale(float xfac,float yfac)
	{
		left*=xfac;
		width*=xfac;
		top*=yfac;
		height*=yfac;
	}
};

enum ECreateTexBufferFlags
{
	CTF_CheckHires = 1,		// use external hires replacement if found
	CTF_Expand = 2,			// create buffer with a one-pixel wide border
	CTF_ProcessData = 4,	// run postprocessing on the generated buffer. This is only needed when using the data for a hardware texture.
};



class FBitmap;
struct FRemapTable;
struct FCopyInfo;
class FScanner;

// Texture IDs
class FTextureManager;
class FTerrainTypeArray;
class IHardwareTexture;
class FMaterial;
extern int r_spriteadjustSW, r_spriteadjustHW;

class FNullTextureID : public FTextureID
{
public:
	FNullTextureID() : FTextureID(0) {}
};

//
// Animating textures and planes
//
// [RH] Expanded to work with a Hexen ANIMDEFS lump
//

struct FAnimDef
{
	FTextureID 	BasePic;
	uint16_t	NumFrames;
	uint16_t	CurFrame;
	uint8_t	AnimType;
	bool	bDiscrete;			// taken out of AnimType to have better control
	uint64_t	SwitchTime;			// Time to advance to next frame
	struct FAnimFrame
	{
		uint32_t	SpeedMin;		// Speeds are in ms, not tics
		uint32_t	SpeedRange;
		FTextureID	FramePic;
	} Frames[1];
	enum
	{
		ANIM_Forward,
		ANIM_Backward,
		ANIM_OscillateUp,
		ANIM_OscillateDown,
		ANIM_Random
	};

	void SetSwitchTime (uint64_t mstime);
};

struct FSwitchDef
{
	FTextureID PreTexture;		// texture to switch from
	FSwitchDef *PairDef;		// switch def to use to return to PreTexture
	uint16_t NumFrames;		// # of animation frames
	bool QuestPanel;	// Special texture for Strife mission
	int Sound;			// sound to play at start of animation. Changed to int to avoiud having to include s_sound here.
	struct frame		// Array of times followed by array of textures
	{					//   actual length of each array is <NumFrames>
		uint16_t TimeMin;
		uint16_t TimeRnd;
		FTextureID Texture;
	} frames[1];
};

struct FDoorAnimation
{
	FTextureID BaseTexture;
	FTextureID *TextureFrames;
	int NumTextureFrames;
	FName OpenSound;
	FName CloseSound;
};

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures, and we compose
// textures from the TEXTURE1/2 lists of patches.
struct patch_t
{ 
	int16_t			width;			// bounding box size 
	int16_t			height; 
	int16_t			leftoffset; 	// pixels to the left of origin 
	int16_t			topoffset;		// pixels below the origin 
	uint32_t 			columnofs[];	// only [width] used
	// the [0] is &columnofs[width] 
};

// All FTextures present their data to the world in 8-bit format, but if
// the source data is something else, this is it.
enum FTextureFormat : uint32_t
{
	TEX_Pal,
	TEX_Gray,
	TEX_RGB,		// Actually ARGB

	TEX_Count
};

class FSoftwareTexture;
class FGLRenderState;

struct spriteframewithrotate;
class FSerializer;
namespace OpenGLRenderer
{
	class FGLRenderState;
	class FHardwareTexture;
}


// Base texture class
class FTexture
{
	// This is initialization code that is allowed to have full access.
	friend void R_InitSpriteDefs ();
	friend void R_InstallSprite (int num, spriteframewithrotate *sprtemp, int &maxframe);
	friend class GLDefsParser;
	
	// The serializer also needs access to more specific info that shouldn't be accessible through the interface.
	friend FSerializer &Serialize(FSerializer &arc, const char *key, FTextureID &value, FTextureID *defval);

	// For now only give access to classes which cannot be reworked yet. None of these should remain here when all is done.
	friend class FSoftwareTexture;
	friend class FWarpTexture;
	friend class FMaterial;
	friend class OpenGLRenderer::FGLRenderState;	// For now this needs access to some fields in ApplyMaterial. This should be rerouted through the Material class
	friend struct FTexCoordInfo;
	friend class OpenGLRenderer::FHardwareTexture;
	friend class FMultiPatchTexture;
	friend class FSkyBox;
	friend class FBrightmapTexture;
	friend class FFontChar1;
	friend void RecordTextureColors (FTexture *pic, uint8_t *usedcolors);


public:
	static FTexture *CreateTexture(const char *name, int lumpnum, ETextureType usetype);
	static FTexture *CreateTexture(int lumpnum, ETextureType usetype);
	virtual ~FTexture ();
	void AddAutoMaterials();
	unsigned char *CreateUpsampledTextureBuffer(unsigned char *inputBuffer, const int inWidth, const int inHeight, int &outWidth, int &outHeight, bool hasAlpha);

	// These should only be used in places where the texture scaling must be ignored and absolutely nowhere else!
	// Preferably all code depending on the physical texture size should be rewritten, unless it is part of the software rasterizer.
	//int GetPixelWidth() { return GetWidth(); }
	//int GetPixelHeight() { return GetHeight(); }

	// These are mainly meant for 2D code which only needs logical information about the texture to position it properly.
	int GetDisplayWidth() { return GetScaledWidth(); }
	int GetDisplayHeight() { return GetScaledHeight(); }
	double GetDisplayWidthDouble() { return GetScaledWidthDouble(); }
	double GetDisplayHeightDouble() { return GetScaledHeightDouble(); }
	int GetDisplayLeftOffset() { return GetScaledLeftOffset(0); }
	int GetDisplayTopOffset() { return GetScaledTopOffset(0); }
	double GetDisplayLeftOffsetDouble() { return GetScaledLeftOffsetDouble(0); }
	double GetDisplayTopOffsetDouble() { return GetScaledTopOffsetDouble(0); }
	
	
	bool isValid() const { return UseType != ETextureType::Null; }
	bool isSWCanvas() const { return UseType == ETextureType::SWCanvas; }
	bool isSkybox() const { return bSkybox; }
	bool isFullbrightDisabled() const { return bDisableFullbright; }
	bool isHardwareCanvas() const { return bHasCanvas; }	// There's two here so that this can deal with software canvases in the hardware renderer later.
	bool isCanvas() const { return bHasCanvas; }
	bool isMiscPatch() const { return UseType == ETextureType::MiscPatch; }	// only used by the intermission screen to decide whether to tile the background image or not. 
	int isWarped() const { return bWarped; }
	int GetRotations() const { return Rotations; }
	void SetRotations(int rot) { Rotations = int16_t(rot); }
	
	const FString &GetName() const { return Name; }
	bool allowNoDecals() const { return bNoDecals; }
	bool isScaled() const { return Scale.X != 1 || Scale.Y != 1; }
	bool isMasked() const { return bMasked; }
	int GetSkyOffset() const { return SkyOffset; }
	FTextureID GetID() const { return id; }
	PalEntry GetSkyCapColor(bool bottom);
	virtual FTexture *GetRawTexture();		// for FMultiPatchTexture to override
	virtual int GetSourceLump() { return SourceLump; }	// needed by the scripted GetName method.
	void GetGlowColor(float *data);
	bool isGlowing() const { return bGlowing; }
	bool isAutoGlowing() const { return bAutoGlowing; }
	int GetGlowHeight() const { return GlowHeight; }
	bool isFullbright() const { return bFullbright; }
	void CreateDefaultBrightmap();
	bool FindHoles(const unsigned char * buffer, int w, int h);

	// Returns the whole texture, stored in column-major order
	virtual TArray<uint8_t> Get8BitPixels(bool alphatex);

public:
	static void FlipSquareBlock (uint8_t *block, int x, int y);
	static void FlipSquareBlockBgra (uint32_t *block, int x, int y);
	static void FlipSquareBlockRemap (uint8_t *block, int x, int y, const uint8_t *remap);
	static void FlipNonSquareBlock (uint8_t *blockto, const uint8_t *blockfrom, int x, int y, int srcpitch);
	static void FlipNonSquareBlockBgra (uint32_t *blockto, const uint32_t *blockfrom, int x, int y, int srcpitch);
	static void FlipNonSquareBlockRemap (uint8_t *blockto, const uint8_t *blockfrom, int x, int y, int srcpitch, const uint8_t *remap);
	static bool SmoothEdges(unsigned char * buffer,int w, int h);
	static PalEntry averageColor(const uint32_t *data, int size, int maxout);

	
	FSoftwareTexture *GetSoftwareTexture();

protected:
	//int16_t LeftOffset, TopOffset;

	DVector2 Scale;

	int SourceLump;
	FTextureID id;

	FMaterial *Material[2] = { nullptr, nullptr };
	IHardwareTexture *SystemTexture[2] = { nullptr, nullptr };
	FSoftwareTexture *SoftwareTexture = nullptr;

	// None of the following pointers are owned by this texture, they are all controlled by the texture manager.

	// Paletted variant
	FTexture *PalVersion = nullptr;
	// External hires texture
	FTexture *HiresTexture = nullptr;
	// Material layers
	FTexture *Brightmap = nullptr;
	FTexture *Normal = nullptr;							// Normal map texture
	FTexture *Specular = nullptr;						// Specular light texture for the diffuse+normal+specular light model
	FTexture *Metallic = nullptr;						// Metalness texture for the physically based rendering (PBR) light model
	FTexture *Roughness = nullptr;						// Roughness texture for PBR
	FTexture *AmbientOcclusion = nullptr;				// Ambient occlusion texture for PBR
	
	FTexture *CustomShaderTextures[MAX_CUSTOM_HW_SHADER_TEXTURES] = { nullptr }; // Custom texture maps for custom hardware shaders

	FString Name;
	ETextureType UseType;	// This texture's primary purpose

	uint8_t bNoDecals:1;		// Decals should not stick to texture
	uint8_t bNoRemap0:1;		// Do not remap color 0 (used by front layer of parallax skies)
	uint8_t bWorldPanning:1;	// Texture is panned in world units rather than texels
	uint8_t bMasked:1;			// Texture (might) have holes
	uint8_t bAlphaTexture:1;	// Texture is an alpha channel without color information
	uint8_t bHasCanvas:1;		// Texture is based off FCanvasTexture
	uint8_t bWarped:2;			// This is a warped texture. Used to avoid multiple warps on one texture
	uint8_t bComplex:1;		// Will be used to mark extended MultipatchTextures that have to be
							// fully composited before subjected to any kind of postprocessing instead of
							// doing it per patch.
	uint8_t bMultiPatch:2;		// This is a multipatch texture (we really could use real type info for textures...)
	uint8_t bKeepAround:1;		// This texture was used as part of a multi-patch texture. Do not free it.
	uint8_t bFullNameTexture : 1;
	uint8_t bBrightmapChecked : 1;				// Set to 1 if brightmap has been checked
	uint8_t bGlowing : 1;						// Texture glow color
	uint8_t bAutoGlowing : 1;					// Glow info is determined from texture image.
	uint8_t bFullbright : 1;					// always draw fullbright
	uint8_t bDisableFullbright : 1;				// This texture will not be displayed as fullbright sprite
	uint8_t bSkybox : 1;						// is a cubic skybox
	uint8_t bNoCompress : 1;
	uint8_t bNoExpand : 1;
	int8_t bTranslucent : 2;
	bool bHiresHasColorKey = false;				// Support for old color-keyed Doomsday textures

	uint16_t Rotations;
	int16_t SkyOffset;
	FloatRect *areas = nullptr;
	int areacount = 0;
	int GlowHeight = 128;
	PalEntry GlowColor = 0;
	int HiresLump = -1;							// For external hires textures.
	float Glossiness = 10.f;
	float SpecularLevel = 0.1f;
	float shaderspeed = 1.f;
	int shaderindex = 0;



	// Returns true if GetPixelsBgra includes mipmaps
	virtual bool Mipmapped() { return true; }

	virtual int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate=0, FCopyInfo *inf = NULL);
	virtual int CopyTrueColorTranslated(FBitmap *bmp, int x, int y, int rotate, PalEntry *remap, FCopyInfo *inf = NULL);
	virtual bool UseBasePalette();
	virtual FTexture *GetRedirect();

	// Returns the native pixel format for this image
	virtual FTextureFormat GetFormat();

	// Fill the native texture buffer with pixel data for this image
	virtual void FillBuffer(uint8_t *buff, int pitch, int height, FTextureFormat fmt);
	void SetSpeed(float fac) { shaderspeed = fac; }

	int GetWidth () { return Width; }
	int GetHeight () { return Height; }

	int GetScaledWidth () { int foo = int((Width * 2) / Scale.X); return (foo >> 1) + (foo & 1); }
	int GetScaledHeight () { int foo = int((Height * 2) / Scale.Y); return (foo >> 1) + (foo & 1); }
	double GetScaledWidthDouble () { return Width / Scale.X; }
	double GetScaledHeightDouble () { return Height / Scale.Y; }
	double GetScaleY() const { return Scale.Y; }

	// Now with improved offset adjustment.
	int GetLeftOffset(int adjusted) { return _LeftOffset[adjusted]; }
	int GetTopOffset(int adjusted) { return _TopOffset[adjusted]; }
	int GetScaledLeftOffset (int adjusted) { int foo = int((_LeftOffset[adjusted] * 2) / Scale.X); return (foo >> 1) + (foo & 1); }
	int GetScaledTopOffset (int adjusted) { int foo = int((_TopOffset[adjusted] * 2) / Scale.Y); return (foo >> 1) + (foo & 1); }
	double GetScaledLeftOffsetDouble(int adjusted) { return _LeftOffset[adjusted] / Scale.X; }
	double GetScaledTopOffsetDouble(int adjusted) { return _TopOffset[adjusted] / Scale.Y; }

	// Interfaces for the different renderers. Everything that needs to check renderer-dependent offsets
	// should use these, so that if changes are needed, this is the only place to edit.

	// For the original software renderer
	int GetLeftOffsetSW() { return _LeftOffset[r_spriteadjustSW]; }
	int GetTopOffsetSW() { return _TopOffset[r_spriteadjustSW]; }
	int GetScaledLeftOffsetSW() { return GetScaledLeftOffset(r_spriteadjustSW); }
	int GetScaledTopOffsetSW() { return GetScaledTopOffset(r_spriteadjustSW); }

	// For the softpoly renderer, in case it wants adjustment
	int GetLeftOffsetPo() { return _LeftOffset[r_spriteadjustSW]; }
	int GetTopOffsetPo() { return _TopOffset[r_spriteadjustSW]; }
	int GetScaledLeftOffsetPo() { return GetScaledLeftOffset(r_spriteadjustSW); }
	int GetScaledTopOffsetPo() { return GetScaledTopOffset(r_spriteadjustSW); }

	// For the hardware renderer
	int GetLeftOffsetHW() { return _LeftOffset[r_spriteadjustHW]; }
	int GetTopOffsetHW() { return _TopOffset[r_spriteadjustHW]; }

	virtual void ResolvePatches() {}

	virtual void SetFrontSkyLayer();

	void CopyToBlock (uint8_t *dest, int dwidth, int dheight, int x, int y, int rotate, const uint8_t *translation, bool style);

	static void InitGrayMap();

	void CopySize(FTexture *BaseTexture)
	{
		Width = BaseTexture->GetWidth();
		Height = BaseTexture->GetHeight();
		_TopOffset[0] = BaseTexture->_TopOffset[0];
		_TopOffset[1] = BaseTexture->_TopOffset[1];
		_LeftOffset[0] = BaseTexture->_LeftOffset[0];
		_LeftOffset[1] = BaseTexture->_LeftOffset[1];
		Scale = BaseTexture->Scale;
	}

	void SetScaledSize(int fitwidth, int fitheight);

protected:
	uint16_t Width, Height;
	int16_t _LeftOffset[2], _TopOffset[2];
	static uint8_t GrayMap[256];
	uint8_t *GetRemap(bool wantluminance, bool srcisgrayscale = false)
	{
		if (wantluminance)
		{
			return translationtables[TRANSLATION_Standard][srcisgrayscale ? STD_Gray : STD_Grayscale]->Remap;
		}
		else
		{
			return srcisgrayscale ? GrayMap : GPalette.Remap;
		}
	}

	uint8_t RGBToPalettePrecise(bool wantluminance, int r, int g, int b, int a = 255)
	{
		if (wantluminance)
		{
			return (uint8_t)Luminance(r, g, b) * a / 255;
		}
		else
		{
			return ColorMatcher.Pick(r, g, b);
		}
	}

	uint8_t RGBToPalette(bool wantluminance, int r, int g, int b, int a = 255)
	{
		if (wantluminance)
		{
			// This is the same formula the OpenGL renderer uses for grayscale textures with an alpha channel.
			return (uint8_t)(Luminance(r, g, b) * a / 255);
		}
		else
		{
			return a < 128? 0 : RGB256k.RGB[r >> 2][g >> 2][b >> 2];
		}
	}

	uint8_t RGBToPalette(bool wantluminance, PalEntry pe, bool hasalpha = true)
	{
		return RGBToPalette(wantluminance, pe.r, pe.g, pe.b, hasalpha? pe.a : 255);
	}

	uint8_t RGBToPalette(FRenderStyle style, int r, int g, int b, int a = 255)
	{
		return RGBToPalette(!!(style.Flags & STYLEF_RedIsAlpha), r, g, b, a);
	}

	FTexture (const char *name = NULL, int lumpnum = -1);

	void CopyInfo(FTexture *other)
	{
		CopySize(other);
		bNoDecals = other->bNoDecals;
		Rotations = other->Rotations;
	}



public:
	unsigned char * CreateTexBuffer(int translation, int & w, int & h, int flags = 0);
	bool GetTranslucency();

private:
	int CheckDDPK3();
	int CheckExternalFile(bool & hascolorkey);
	unsigned char *LoadHiresTexture(int *width, int *height);

	bool bSWSkyColorDone = false;
	PalEntry FloorSkyColor;
	PalEntry CeilingSkyColor;

public:

	void CheckTrans(unsigned char * buffer, int size, int trans);
	bool ProcessData(unsigned char * buffer, int w, int h, bool ispatch);
	int CheckRealHeight();
	void SetSpriteAdjust();

	friend class FTextureManager;
};


class FxAddSub;
// Texture manager
class FTextureManager
{
	friend class FxAddSub;	// needs access to do a bounds check on the texture ID.
public:
	FTextureManager ();
	~FTextureManager ();

	// This only gets used in UI code so we do not need PALVERS handling.
	FTexture *GetTextureByName(const char *name, bool animate = false)
	{
		FTextureID texnum = GetTextureID (name, ETextureType::MiscPatch);
		if (!texnum.Exists()) return nullptr;
		if (!animate) return Textures[texnum.GetIndex()].Texture;
	 	else return Textures[Translation[texnum.GetIndex()]].Texture;
	}
	
	FTexture *GetTexture(FTextureID texnum, bool animate = false)
	{
		if ((size_t)texnum.GetIndex() >= Textures.Size()) return nullptr;
		if (animate) texnum = Translation[texnum.GetIndex()];
		return Textures[texnum.GetIndex()].Texture;
	}
	
	// This is the only access function that should be used inside the software renderer.
	FTexture *GetPalettedTexture(FTextureID texnum, bool animate)
	{
		if ((size_t)texnum.texnum >= Textures.Size()) return nullptr;
		if (animate) texnum = Translation[texnum.GetIndex()];
		texnum = PalCheck(texnum).GetIndex();
		return Textures[texnum.GetIndex()].Texture;
	}
	
	FTexture *ByIndex(int i, bool animate = false)
	{
		if (unsigned(i) >= Textures.Size()) return NULL;
		if (animate) i = Translation[i];
		return Textures[i].Texture;
	}
	FTexture *FindTexture(const char *texname, ETextureType usetype = ETextureType::MiscPatch, BITFIELD flags = TEXMAN_TryAny);



//public:

	FTextureID PalCheck(FTextureID tex);

	enum
	{
		TEXMAN_TryAny = 1,
		TEXMAN_Overridable = 2,
		TEXMAN_ReturnFirst = 4,
		TEXMAN_AllowSkins = 8,
		TEXMAN_ShortNameOnly = 16,
		TEXMAN_DontCreate = 32
	};

	enum
	{
		HIT_Wall = 1,
		HIT_Flat = 2,
		HIT_Sky = 4,
		HIT_Sprite = 8,

		HIT_Columnmode = HIT_Wall|HIT_Sky|HIT_Sprite
	};

	FTextureID CheckForTexture (const char *name, ETextureType usetype, BITFIELD flags=TEXMAN_TryAny);
	FTextureID GetTextureID (const char *name, ETextureType usetype, BITFIELD flags=0);
	int ListTextures (const char *name, TArray<FTextureID> &list, bool listall = false);

	void AddTexturesLump (const void *lumpdata, int lumpsize, int deflumpnum, int patcheslump, int firstdup=0, bool texture1=false);
	void AddTexturesLumps (int lump1, int lump2, int patcheslump);
	void AddGroup(int wadnum, int ns, ETextureType usetype);
	void AddPatches (int lumpnum);
	void AddHiresTextures (int wadnum);
	void LoadTextureDefs(int wadnum, const char *lumpname);
	void ParseTextureDef(int remapLump);
	void ParseXTexture(FScanner &sc, ETextureType usetype);
	void SortTexturesByType(int start, int end);
	bool AreTexturesCompatible (FTextureID picnum1, FTextureID picnum2);

	FTextureID CreateTexture (int lumpnum, ETextureType usetype=ETextureType::Any);	// Also calls AddTexture
	FTextureID AddTexture (FTexture *texture);
	FTextureID GetDefaultTexture() const { return DefaultTexture; }

	void LoadTextureX(int wadnum);
	void AddTexturesForWad(int wadnum);
	void Init();
	void DeleteAll();
	void SpriteAdjustChanged();

	// Replaces one texture with another. The new texture will be assigned
	// the same name, slot, and use type as the texture it is replacing.
	// The old texture will no longer be managed. Set free true if you want
	// the old texture to be deleted or set it false if you want it to
	// be left alone in memory. You will still need to delete it at some
	// point, because the texture manager no longer knows about it.
	// This function can be used for such things as warping textures.
	void ReplaceTexture (FTextureID picnum, FTexture *newtexture, bool free);

	int NumTextures () const { return (int)Textures.Size(); }

	void UpdateAnimations (uint64_t mstime);
	int GuesstimateNumTextures ();

	FSwitchDef *FindSwitch (FTextureID texture);
	FDoorAnimation *FindAnimatedDoor (FTextureID picnum);

private:

	// texture counting
	int CountTexturesX ();
	int CountLumpTextures (int lumpnum);
	void AdjustSpriteOffsets();

	// Build tiles
	void AddTiles (const FString &pathprefix, const void *, int translation);
	//int CountBuildTiles ();
	void InitBuildTiles ();

	// Animation stuff
	FAnimDef *AddAnim (FAnimDef *anim);
	void FixAnimations ();
	void InitAnimated ();
	void InitAnimDefs ();
	FAnimDef *AddSimpleAnim (FTextureID picnum, int animcount, uint32_t speedmin, uint32_t speedrange=0);
	FAnimDef *AddComplexAnim (FTextureID picnum, const TArray<FAnimDef::FAnimFrame> &frames);
	void ParseAnim (FScanner &sc, ETextureType usetype);
	FAnimDef *ParseRangeAnim (FScanner &sc, FTextureID picnum, ETextureType usetype, bool missing);
	void ParsePicAnim (FScanner &sc, FTextureID picnum, ETextureType usetype, bool missing, TArray<FAnimDef::FAnimFrame> &frames);
	void ParseWarp(FScanner &sc);
	void ParseCameraTexture(FScanner &sc);
	FTextureID ParseFramenum (FScanner &sc, FTextureID basepicnum, ETextureType usetype, bool allowMissing);
	void ParseTime (FScanner &sc, uint32_t &min, uint32_t &max);
	FTexture *Texture(FTextureID id) { return Textures[id.GetIndex()].Texture; }
	void SetTranslation (FTextureID fromtexnum, FTextureID totexnum);
	void ParseAnimatedDoor(FScanner &sc);

	void InitPalettedVersions();
	void GenerateGlobalBrightmapFromColormap();

	// Switches

	void InitSwitchList ();
	void ProcessSwitchDef (FScanner &sc);
	FSwitchDef *ParseSwitchDef (FScanner &sc, bool ignoreBad);
	void AddSwitchPair (FSwitchDef *def1, FSwitchDef *def2);

	struct TextureHash
	{
		FTexture *Texture;
		int HashNext;
	};
	enum { HASH_END = -1, HASH_SIZE = 1027 };
	TArray<TextureHash> Textures;
	TArray<int> Translation;
	int HashFirst[HASH_SIZE];
	FTextureID DefaultTexture;
	TArray<int> FirstTextureForFile;
	TArray<TArray<uint8_t> > BuildTileData;

	TArray<FAnimDef *> mAnimations;
	TArray<FSwitchDef *> mSwitchDefs;
	TArray<FDoorAnimation> mAnimatedDoors;

public:
	bool HasGlobalBrightmap;
	FRemapTable GlobalBrightmap;
	short sintable[2048];	// for texture warping
	enum
	{
		SINMASK = 2047
	};

	FTextureID glLight;
	FTextureID glPart2;
	FTextureID glPart;
	FTextureID mirrorTexture;

};

// base class for everything that can be used as a world texture. 
// This intermediate class encapsulates the buffers for the software renderer.
class FWorldTexture : public FTexture
{
protected:
	FWorldTexture(const char *name = nullptr, int lumpnum = -1);
};

// A texture that doesn't really exist
class FDummyTexture : public FTexture
{
public:
	FDummyTexture ();
	void SetSize (int width, int height);
};


// A texture that can be drawn to.
class DCanvas;
class AActor;

class FCanvasTexture : public FTexture
{
public:
	FCanvasTexture (const char *name, int width, int height);

	//const uint8_t *GetColumn(FRenderStyle style, unsigned int column, const FSoftwareTextureSpan **spans_out);
	//const uint32_t *GetPixelsBgra() override;

	//const uint8_t *Get8BitPixels(bool alphatex);
	bool CheckModified (FRenderStyle) /*override*/;
	void NeedUpdate() { bNeedsUpdate=true; }
	void SetUpdated() { bNeedsUpdate = false; bDidUpdate = true; bFirstUpdate = false; }
	DCanvas *GetCanvas() { return Canvas; }
	DCanvas *GetCanvasBgra() { return CanvasBgra; }
	bool Mipmapped() override { return false; }
	void MakeTextureBgra ();

protected:

	DCanvas *Canvas = nullptr;
	DCanvas *CanvasBgra = nullptr;
	uint8_t *Pixels = nullptr;
	uint32_t *PixelsBgra = nullptr;
	//FSoftwareTextureSpan DummySpans[2];
	bool bNeedsUpdate = true;
	bool bDidUpdate = false;
	bool bPixelsAllocated = false;
	bool bPixelsAllocatedBgra = false;
public:
	bool bFirstUpdate;


	friend struct FCanvasTextureInfo;
};

// A wrapper around a hardware texture, to allow using it in the 2D drawing interface.
class FWrapperTexture : public FTexture
{
	int Format;
public:
	FWrapperTexture(int w, int h, int bits = 1);
	IHardwareTexture *GetSystemTexture(int slot)
	{
		return SystemTexture[slot];
	}

	int GetColorFormat() const
	{
		return Format;
	}
};

extern FTextureManager TexMan;

struct FTexCoordInfo
{
	int mRenderWidth;
	int mRenderHeight;
	int mWidth;
	FVector2 mScale;
	FVector2 mTempScale;
	bool mWorldPanning;

	float FloatToTexU(float v) const { return v / mRenderWidth; }
	float FloatToTexV(float v) const { return v / mRenderHeight; }
	float RowOffset(float ofs) const;
	float TextureOffset(float ofs) const;
	float TextureAdjustWidth() const;
	void GetFromTexture(FTexture *tex, float x, float y);
};



#endif




// Modified by oggzee & usptactical

#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "gui.h"
#include "sys.h"
#include "fat.h"
#include "video.h"
#include "cache.h"
#include "cfg.h"
#include "grid.h"
#include "coverflow.h"
//#include "wgui.h"
#include "menu.h"

#include "png.h"

extern void *bg_buf_rgba;
extern bool imageNotFound;
extern int gui_style;
extern int grid_rows;
extern int page_gi;
extern int __console_disable;

bool loadGame;
bool suppressCoverDrawing;

/* Constants */
int COVER_WIDTH = 160;
int COVER_HEIGHT = 225;

// #define USBLOADER_PATH		"sd:/usb-loader"

extern bool imageNotFound;

#define SAFE_PNGU_RELEASE(CTX) if(CTX){PNGU_ReleaseImageContext(CTX);CTX=NULL;}

static int grr_init = 0;
static int grx_init = 0;
static int prev_cover_style = CFG_COVER_STYLE_2D;
static int prev_cover_height;
static int prev_cover_width;

int game_select = -1;

s32 __Gui_DrawPngA(void *img, u32 x, u32 y);
s32 __Gui_DrawPngRA(void *img, u32 x, u32 y, int width, int height);
void CopyRGBA(char *src, int srcWidth, int srcHeight,
		char *dest, int dstWidth, int dstHeight,
		int srcX, int srcY, int dstX, int dstY, int w, int h);
void CompositeRGBA(char *bg_buf, int bg_w, int bg_h,
		int x, int y, char *img, int width, int height);
void ResizeRGBA(char *img, int imgWidth, int imgHeight,
	   char *resize, int width, int height);
int Load_Theme_Image(char *name, void **img_buf);

// FLOW_Z camera
float cam_f = 0.0f;
float cam_dir = 1.0;
float cam_z = -578.0F;
Vector cam_look = {319.5F, 239.5F, 0.0F};

//AA
GRRLIB_texImg aa_texBuffer[4];

Mtx GXmodelView2D;
extern unsigned char bgImg[];
//extern unsigned char bgImg_wide[];
extern unsigned char bg_gui[];
extern unsigned char introImg[];
extern unsigned char coverImg[];
extern unsigned char coverImg_full[];
//extern unsigned char coverImg_wide[];
extern unsigned char pointer[];
extern unsigned char hourglass[];
extern unsigned char star_icon[];
extern unsigned char gui_font[];
extern unsigned char font_10[];
extern unsigned char font_12[];

struct M2_texImg t2_bg;
struct M2_texImg t2_bg_con;
struct M2_texImg t2_nocover;
struct M2_texImg t2_nocover_full;

GRRLIB_texImg tx_pointer;
GRRLIB_texImg tx_hourglass;
GRRLIB_texImg tx_star;
GRRLIB_texImg tx_font;
GRRLIB_texImg tx_font_clock;


s32 __Gui_GetPngDimensions(void *img, u32 *w, u32 *h)
{
	IMGCTX   ctx = NULL;
	PNGUPROP imgProp;

	s32 ret;

	/* Select PNG data */
	ctx = PNGU_SelectImageFromBuffer(img);
	if (!ctx) {
		ret = -1;
		goto out;
	}

	/* Get image properties */
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) {
		ret = -1;
		goto out;
	}

	/* Set image width and height */
	*w = imgProp.imgWidth;
	*h = imgProp.imgHeight;

	/* Success */
	ret = 0;

out:
	/* Free memory */
	if (ctx)
		PNGU_ReleaseImageContext(ctx);

	return ret;
}

s32 __Gui_DrawPng(void *img, u32 x, u32 y)
{
	IMGCTX   ctx = NULL;
	PNGUPROP imgProp;

	s32 ret;

	/* Select PNG data */
	ctx = PNGU_SelectImageFromBuffer(img);
	if (!ctx) {
		ret = -1;
		goto out;
	}

	/* Get image properties */
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) {
		ret = -1;
		goto out;
	}

	/* Draw image */
	Video_DrawPng(ctx, imgProp, x, y);

	/* Success */
	ret = 0;

out:
	/* Free memory */
	if (ctx)
		PNGU_ReleaseImageContext(ctx);

	return ret;
}


void Gui_InitConsole(void)
{
	/* Initialize console */
	Con_Init(CONSOLE_XCOORD, CONSOLE_YCOORD, CONSOLE_WIDTH, CONSOLE_HEIGHT);
}

void Gui_Console_Enable(void)
{
	if (!__console_disable) return;
	Video_DrawBg();
	// current frame buffer
	__console_enable(_Video_GetFB(-1));
}

IMGCTX Gui_OpenPNG(void *img, int *w, int *h)
{
	IMGCTX ctx = NULL;
	PNGUPROP imgProp;
	int ret;

	// Select PNG data
	ctx = PNGU_SelectImageFromBuffer(img);
	if (!ctx) {
		return NULL;
	}

	// Get image properties
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK
		|| imgProp.imgWidth == 0
		|| imgProp.imgWidth > 1024
		|| imgProp.imgHeight == 0
		|| imgProp.imgHeight > 1024)
	{
		PNGU_ReleaseImageContext(ctx);
		return NULL;
	}

	if (w) *w = imgProp.imgWidth;
	if (h) *h = imgProp.imgHeight;

	return ctx;
}

void* Gui_DecodePNG(IMGCTX ctx)
{
	PNGUPROP imgProp;
	void *img_buf = NULL;
	int ret;

	if (ctx == NULL) return NULL;

	// Get image properties
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) {
		return NULL;
	}
	// alloc
	img_buf = memalign(32, imgProp.imgWidth * imgProp.imgHeight * 4);
	if (img_buf == NULL) {
		return NULL;
	}
	// decode
	ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0,
			imgProp.imgWidth, imgProp.imgHeight, 255,
			imgProp.imgWidth, imgProp.imgHeight, img_buf);
	return img_buf;
}

int Gui_DecodePNG_scale_to(IMGCTX ctx, void *img_buf, int dest_w, int dest_h)
{
	PNGUPROP imgProp;
	int width, height;
	int ret;

	if (ctx == NULL) return -1;
	// Get image properties
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) return -1;
	width = imgProp.imgWidth;
	height = imgProp.imgHeight;

	if (width != dest_w || height != dest_h) {
		// fix size
		char *resize_buf = NULL;
		// decode
		resize_buf = Gui_DecodePNG(ctx);
		if (!resize_buf) return -1;
		// resize
		ResizeRGBA(resize_buf, width, height, img_buf, dest_w, dest_h);
		// free
		SAFE_FREE(resize_buf);
	} else {
		// Decode image
		PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0, width, height, 255,
			dest_w, dest_h, img_buf);
	}
	return 0;
}

int Gui_DecodePNG_scaleBG_to(IMGCTX ctx, void *img_buf)
{
	PNGUPROP imgProp;
	int width, height;
	int ret;

	if (ctx == NULL) return -1;

	// Get image properties
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) return -1;
	width = imgProp.imgWidth;
	height = imgProp.imgHeight;

	if (height != BACKGROUND_HEIGHT || width < BACKGROUND_WIDTH) {
		return -1;
	}

	if (width > BACKGROUND_WIDTH) {
		// fix size
		char *resize_buf = NULL;
		// decode
		resize_buf = Gui_DecodePNG(ctx);
		if (!resize_buf) {
			return -1;
		}
		if (CFG.widescreen) {
			// resize
			ResizeRGBA(resize_buf, width, height,
				   img_buf, BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
		} else {
			// crop
			int x = (width - BACKGROUND_WIDTH) / 2;
			if (x<0) x = 0;
			CopyRGBA(resize_buf, width, height,
					img_buf, BACKGROUND_WIDTH, BACKGROUND_HEIGHT,
					x, 0, 0, 0, BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
		}
		// free
		SAFE_FREE(resize_buf);
	} else {
		// Decode image
		PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0, width, height, 255,
			BACKGROUND_WIDTH, BACKGROUND_HEIGHT, img_buf);
	}
	return 0;
}

void* Gui_AllocBG()
{
	return memalign(32, BACKGROUND_WIDTH * BACKGROUND_HEIGHT * 4);
}

void *Gui_DecodePNG_scaleBG(IMGCTX ctx)
{
	void *img_buf = NULL;
	int ret;

	if (ctx == NULL) return NULL;

	img_buf = Gui_AllocBG();
	if (img_buf == NULL) return NULL;

	ret = Gui_DecodePNG_scaleBG_to(ctx, img_buf);
	if (ret) {
		SAFE_FREE(img_buf);
		return NULL;
	}
	return img_buf;
}

int Gui_LoadBG_data_to(void *data, void *dest_buf)
{
	IMGCTX ctx = NULL;
	int ret;

	ctx = Gui_OpenPNG(data, NULL, NULL);
	if (ctx == NULL) {
		return -1;
	}
	ret = Gui_DecodePNG_scaleBG_to(ctx, dest_buf);
	PNGU_ReleaseImageContext(ctx);
	return ret;
}

void* Gui_LoadBG_data(void *data)
{
	void *img_buf;
	int ret;

	img_buf = Gui_AllocBG();
	if (img_buf == NULL) return NULL;
	ret = Gui_LoadBG_data_to(data, img_buf);
	if (ret) {
		SAFE_FREE(img_buf);
	}
	return img_buf;
}

int Gui_LoadBG_to(char *path, void *dest_buf)
{
	void *data = NULL;
	int ret = -1;

	ret = Fat_ReadFile(path, &data);
	if (ret <= 0 || data == NULL) {
		return -1;
	}
	ret = Gui_LoadBG_data_to(data, dest_buf);
	SAFE_FREE(data);
	return ret;
}

void* Gui_LoadBG(char *path)
{
	void *data = NULL;
	void *img_buf = NULL;
	int ret = -1;

	ret = Fat_ReadFile(path, &data);
	if (ret <= 0 || data == NULL) {
		return NULL;
	}
	img_buf = Gui_LoadBG_data(data);
	SAFE_FREE(data);
	return img_buf;
}

int Gui_OverlayBg(char *path, void *bg_buf)
{
	void *img_buf = NULL;
	img_buf = Gui_LoadBG(path);
	if (img_buf == NULL) {
		return -1;
	}
	// overlay
	CompositeRGBA(bg_buf, BACKGROUND_WIDTH, BACKGROUND_HEIGHT,
			0, 0, img_buf, BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
	SAFE_FREE(img_buf);
	return 0;
}

void Gui_LoadBackground(void)
{
	s32 ret = -1;

	Video_AllocBg();

	// Try to open background image from SD
	if (*CFG.background) {
		ret = Gui_LoadBG_to(CFG.background, bg_buf_rgba);
	}
	// if failed, use builtin
	if (ret) {
		ret = Gui_LoadBG_data_to(bgImg, bg_buf_rgba);
	}

	// Overlay
	char path[200];
	ret = -1;

	if (CFG.widescreen) {
		snprintf(D_S(path), "%s/bg_overlay_w.png", CFG.theme_path);
		ret = Gui_OverlayBg(path, bg_buf_rgba);
	}
	if (ret) {
		snprintf(D_S(path), "%s/bg_overlay.png", CFG.theme_path);
		ret = Gui_OverlayBg(path, bg_buf_rgba);
	}

	// save background to ycbr
	Video_SaveBgRGBA();
} 

void Gui_DrawBackground(void)
{
	if (CFG.direct_launch) return;

	// Load background
	Gui_LoadBackground();

	// Draw background
	Video_DrawBg();
}

void Gui_DrawIntro(void)
{
	GXRModeObj *rmode = _Video_GetVMode();
	if (CFG.direct_launch && CFG.intro==0) {
		CON_InitTr(rmode, 560, 424, 80, 48, CONSOLE_BG_COLOR);
		return;
	}
	
	// Draw the intro image
	VIDEO_WaitVSync();
	//__Gui_DrawPng(introImg, 0, 0);
	IMGCTX ctx = Gui_OpenPNG(introImg, NULL, NULL);
	if (!ctx) return;
	void *img_buf = memalign(32, rmode->fbWidth * rmode->xfbHeight * 4);
	if (!img_buf) return;
	Gui_DecodePNG_scale_to(ctx, img_buf, rmode->fbWidth, rmode->xfbHeight);
	VIDEO_WaitVSync();
	Video_DrawRGBA(0, 0, img_buf, rmode->fbWidth, rmode->xfbHeight);

	CON_InitTr(rmode, 560, 424, 80, 48, CONSOLE_BG_COLOR);
	Con_Clear();
	printf("\nv%s", CFG_VERSION);
	__console_flush(0);
	
	VIDEO_WaitVSync();
	SAFE_FREE(img_buf);
}

/**
 * Tries to read the game cover image from disk.  This method uses the file path of 
 *  the currently set CFG.cover_style (e.g. 2d, 3d, full) to look for the images.
 * 
 *  @param *discid the disk id of the cover to load
 *  @param **p_imgData the image
 *  @param load_noimage bool representing if the noimage png should be loaded
 *  @return int
 */
int Gui_LoadCover(u8 *discid, void **p_imgData, bool load_noimage)
{
	int ret = -1;
	char filepath[200];
	char *wide = "";

	if (*discid) {
		if (CFG.widescreen) wide = "_wide";
		retry_open:
		/* Generate cover filepath */
		snprintf(filepath, sizeof(filepath), "%s/%.6s%s.png",
				CFG.covers_path, discid, wide);

		/* Open cover */
		ret = Fat_ReadFile(filepath, p_imgData);
		if (ret <= 0) {
			// if 6 character id not found, try 4 character id
			snprintf(filepath, sizeof(filepath), "%s/%.4s%s.png",
					CFG.covers_path, discid, wide);
			ret = Fat_ReadFile(filepath, p_imgData);
		}
		if (ret <= 0 && *wide) {
			// retry with non-wide
			wide = "";
			goto retry_open;
		}
		if (ret <= 0) {
			imageNotFound = true;
		}
	}
	if (ret <= 0 && load_noimage) {
		if (CFG.widescreen) wide = "_wide";
		retry_open2:
		snprintf(filepath, sizeof(filepath), "%s/noimage%s.png",
				CFG.covers_path, wide);
		ret = Fat_ReadFile(filepath, p_imgData);
		if (ret <= 0 && *wide) {
			// retry with non-wide noimage
			wide = "";
			goto retry_open2;
		}
	}
	return ret;
}


/**
 * Tries to read the game cover image from disk.  This method uses the passed
 *  in cover_style value to determine which file path to use.  
 * 
 *  @param *discid the disk id of the cover to load
 *  @param **p_imgData the image
 *  @param load_noimage bool representing if the noimage png should be loaded
 *  @param cover_style represents the cover style to load
 *  @return int
 */
int Gui_LoadCover_style(u8 *discid, void **p_imgData, bool load_noimage, int cover_style)
{
	s32 ret = -1;
	char filepath[200];
	char coverpath[200];

	if (*discid) {
	
		switch(cover_style){
			case(CFG_COVER_STYLE_FULL):
				STRCOPY(coverpath, CFG.covers_path_full);
				break;
			case(CFG_COVER_STYLE_3D):
				STRCOPY(coverpath, CFG.covers_path_3d);
				break;
			case(CFG_COVER_STYLE_DISC):
				STRCOPY(coverpath, CFG.covers_path_disc);
				break;
			default:
			case(CFG_COVER_STYLE_2D):
				STRCOPY(coverpath, CFG.covers_path_2d);
				break;
		}
		
		/* Generate cover filepath for full covers */
		snprintf(filepath, sizeof(filepath), "%s/%.6s.png",
			coverpath, discid);

		/* Open cover */
		ret = Fat_ReadFile(filepath, p_imgData);
		if (ret <= 0) {
		
			// if 6 character id not found, try 4 character id
			snprintf(filepath, sizeof(filepath), "%s/%.4s.png",
				coverpath, discid);
			ret = Fat_ReadFile(filepath, p_imgData);
		}
		if (ret <= 0) {
			imageNotFound = true;
		}
	}
	if (ret <= 0 && load_noimage) {
		snprintf(filepath, sizeof(filepath), "%s/noimage.png",
				CFG.covers_path);
		ret = Fat_ReadFile(filepath, p_imgData);
	}
	return ret;
}


void DrawCoverImg(u8 *discid, char *img_buf, int width, int height)
{
	char *bg_buf = NULL;
	char *star_buf = NULL;
	IMGCTX ctx = NULL;
	s32 ret;

	// get background
	bg_buf = memalign(32, width * height * 4);
	if (!bg_buf) return;
	Video_GetBG(COVER_XCOORD, COVER_YCOORD, bg_buf, width, height);
	// composite img to bg
	CompositeRGBA(bg_buf, width, height, 0, 0,
				img_buf, width, height);
	// favorite?
	if (is_favorite(discid)) {
		// star
		PNGUPROP imgProp;
		void *fav_buf = NULL;
		// Select PNG data
		ret = Load_Theme_Image("favorite.png", &fav_buf);
		if (ret == 0 && fav_buf) {
			ctx = PNGU_SelectImageFromBuffer(fav_buf);
		} else {
			ctx = PNGU_SelectImageFromBuffer(star_icon);
		}
		if (!ctx) goto out;
		// Get image properties
		ret = PNGU_GetImageProperties(ctx, &imgProp);
		if (ret != PNGU_OK) goto out;
		// alloc star img buf
		star_buf = memalign(32, imgProp.imgWidth * imgProp.imgHeight * 4);
		if (!star_buf) goto out;
		// decode
		ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0,
				imgProp.imgWidth, imgProp.imgHeight, 255,
				imgProp.imgWidth, imgProp.imgHeight, star_buf);
		// composite star
		CompositeRGBA(bg_buf, width, height,
			   	COVER_WIDTH - imgProp.imgWidth, 0, // right upper corner
				star_buf, imgProp.imgWidth, imgProp.imgHeight);
		SAFE_FREE(fav_buf);
		SAFE_FREE(star_buf);
		SAFE_PNGU_RELEASE(ctx);
	}

out:
	// draw
	Video_DrawRGBA(COVER_XCOORD, COVER_YCOORD, bg_buf, width, height);
	SAFE_FREE(bg_buf);
}

void Gui_DrawCover(u8 *discid)
{
	void *builtin = coverImg;
	void *imgData;

	s32  ret, size;

	imageNotFound = false;

	if (CFG.covers == 0) return;

	//if (CFG.widescreen) {
	//	builtin = coverImg_wide;
	//}
	imgData = builtin;

	ret = Gui_LoadCover(discid, &imgData, true);
	if (ret <= 0) imgData = builtin;

	size = ret;

	u32 width, height;

	/* Get image dimensions */
	ret = __Gui_GetPngDimensions(imgData, &width, &height);

	/* If invalid image, use default noimage */
	if (ret) {
		imageNotFound = true;
		if (imgData != builtin) free(imgData);
		imgData = builtin;
		ret = __Gui_GetPngDimensions(imgData, &width, &height);
		if (ret) return;
	}

	char *img_buf = NULL;
	char *resize_buf = NULL;
	IMGCTX ctx = NULL;
	img_buf = memalign(32, COVER_WIDTH * COVER_HEIGHT * 4);
	if (!img_buf) { ret = -1; goto out; }

	// Select PNG data
	ctx = PNGU_SelectImageFromBufferX(imgData, size);
	if (!ctx) { ret = -1; goto out; }

	// resize needed?
	if ((width != COVER_WIDTH) || (height != COVER_HEIGHT)) {
		// alloc tmp resize buf
		resize_buf = memalign(32, width * height * 4);
		if (!resize_buf) { ret = -1; goto out; }
		// decode
		ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0,
				width, height, 255, width, height, resize_buf);
		// resize
		ResizeRGBA(resize_buf, width, height, img_buf, COVER_WIDTH, COVER_HEIGHT);
		// discard tmp resize buf
		SAFE_FREE(resize_buf);
	} else {
		// decode
		ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0,
				width, height, 255, width, height, img_buf);
	}

	SAFE_PNGU_RELEASE(ctx);

	DrawCoverImg(discid, img_buf, COVER_WIDTH, COVER_HEIGHT);

out:

	SAFE_FREE(img_buf);
	SAFE_FREE(resize_buf);
	SAFE_PNGU_RELEASE(ctx);

	/* Free memory */
	if (imgData != builtin)
		free(imgData);
}

int Load_Theme_Image(char *name, void **img_buf)
{
	char path[200];
	void *data = NULL;
	int ret;
   	u32	width, height;

	// try theme-specific file
	snprintf(D_S(path), "%s/%s", CFG.theme_path, name);
	ret = Fat_ReadFile(path, &data);
	if (ret > 0 && data) {
		ret = __Gui_GetPngDimensions(data, &width, &height);
		if (ret == 0) {
			*img_buf = data;
			return 0;
		}
		SAFE_FREE(data);
	}
	// failed, try global
	snprintf(D_S(path), "%s/%s", USBLOADER_PATH, name);
	ret = Fat_ReadFile(path, &data);
	if (ret > 0 && data) {
		ret = __Gui_GetPngDimensions(data, &width, &height);
		if (ret == 0) {
			*img_buf = data;
			return 0;
		}
		SAFE_FREE(data);
	}
	// failed
	return -1;
}

s32 __Gui_DrawPngA(void *img, u32 x, u32 y)
{
	IMGCTX   ctx = NULL;
	PNGUPROP imgProp;

	s32 ret;

	/* Select PNG data */
	ctx = PNGU_SelectImageFromBuffer(img);
	if (!ctx) {
		ret = -1;
		goto out;
	}

	/* Get image properties */
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) {
		ret = -1;
		goto out;
	}

	char *img_buf;
	img_buf = memalign(32, imgProp.imgWidth * imgProp.imgHeight * 4);
	if (!img_buf) {
		ret = -1;
		goto out;
	}

	// decode
	ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0, imgProp.imgWidth, imgProp.imgHeight, 255,
			imgProp.imgWidth, imgProp.imgHeight, img_buf);
	// combine
	Video_CompositeRGBA(x, y, img_buf, imgProp.imgWidth, imgProp.imgHeight);
	// draw
	Video_DrawRGBA(x, y, img_buf, imgProp.imgWidth, imgProp.imgHeight);

	SAFE_FREE(img_buf);

	/* Success */
	ret = 0;

out:
	/* Free memory */
	if (ctx)
		PNGU_ReleaseImageContext(ctx);

	return ret;
}

void CopyRGBA(char *src, int srcWidth, int srcHeight,
		char *dest, int dstWidth, int dstHeight,
		int srcX, int srcY, int dstX, int dstY, int w, int h)
{
	int x, y;
	for (y=0; y<h; y++) {
		for (x=0; x<w; x++) {
			int sx = srcX + x;
			int sy = srcY + y;
			int dx = dstX + x;
			int dy = dstY + y;
			if (sx < srcWidth && sy < srcHeight
				&& dx < dstWidth && dy < dstHeight)
			{
				((int*)dest)[dy*dstWidth + dx] = ((int*)src)[sy*srcWidth + sx];
			}
		}
	}
}

void PadRGBA(char *img, int imgWidth, int imgHeight,
	   char *resize, int width, int height)
{
	int x, y;
	for (y=0; y<height; y++) {
		for (x=0; x<width; x++) {
			if (x < imgWidth && y < imgHeight) {
				((int*)resize)[y*width + x] = ((int*)img)[y*imgWidth + x];
			} else {
				((int*)resize)[y*width + x] = 0x00000000;
			}
		}
	}
}

void ResizeRGBA1(char *img, int imgWidth, int imgHeight,
	   char *resize, int width, int height)
{
	int x, y, ix, iy;
	for (y=0; y<height; y++) {
		for (x=0; x<width; x++) {
			ix = x * imgWidth / width;
			iy = y * imgHeight / height;
			((int*)resize)[y*width + x] =
				((int*)img)[iy*imgWidth + ix];
		}
	}
}

// resize img buf to resize buf
void ResizeRGBA(char *img, int imgWidth, int imgHeight,
	   char *resize, int width, int height)
{
	u32 x, y, ix, iy, i;
	u32 rx, ry, nx, ny;
	u32 fix, fnx, fiy, fny;

	for (y=0; y<height; y++) {
		for (x=0; x<width; x++) {
			rx = (x << 8) * imgWidth / width;
			fnx = rx & 0xff;
			fix = 0x100 - fnx;
			ix = rx >> 8;
			nx = ix+1;
			if (nx >= imgWidth) nx = imgWidth - 1;

			ry = (y << 8) * imgHeight / height;
			fny = ry & 0xff;
			fiy = 0x100 - fny;
			iy = ry >> 8;
			ny = iy+1;
			if (ny >= imgHeight) ny = imgHeight - 1;

			// source pixel pointers (syx i=index n=next)
			u8 *rp  = (u8*)&((int*)resize)[y*width + x];
			u8 *sii = (u8*)&((int*)img)[iy*imgWidth + ix];
			u8 *sin = (u8*)&((int*)img)[iy*imgWidth + nx];
			u8 *sni = (u8*)&((int*)img)[ny*imgWidth + ix];
			u8 *snn = (u8*)&((int*)img)[ny*imgWidth + nx];
			// pixel factors (fyx)
			u32 fii = fiy * fix;
			u32 fin = fiy * fnx;
			u32 fni = fny * fix;
			u32 fnn = fny * fnx;
			// pre-multiply factors with alpha
			fii *= (u32)sii[3];
			fin *= (u32)sin[3];
			fni *= (u32)sni[3];
			fnn *= (u32)snn[3];
			// compute alpha
			u32 fff;
			fff = fii + fin + fni + fnn;
			rp[3] = fff >> 16;
			// compute color
			for (i=0; i<3; i++) { // for each R,G,B
				if (fff == 0) {
					rp[i] = 0;
				} else {
					rp[i] = 
						( (u32)sii[i] * fii
						+ (u32)sin[i] * fin
						+ (u32)sni[i] * fni
						+ (u32)snn[i] * fnn
						) / fff;
				}
			}
		}
	}
}


// destination: bg_buf
void CompositeRGBA(char *bg_buf, int bg_w, int bg_h,
		int x, int y, char *img, int width, int height)
{
	int ix, iy;
	char *c, *bg, a;
	c = img;
	for (iy=0; iy<height; iy++) {
		bg = (char*)bg_buf + (y+iy)*(bg_w * 4) + x*4;
		for (ix=0; ix<width; ix++) {
			a = c[3];
			png_composite(bg[0], c[0], a, bg[0]); // r
			png_composite(bg[1], c[1], a, bg[1]); // g
			png_composite(bg[2], c[2], a, bg[2]); // b
			c += 4;
			bg += 4;
		}
	}
}

s32 __Gui_DrawPngRA(void *img, u32 x, u32 y, int width, int height)
{
	IMGCTX   ctx = NULL;
	PNGUPROP imgProp;
	char *img_buf = NULL, *resize_buf = NULL;

	s32 ret;

	/* Select PNG data */
	ctx = PNGU_SelectImageFromBuffer(img);
	if (!ctx) {
		ret = -1;
		goto out;
	}

	/* Get image properties */
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) {
		ret = -1;
		goto out;
	}

	img_buf = memalign(32, imgProp.imgWidth * imgProp.imgHeight * 4);
	if (!img_buf) {
		ret = -1;
		goto out;
	}
	resize_buf = memalign(32, width * height * 4);
	if (!resize_buf) {
		ret = -1;
		goto out;
	}

	// decode
	ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0, imgProp.imgWidth, imgProp.imgHeight, 255,
			imgProp.imgWidth, imgProp.imgHeight, img_buf);
	// resize
	ResizeRGBA(img_buf, imgProp.imgWidth, imgProp.imgHeight, resize_buf, width, height);
	// combine
	Video_CompositeRGBA(x, y, resize_buf, width, height);
	// draw
	Video_DrawRGBA(x, y, resize_buf, width, height);

	/* Success */
	ret = 0;

out:
	if (img_buf) free(img_buf);
	if (resize_buf) free(resize_buf);

	/* Free memory */
	if (ctx)
		PNGU_ReleaseImageContext(ctx);

	return ret;
}


static void RGBA_to_4x4(const unsigned char *src, void *dst,
		const unsigned int width, const unsigned int height)
{
	unsigned int block;
	unsigned int i, c, j;
	unsigned int ar, gb;
	unsigned char *p = (unsigned char*)dst;

	for (block = 0; block < height; block += 4) {
		for (i = 0; i < width; i += 4) {
			/* Alpha and Red */
			for (c = 0; c < 4; ++c) {
				for (ar = 0; ar < 4; ++ar) {
					j = ((i + ar) + ((block + c) * width)) * 4;
					/* Alpha pixels */
					//*p++ = 255;
					*p++ = src[j + 3];
					/* Red pixels */
					*p++ = src[j + 0];
				}
			}

			/* Green and Blue */
			for (c = 0; c < 4; ++c) {
				for (gb = 0; gb < 4; ++gb) {
					j = (((i + gb) + ((block + c) * width)) * 4);
					/* Green pixels */
					*p++ = src[j + 1];
					/* Blue pixels */
					*p++ = src[j + 2];
				}
			}
		} /* i */
	} /* block */
}

static void C4x4_to_RGBA(const unsigned char *src, unsigned char *dst,
		const unsigned int width, const unsigned int height)
{
	unsigned int block;
	unsigned int i, c, j;
	unsigned int ar, gb;
	unsigned char *p = (unsigned char*)src;

	for (block = 0; block < height; block += 4) {
		for (i = 0; i < width; i += 4) {
			/* Alpha and Red */
			for (c = 0; c < 4; ++c) {
				for (ar = 0; ar < 4; ++ar) {
					j = ((i + ar) + ((block + c) * width)) * 4;
					/* Alpha pixels */
					//*p++ = 255;
					dst[j + 3] = *p++;
					/* Red pixels */
					dst[j + 0] = *p++;
				}
			}

			/* Green and Blue */
			for (c = 0; c < 4; ++c) {
				for (gb = 0; gb < 4; ++gb) {
					j = (((i + gb) + ((block + c) * width)) * 4);
					/* Green pixels */
					dst[j + 1] = *p++;
					/* Blue pixels */
					dst[j + 2] = *p++;
				}
			}
		} /* i */
	} /* block */
}


GRRLIB_texImg Gui_LoadTexture_RGBA8(const unsigned char my_png[],
		int width, int height, void *dest)
{
	PNGUPROP imgProp;
	IMGCTX ctx = NULL;
	GRRLIB_texImg my_texture;
	int ret, width4, height4;
	char *buf1 = NULL, *buf2 = NULL;

	memcheck();

	my_texture.data = NULL;
	my_texture.w = 0;
	my_texture.h = 0;
	ctx = PNGU_SelectImageFromBuffer(my_png);
	if (ctx == NULL) goto out;
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) goto out;
   
   	buf1 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);
	if (buf1 == NULL) goto out;

	PNGU_DecodeToRGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, buf1, 0, 255);

	if (imgProp.imgWidth != width || imgProp.imgHeight != height) {
	   	buf2 = memalign (32, width * height * 4);
		if (buf2 == NULL) goto out;
		ResizeRGBA(buf1, imgProp.imgWidth, imgProp.imgHeight, buf2, width, height);
		free(buf1);
		buf1 = buf2;
		buf2 = NULL;
	}

	width4 = (width + 3) >> 2 << 2;
	height4 = (height + 3) >> 2 << 2;
	buf2 = memalign (32, width4 * height4 * 4);
	if (buf2 == NULL) goto out;

	PadRGBA(buf1, width, height, buf2, width4, height4);

	if (dest == NULL) {
		dest = memalign (32, width4 * height4 * 4);
		if (dest == NULL) goto out;
	}
	memcheck();

	RGBA_to_4x4((u8*)buf2, (u8*)dest, width4, height4);

	my_texture.data = dest;
	my_texture.w = width4;
	my_texture.h = height4;
	GRRLIB_FlushTex(my_texture);
	out:
	SAFE_FREE(buf1);
	SAFE_FREE(buf2);
	if (ctx) PNGU_ReleaseImageContext(ctx);
	return my_texture;
}


/**
 * Method used to convert a RGBA8 image into CMPR
 */
GRRLIB_texImg Convert_to_CMPR(void *img, int width, int height, void *dest)
{
	GRRLIB_texImg my_texture;
	int ret;
	void *buf1 = NULL;

	my_texture.data = NULL;
	my_texture.w = 0;
	my_texture.h = 0;

	buf1 = memalign (32, width * height * 4);
	if (buf1 == NULL) goto out;
	RGBA_to_4x4((u8*)img, (u8*)buf1, width, height);

	if (dest == NULL) {
		dest = memalign (32, (width * height) / 2);
		if (dest == NULL) goto out;
	}
	
	ret = PNGU_RGBA8_To_CMPR(buf1, width, height, dest);
	if (ret != PNGU_OK) goto out;
	
	my_texture.data = dest;
	my_texture.w = width;
	my_texture.h = height;
	GRRLIB_FlushTex(my_texture);

	out:
	SAFE_FREE(buf1);
	return my_texture;
}


/**
 * Converts a PNG to CMPR
 *
 * @param my_png[] PNG image
 * @param width png image width
 * @param height png image height
 * @param *dest image destination (CMPR-encoded)
 */
GRRLIB_texImg Gui_LoadTexture_CMPR(const unsigned char my_png[],
		int width, int height, void *dest)
{
	PNGUPROP imgProp;
	IMGCTX ctx = NULL;
	GRRLIB_texImg my_texture;
	int ret;
	void *buf1 = NULL;
	void *buf2 = NULL;
	u32 imgheight, imgwidth;

	my_texture.data = NULL;
	my_texture.w = 0;
	my_texture.h = 0;

	ctx = PNGU_SelectImageFromBuffer(my_png);
	if (ctx == NULL) goto out;
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) goto out;
   
	//CMPR is divisible by 8
	imgheight = imgProp.imgHeight & ~7u;
	imgwidth = imgProp.imgWidth & ~7u;
	
	//check if we need to resize
	if ((imgheight != height) || (imgwidth != width)) {
		//get RGBA8 version of image
		buf1 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);
		if (buf1 == NULL) goto out;
		PNGU_DecodeToRGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, buf1, 0, 255);
	   	
		//allocate for resize to passed in dimensions
		buf2 = memalign (32, width * height * 4);
		if (buf2 == NULL) goto out;		
		ResizeRGBA(buf1, imgProp.imgWidth, imgProp.imgHeight, buf2, width, height);
		free(buf1);
		buf1 = buf2;
		buf2 = NULL;

		my_texture = Convert_to_CMPR(buf1, width, height, dest);		

	} else {
		if (dest == NULL) {
			dest = memalign (32, (imgheight * imgwidth) / 2);
			if (dest == NULL) goto out;
		}
		ret = PNGU_DecodeToCMPR (ctx, imgProp.imgWidth, imgProp.imgHeight, dest);
		if (ret != PNGU_OK) goto out;

		my_texture.w = width;
		my_texture.h = height;
		my_texture.data = dest;
		GRRLIB_FlushTex(my_texture);
	}

	out:
	SAFE_FREE(buf1);
	SAFE_FREE(buf2);
	if (ctx) PNGU_ReleaseImageContext(ctx);
	return my_texture;
}


/**
 * Method used by the cache to paste 2d cover images into the nocover_full image
 */
GRRLIB_texImg Gui_LoadTexture_fullcover(const unsigned char my_png[],
		int width, int height, int frontCoverWidth, void *dest)
{
	PNGUPROP imgProp;
	IMGCTX ctx = NULL;
	GRRLIB_texImg my_texture;
	int ret;
	int frontCoverStart = t2_nocover_full.tx.w - frontCoverWidth;
	void *buf1 = NULL;
	void *buf2 = NULL;
	void *buf3 = NULL;

	my_texture.data = NULL;
	my_texture.w = 0;
	my_texture.h = 0;

	ctx = PNGU_SelectImageFromBuffer(my_png);
	if (ctx == NULL) goto out;
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) goto out;
	
	//allocate for RGBA8 2D cover
   	buf1 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);
	if (buf1 == NULL) goto out;
	PNGU_DecodeToRGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, buf1, 0, 255);

	//check if we need to resize
	if (imgProp.imgWidth != frontCoverWidth || imgProp.imgHeight != height) {
	   	buf2 = memalign (32, frontCoverWidth * height * 4);
		if (buf2 == NULL) goto out;
		ResizeRGBA(buf1, imgProp.imgWidth, imgProp.imgHeight, buf2, frontCoverWidth, height);
		free(buf1);
		buf1 = buf2;
		buf2 = NULL;
	}

	//allocate for RGBA8 nocover_full image
	buf2 = memalign (32, t2_nocover_full.size);
	if (buf2 == NULL) goto out;
	C4x4_to_RGBA(t2_nocover_full.tx.data, buf2, t2_nocover_full.tx.w, t2_nocover_full.tx.h);
	
	//check if we need to resize
	if (t2_nocover_full.tx.w != width || t2_nocover_full.tx.h != height) {
		buf3 = memalign (32, height * width * 4);
		if (buf3 == NULL) goto out;
		ResizeRGBA(buf2, t2_nocover_full.tx.w, t2_nocover_full.tx.h, buf3, width, height);
		free(buf2);
		buf2 = buf3;
		buf3 = NULL;
		SAFE_FREE(buf3);
	}
	
	//the full cover image is layed out like this:  back | spine | front
	//so we want to paste the passed in image in the front section
	CompositeRGBA(buf2, width, height,
			frontCoverStart, 0, //place where front cover is located
			buf1, frontCoverWidth, height);

	if (CFG.gui_compress_covers) {
		my_texture = Convert_to_CMPR(buf2, width, height, dest);
	} else {
		RGBA_to_4x4((u8*)buf2, (u8*)dest, width, height);
		my_texture.data = dest;
		my_texture.w = width;
		my_texture.h = height;
		GRRLIB_FlushTex(my_texture);
	}
	
	out:
	SAFE_FREE(buf1);
	SAFE_FREE(buf2);
	if (ctx) PNGU_ReleaseImageContext(ctx);
	return my_texture;
}


/**
 * Method used to paste the src image into the dest image (fullcover size)
 */
GRRLIB_texImg Gui_paste_into_fullcover(void *src, int src_w, int src_h, 
		void *dest, int dest_w, int dest_h)
{
	int height, width;
	void *buf1 = NULL;
	void *buf2 = NULL;
	GRRLIB_texImg my_texture;

	my_texture.w = 0;
	my_texture.h = 0;
	my_texture.data = NULL;
	
	if (!dest) goto out;
	
	buf1 = memalign (32, src_w * src_h * 4);
	if (buf1 == NULL) goto out;
	C4x4_to_RGBA(src, buf1, src_w, src_h);

	buf2 = memalign (32, dest_w * dest_h * 4);
	if (buf2 == NULL) goto out;
	C4x4_to_RGBA(dest, buf2, dest_w, dest_h);
	
	//the full cover image is layed out like this:  back | spine | front
	//so we want to paste the passed in image in the front section
	CompositeRGBA(buf2, dest_w, dest_h,
			dest_w - (COVER_WIDTH_FRONT/2) - (src_w/2), (dest_h/2) - (src_h/2), //place where front cover is located
			buf1, src_w, src_h);
	
	SAFE_FREE(buf1);
	
	if (CFG.gui_compress_covers) {
		//CMPR is divisible by 8
		height = dest_h & ~7u;
		width = dest_w & ~7u;
		my_texture = Convert_to_CMPR(buf2, width, height, buf1);
	} else {
		buf1 = memalign(32, dest_w * dest_h * 4);
		RGBA_to_4x4((u8*)buf2, (u8*)buf1, dest_w, dest_h);
		my_texture.data = buf1;
		my_texture.w = dest_w;
		my_texture.h = dest_h;
		GRRLIB_FlushTex(my_texture);
	}
	
	out:;
	SAFE_FREE(buf2);
	return my_texture;
}


void cache2_tex(struct M2_texImg *dest, GRRLIB_texImg *src)
{
	int src_size = src->w * src->h * 4;
	void *m2_buf;
	if (src->data == NULL) return;
	// data exists and big enough?
	if (dest->tx.data && dest->size >= src_size) {
		// overwrite data
		m2_buf = dest->tx.data;
	} else {
		// alloc new
		m2_buf = LARGE_memalign(32, src_size);
		dest->size = src_size;
	}
	memcpy(m2_buf, src->data, src_size);
	memcpy(&dest->tx, src, sizeof (GRRLIB_texImg));
	dest->tx.data = m2_buf;
	// free src texture
	SAFE_FREE(src->data);
	src->w = src->h = 0;
	GRRLIB_FlushTex(dest->tx);
}

void cache2_tex_alloc(struct M2_texImg *dest, int w, int h)
{
	int src_size = w * h * 4;
	void *m2_buf;
	if (dest->tx.data) return;
	// alloc new
	m2_buf = LARGE_memalign(32, src_size);
	if (m2_buf == NULL) return;
	dest->tx.data = m2_buf;
	dest->tx.w = w;
	dest->tx.h = h;
	dest->size = src_size;
}

void cache2_tex_alloc_fullscreen(struct M2_texImg *dest)
{
	GXRModeObj *rmode = _Video_GetVMode();
	cache2_tex_alloc(dest, rmode->fbWidth, rmode->efbHeight);
}

void cache2_GRRLIB_tex_alloc(GRRLIB_texImg *dest, int w, int h)
{
	int src_size = w * h * 4;
	void *m2_buf;
	if (dest->data) return;
	// alloc new
	m2_buf = LARGE_memalign(32, src_size);
	if (m2_buf == NULL) return;
	dest->data = m2_buf;
	dest->h = h;
	dest->w = w;
}

void cache2_GRRLIB_tex_alloc_fullscreen(GRRLIB_texImg *dest)
{
	GXRModeObj *rmode = _Video_GetVMode();
	cache2_GRRLIB_tex_alloc(dest, rmode->fbWidth, rmode->efbHeight);
}

void Gui_Render()
{
	_GRRLIB_Render();
	GX_DrawDone();
	//__console_disable=0; __console_flush(-1); // debug
	_GRRLIB_VSync();
	if (gui_style == GUI_STYLE_FLOW_Z) {
		GX_SetZMode (GX_FALSE, GX_NEVER, GX_TRUE);
	}
}

/**
 * Renders the current scene to a texture and stores it in the global aa_texBuffer array.
 *  @param aaStep int position in the array to store the created tex
 *  @return void
 */
void Gui_RenderAAPass(int aaStep) {
	if (aa_texBuffer[aaStep].data==NULL) {
		// first time, allocate
		int i;
		for (i=0; i<CFG.gui_antialias; i++) {
			// first time, allocate
			cache2_GRRLIB_tex_alloc_fullscreen(&aa_texBuffer[i]);
		}
	}
	GRRLIB_AAScreen2Texture_buf(&aa_texBuffer[aaStep]);
}

void Gui_Init()
{
	if (CFG.gui == 0) return;
	if (grr_init) return;

	VIDEO_WaitVSync();
	if (GRRLIB_Init_VMode(_Video_GetVMode(), _Video_GetFB(0), _Video_GetFB(1)))
	{
		printf("Error GRRLIB init\n");
		sleep(1);
		return;
	}
	VIDEO_WaitVSync();
	grr_init = 1;
}

bool Load_Theme_Texture_1(char *name, GRRLIB_texImg *tx)
{
	void *data = NULL;
	int ret;

	SAFE_FREE(tx->data);
	ret = Load_Theme_Image(name, &data);
	if (ret == 0 && data) {
		*tx = GRRLIB_LoadTexture(data);
		SAFE_FREE(data);
		if (tx->data) return true;
	}
	return false;
}
void Load_Theme_Texture(char *name, GRRLIB_texImg *tx, void *builtin)
{
	if (Load_Theme_Texture_1(name, tx)) return;
	// failed, use builtin
	*tx = GRRLIB_LoadTexture(builtin);
}

void GRRLIB_CopyTextureBlock(GRRLIB_texImg *src, int x, int y, int w, int h,
		GRRLIB_texImg *dest, int dx, int dy)
{
	int ix, iy;
	u32 c;
	for (ix=0; ix<w; ix++) {
		for (iy=0; iy<h; iy++) {
			c = GRRLIB_GetPixelFromtexImg(x+ix, y+iy, *src);
			GRRLIB_SetPixelTotexImg(dx+ix, dy+iy, *dest, c);
		}
	}
}

void GRRLIB_RearrangeFont128(GRRLIB_texImg *tx)
{
	int tile_w = tx->w / 128;
	int tile_h = tx->h;
	if (tx->w <= 1024) return;
	// split to max 1024 width stripes
	GRRLIB_texImg tt;
	int i;
	int c_per_stripe;
	int nstripe;
	int stripe_w;
	// max number of characters per stripe
	c_per_stripe = 1024 / tile_w;
	// round down to a multiple of 4, so that the texture size
	// is a multiple of 4 as well.
	c_per_stripe = c_per_stripe >> 2 << 2;
	// number of stripes
	nstripe = (128 + c_per_stripe - 1) / c_per_stripe;
	// stripe width
	stripe_w = c_per_stripe * tile_w;
	tt = GRRLIB_CreateEmptyTexture(stripe_w, tile_h * nstripe);
	for (i=0; i<nstripe; i++) {
		GRRLIB_CopyTextureBlock(tx, stripe_w * i, 0, stripe_w, tile_h,
				&tt, 0, tile_h * i);
	}
	free(tx->data);
	*tx = tt;
	tt.data = NULL;
}

void GRRLIB_InitFont(GRRLIB_texImg *tx)
{
	int tile_w;
	int tile_h;

	if (!tx->data) return;

	// detect font image type: 128x1 or 32x16 (512)
	if (tx->w / tx->h >= 32) {
		tile_w = tx->w / 128;
		tile_h = tx->h;
		// 128x1 font
		if (tx->w > 1024) {
			// split to max 1024 width stripes
			GRRLIB_RearrangeFont128(tx);
		}
	} else {
		// 32x16 font
		tile_w = tx->w / 32;
		tile_h = tx->h / 16;
	}
	//Gui_Console_Enable();
	//printf("\n%d %d %d %d\n", tx->w, tx->h, tile_w, tile_h);
	//Wpad_WaitButtons();

	// init tile
	GRRLIB_InitTileSet(tx, tile_w, tile_h, 0);
	GRRLIB_FlushTex(*tx);
}

void GRRLIB_TrimTile(GRRLIB_texImg *tx, int maxn)
{
	if (!tx || !tx->data) return;
	int n = tx->nbtilew * tx->nbtileh;
	if (n <= maxn) return;
	// roundup to a full row
	tx->nbtileh = (maxn + tx->nbtilew - 1) / tx->nbtilew;
	tx->h = tx->tileh * tx->nbtileh;
	int size = tx->w * tx->h * 4;
	tx->data = realloc(tx->data, size + 64 - (size%32));
}

void Gui_TestUnicode()
{
	if (CFG.debug != 2) return;
	wchar_t wc;
	char mb[32*6+1];
	char line[10+32*6+1];
	int i, k, len=0, count=0, y=0;
	int fh = tx_font.tileh;
	for (i=0; i<512; i++) {
		wc = i;
		k = wctomb(mb+len, wc);
		if (k < 1 || mb[len] == 0) {
			mb[len] = wc < 128 ? '!' : '?';
			k = 1;
		}
		len += k;
		mb[len] = 0;
		count++;
		if (count >= 32) {
			sprintf(line, "%03x: %s |", (unsigned)i-31, mb);
			y = 10+(i/32*fh);
			Gui_Print2(5, y, line);
			len = 0;
			count = 0;
		}
	}
	for (i=0; ufont_map[i]; i+=2) {
		wc = ufont_map[i];
		k = wctomb(mb+len, wc);
		if (k < 1 || mb[len] == 0) {
			mb[len] = wc < 128 ? '!' : '?';
			k = 1;
		}
		len += k;
		mb[len] = 0;
		count++;
		if (count >= 32 || !ufont_map[i+2]) {
			sprintf(line, "map: %s |", mb);
			y += fh;
			Gui_Print2(5, y, line);
			len = 0;
			count = 0;
		}
	}
	y += fh;
	Gui_Print2(5, y, "Press any button");
	Gui_Render();
	Wpad_WaitButtons();
}

void Gui_PrintEx2(int x, int y, GRRLIB_texImg font,
		int color, int color_outline, int color_shadow, char *str)
{
	GRRLIB_Print2(x, y, font, color, color_outline, color_shadow, str);
}


int get_outline_color(int text_color, int outline_color, int outline_auto)
{
	unsigned color, alpha;
	if (!outline_color) return 0;

	if (outline_auto) {
		// only alpha specified
		int avg;
		avg = ( ((text_color>>8) & 0xFF)
		      + ((text_color>>16) & 0xFF)
		      + ((text_color>>24) & 0xFF) ) / 3;
		if (avg > 0x80) {
			color = 0x00000000;
		} else {
			color = 0xFFFFFF00;
		}
	} else {
		// full color specified
		color = outline_color;
	}
	// apply text alpha to outline alpha
	alpha = (outline_color & 0xFF) * (text_color & 0xFF) / 0xFF;
	color = (color & 0xFFFFFF00) | alpha;
	return color;
}

void Gui_PrintEx(int x, int y, GRRLIB_texImg font, FontColor font_color, char *str)
{
	unsigned outline_color = get_outline_color(
			font_color.color,
			font_color.outline,
			font_color.outline_auto); 
	unsigned shadow_color = get_outline_color(
			font_color.color,
			font_color.shadow,
			font_color.shadow_auto); 
	Gui_PrintEx2(x, y, font, font_color.color, outline_color, shadow_color, str);
}

void Gui_PrintfEx(int x, int y, GRRLIB_texImg font, FontColor font_color, char *fmt, ...)
{
	char str[200];
	va_list argp;
	va_start(argp, fmt);
	vsnprintf(str, sizeof(str), fmt, argp);
	va_end(argp);
	Gui_PrintEx(x, y, font, font_color, str);
}

// alignx: -1=left 0=centre 1=right
// aligny: -1=top  0=centre 1=bottom
void Gui_PrintAlign(int x, int y, int alignx, int aligny,
		GRRLIB_texImg font, FontColor font_color, char *str)
{
	int xx = x, yy = y;
	int len, w, h;
	len = strlen(str);
	w = len * font.tilew;
	h = font.tileh;
	if (alignx == 0) xx = x - w/2;
	else if (alignx > 0) xx = x - w;
	if (aligny == 0) yy = y - h/2;
	else if (aligny > 0) yy = y - h;
	Gui_PrintEx(xx, yy, font, font_color, str);
}

void Gui_Print2(int x, int y, char *str)
{
	Gui_PrintEx(x, y, tx_font, CFG.gui_text2, str);
}

void Gui_Print(int x, int y, char *str)
{
	Gui_PrintEx(x, y, tx_font, CFG.gui_text, str);
}

void Gui_Printf(int x, int y, char *fmt, ...)
{
	char str[200];
	va_list argp;
	va_start(argp, fmt);
	vsnprintf(str, sizeof(str), fmt, argp);
	va_end(argp);
	Gui_Print(x, y, str);
}

void Gui_Print_Clock(int x, int y, FontColor font_color, time_t t)
{
	GRRLIB_texImg *tx = &tx_font_clock;
	if (!tx->data) tx = &tx_font;
	//Gui_PrintEx(x, y, *tx, font_color, get_clock_str(t));
	Gui_PrintAlign(x, y, 0, 0, *tx, font_color, get_clock_str(t));
}

void Grx_Load_BG_Gui()
{
	void *img_buf = NULL;

	// widescreen?
	if (CFG.widescreen && *CFG.bg_gui_wide) {
		img_buf = Gui_LoadBG(CFG.bg_gui_wide);
	}
	// or normal
	if (img_buf == NULL) {
		img_buf = Gui_LoadBG(CFG.bg_gui);
	}
	// or builtin
	if (img_buf == NULL) {
		img_buf = Gui_LoadBG_data(bg_gui);
	}

	// overlay
	int ret = -1;
	char path[200];

	if (CFG.widescreen) {
		snprintf(D_S(path), "%s/bg_gui_over_w.png", CFG.theme_path);
		ret = Gui_OverlayBg(path, img_buf);
	}
   	if (ret) {
		snprintf(D_S(path), "%s/bg_gui_over.png", CFG.theme_path);
		ret = Gui_OverlayBg(path, img_buf);
	}

	// texture
	GRRLIB_texImg tx_tmp;
	tx_tmp = GRRLIB_CreateEmptyTexture(BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
	if (img_buf && tx_tmp.data) {
		RGBA_to_4x4((u8*)img_buf, (u8*)tx_tmp.data, BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
	}
	cache2_tex(&t2_bg, &tx_tmp);
	SAFE_FREE(img_buf);
}


/**
 * Initilaizes all the common images (no cover, pointer, font, background...)
 *
 */
void Grx_Init()
{
	int ret;
	GRRLIB_texImg tx_tmp;

	// detect theme change
	extern int cur_theme;
	static int last_theme = -1;
	bool theme_change = (last_theme != cur_theme);
	last_theme = cur_theme;

	// on cover_style change, need to reload noimage cover
	// (changing cover style invalidates cache and sets ccache_inv)
	if (!grx_init || ccache_inv) {

		// nocover image
		void *img = NULL;
		tx_tmp.data = NULL;
		ret = Gui_LoadCover((u8*)"", &img, true);
		if (ret > 0 && img) {
			tx_tmp = Gui_LoadTexture_RGBA8(img, COVER_WIDTH, COVER_HEIGHT, NULL);
			free(img);
		}
		if (tx_tmp.data == NULL) {
			tx_tmp = Gui_LoadTexture_RGBA8(coverImg, COVER_WIDTH, COVER_HEIGHT, NULL);
		}
		cache2_tex(&t2_nocover, &tx_tmp);

		//full nocover image
		tx_tmp = GRRLIB_LoadTexture(coverImg_full);
		cache2_tex(&t2_nocover_full, &tx_tmp);
	}

	// on theme change we need to reload all gui resources
	if (!grx_init || theme_change) {

		// background gui
		Grx_Load_BG_Gui();

		// background console
		tx_tmp = GRRLIB_CreateEmptyTexture(BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
		if (bg_buf_rgba) {
			RGBA_to_4x4((u8*)bg_buf_rgba, (u8*)tx_tmp.data,
				BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
		}
		cache2_tex(&t2_bg_con, &tx_tmp);

		//tx_pointer = GRRLIB_LoadTexture(pointer);
		Load_Theme_Texture("pointer.png", &tx_pointer, pointer);

		//tx_hourglass = GRRLIB_LoadTexture(hourglass);
		Load_Theme_Texture("hourglass.png", &tx_hourglass, hourglass);

		//tx_star = GRRLIB_LoadTexture(star_icon);
		Load_Theme_Texture("favorite.png", &tx_star, star_icon);

		// Load the image file and initilise the tiles for gui font
		// default: font_uni.png
		if (!Load_Theme_Texture_1(CFG.gui_font, &tx_font)) {
			Load_Theme_Texture("font.png", &tx_font, gui_font);
		}
		GRRLIB_InitFont(&tx_font);

		// if font not found, tx_font_clock.data will be NULL
		Load_Theme_Texture_1("font_clock.png", &tx_font_clock);
		GRRLIB_InitFont(&tx_font_clock);
		GRRLIB_TrimTile(&tx_font_clock, 128);
	}

	grx_init = 1;
}


void gui_tilt_pos(Vector *pos)
{
	if (gui_style == GUI_STYLE_FLOW_Z) {
		// tilt pos
		float deg_max = 45.0;
		float deg = cam_f * cam_dir * deg_max;
		float rad = DegToRad(deg);
		float zf;
		Mtx rot;

		pos->x -= cam_look.x;
		pos->y -= cam_look.y;

		guMtxRotRad(rot, 'y', rad);
		guVecMultiply(rot, pos, pos);

		zf = (cam_z + pos->z) / cam_z;
		pos->x /= zf;
		pos->y /= zf;

		pos->x += cam_look.x;
		pos->y += cam_look.y;
	}
}

void tilt_cam(Vector *cam)
{
	if (gui_style == GUI_STYLE_FLOW_Z) {
		// tilt cam
		float deg_max = 45.0;
		float deg = cam_f * cam_dir * deg_max;
		float rad = DegToRad(deg);
		Mtx rot;

		cam->x -= cam_look.x;
		cam->y -= cam_look.y;

		guMtxRotRad(rot, 'y', rad);
		guVecMultiply(rot, cam, cam);

		cam->x += cam_look.x;
		cam->y += cam_look.y;
	}
}

void Gui_set_camera(ir_t *ir, int enable)
{
	float hold_w = 50.0, out = 64; // same as get_scroll() params
	float sx;
	if (ir) {
		// is out?
		// allow x out of screen by pointer size
		if (ir->sy < 0 || ir->sy > BACKGROUND_HEIGHT
			|| ir->sx < -out || ir->sx > BACKGROUND_WIDTH+out)
		{
			if (cam_f > 0.0) cam_f -= 0.02;
			if (cam_f < 0.0) cam_f = 0.0;
		} else {
			sx = ir->sx;
			if (sx < 0.0) sx = 0.0;
			if (sx > 640.0) sx = 640.0;
			sx = sx - 320.0;
			// check hold position
			if (sx < 0) {
				cam_dir = -1.0;
				sx = -sx;
			} else {
				cam_dir = 1.0;
			}
			sx -= hold_w;
			if (sx < 0) sx = 0;
			cam_f = sx / (320.0 - hold_w);
		}
	}

	if (gui_style == GUI_STYLE_GRID || gui_style == GUI_STYLE_FLOW) {
		enable = 0;
	}
	if (enable == 0) {
		// 2D
		Mtx44 perspective;
		guOrtho(perspective,0, 480, 0, 640, 0, 300.0F);
		GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

		guMtxIdentity(GXmodelView2D);
		guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -50.0F);
		GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);
	} else { // (gui_style == GUI_STYLE_FLOW_Z)
		// 3D
		Mtx44 perspective;
		guPerspective(perspective, 45, 4.0/3.0, 0.1F, 2000.0F);
		GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

		//Vector cam  = {319.5F, 239.5F, -578.0F};
		//Vector look = {319.5F, 239.5F, 0.0F};
		Vector up   = {0.0F, -1.0F, 0.0F};
		Vector cam  = cam_look;
		cam.z = cam_z;

		// tilt cam
		tilt_cam(&cam);

		guLookAt(GXmodelView2D, &cam, &up, &cam_look);
		GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);
	}
	if (gui_style == GUI_STYLE_FLOW_Z) {
		GX_SetZMode (GX_FALSE, GX_NEVER, GX_TRUE);
	}
}


/**
 * Resets the cover style to the previous one.  Primarily used when 
 *  switching from full covers.
 */
void Gui_reset_previous_cover_style() {
	//flip the cover style mode back to whatever it was
	cache_release_all();
	cache_wait_idle();
	cfg_set_cover_style(prev_cover_style);
	cfg_setup_cover_style();
	//set cover width and height back to original
	COVER_HEIGHT = prev_cover_height;
	COVER_WIDTH = prev_cover_width;
}


void Gui_draw_background_alpha2(u32 color1, u32 color2)
{
	Gui_set_camera(NULL, 0);
	//void GRRLIB_DrawSlice2(f32 xpos, f32 ypos, GRRLIB_texImg tex, float degrees,
	//	float scaleX, f32 scaleY, u32 color1, u32 color2,
	//	float x, float y, float w, float h)

	// top half
	GRRLIB_DrawSlice2(0, 0, t2_bg.tx, 0,
		1, 1, color1, color1,
		0, 0, t2_bg.tx.w, t2_bg.tx.h / 2);

	// bottom half
	GRRLIB_DrawSlice2(0, t2_bg.tx.h / 2, t2_bg.tx, 0,
		1, 1, color1, color2,
		0, t2_bg.tx.h / 2, t2_bg.tx.w, t2_bg.tx.h / 2);

	Gui_set_camera(NULL, 1);
}

void Gui_draw_background_alpha(u32 color)
{
	Gui_set_camera(NULL, 0);
	GRRLIB_DrawImg(0, 0, t2_bg.tx, 0, 1, 1, color);
	Gui_set_camera(NULL, 1);
}

void Gui_draw_background()
{
	Gui_draw_background_alpha(0xFFFFFFFF);
}

void Gui_draw_pointer(ir_t *ir)
{
	// reset camera
	Gui_set_camera(NULL, 0);
	// draw pointer
	GRRLIB_DrawImg(ir->sx - tx_pointer.w / 2, ir->sy - tx_pointer.h / 2,
			tx_pointer, ir->angle, 1, 1, 0xFFFFFFFF);
}

void Repaint_ConBg(bool exiting)
{
	int fb;
	void **xfb;
	GRRLIB_texImg tx;
	void *img_buf;

	xfb = _GRRLIB_GetXFB(&fb);
	_Video_SetFB(xfb[fb^1]);
	Video_Clear(COLOR_BLACK);

	if (exiting) return;
	
	// background
	Video_DrawBg();

	// enable console if it was disabled till now
	Gui_Console_Enable();

	// Cover
	if (CFG.covers == 0) return;
	if (gameCnt == 0) return;
	
	int actual_i = cache_game_find(gameSelected);
	if (ccache.game[actual_i].state == CS_PRESENT) {
		tx = ccache.cover[ccache.game[actual_i].cid].tx;
		if ((tx.w != COVER_WIDTH) || (tx.h != COVER_HEIGHT)) {
			extern void __Menu_ShowCover();
			__Menu_ShowCover();
			return;
		}
	} else if (ccache.game[actual_i].state == CS_MISSING) {
		tx = t2_nocover.tx;
	} else { // loading
		extern void __Menu_ShowCover();
		__Menu_ShowCover();
		return;
	}
	img_buf = memalign(32, tx.w * tx.h * 4);
	if (img_buf == NULL) return;
	C4x4_to_RGBA(tx.data, img_buf, tx.w, tx.h);
	DrawCoverImg(gameList[gameSelected].id, img_buf, tx.w, tx.h);
	//Video_CompositeRGBA(COVER_XCOORD, COVER_YCOORD, img_buf, tx.w, tx.h);
	//Video_DrawRGBA(COVER_XCOORD, COVER_YCOORD, img_buf, tx.w, tx.h);
	free(img_buf); 
}

/**
 * This is the main control loop for the 3D cover modes.
 */
int Gui_Mode()
{
	int buttons = 0;
	ir_t ir;
	int fr_cnt = 0;
	bool exiting = false;
	bool suppressWiiPointerChecking = false;
	int ret = 0;

	//check for console mode
	if (CFG.gui == 0) return 0;
	
	memcheck();
	//load all the commonly used images (background, pointers, etc)
	Grx_Init();

	memcheck();

	// setup gui style
	static int first_time = 1;
	if (first_time) {
		gui_style = CFG.gui_style;
		// CFG.gui_style has all the styles unified
		// split coverflow styles out:
		if (gui_style >= GUI_STYLE_COVERFLOW) {
			gui_style = GUI_STYLE_COVERFLOW;
			CFG_cf_global.theme = CFG.gui_style - GUI_STYLE_COVERFLOW;
		}
		grid_rows = CFG.gui_rows;
	}

	prev_cover_style = CFG.cover_style;
	prev_cover_height = COVER_HEIGHT;
	prev_cover_width = COVER_WIDTH;

	if (gui_style == GUI_STYLE_COVERFLOW) {

		if (CFG_cf_global.covers_3d) {
			if (CFG.cover_style != CFG_COVER_STYLE_FULL) {
				cfg_set_cover_style(CFG_COVER_STYLE_FULL);
				cfg_setup_cover_style();
			}
		} else {
			if (CFG.cover_style != CFG_COVER_STYLE_2D) {
				cfg_set_cover_style(CFG_COVER_STYLE_2D);
				cfg_setup_cover_style();
			}
		}
		
		//start the cache thread
		if (Cache_Init()) return 0;
		memcheck();

		Coverflow_Grx_Init();

		__console_flush(0);
		VIDEO_Flush();
		VIDEO_WaitVSync();

		//initilaize the GRRLIB video subsystem
		Gui_Init();

		game_select = -1;
		
		gameSelected = Coverflow_initCoverObjects(gameCnt, gameSelected, false);

		//load the covers - alternate right and left
		cache_request_before_and_after(gameSelected, 5, 1);
		
		if (!first_time) {
			if (prev_cover_style == CFG_COVER_STYLE_3D)
				Coverflow_init_transition(CF_TRANS_MOVE_FROM_CONSOLE_3D, 100, gameCnt, false);
			else
				Coverflow_init_transition(CF_TRANS_MOVE_FROM_CONSOLE, 100, gameCnt, false);
		}
		
	} else {
	
		//start the cache thread
		if (Cache_Init()) return 0;
		memcheck();

		//allocate the grid and initialize the grid cover settings
		grid_init(gameSelected);
	
		__console_flush(0);
		VIDEO_Flush();
		VIDEO_WaitVSync();

		//initilaize the GRRLIB video subsystem
		Gui_Init();

		game_select = -1;
	
		if (!first_time) {
			//fade in the grid
			grid_transition(1, page_gi);
		}
	}
	
	if (first_time) {
		first_time = 0;
		Gui_TestUnicode();
	}

	while (1) {

		restart:

		//dbg_time1();

		buttons = Wpad_GetButtons();
		Wpad_getIR(WPAD_CHAN_0, &ir);


		//----------------------
		//  HOME Button
		//----------------------

		if (buttons & WPAD_BUTTON_HOME) {
			if (CFG.home == CFG_HOME_SCRSHOT) {
				char fn[200];
				extern bool Save_ScreenShot(char *fn, int size);
				Save_ScreenShot(fn, sizeof(fn));
			} else {
				exiting = true;
				__console_disable = 1;
				break;
			}
		}


		//----------------------
		//  A Button
		//----------------------

		if (loadGame) {
			buttons = WPAD_BUTTON_A;
			loadGame = false;
		}

		if (buttons & WPAD_BUTTON_A) {
			if (game_select >= 0) {
				gameSelected = game_select;
				break;
			}
		}


		//----------------------
		//  B Button
		//----------------------

		if (buttons & WPAD_BUTTON_B) {
			if (game_select >= 0) gameSelected = game_select;
			break;
		}


		//----------------------
		//  dpad RIGHT
		//----------------------

		if (buttons & WPAD_BUTTON_RIGHT) {
			if (gui_style == GUI_STYLE_COVERFLOW) {
				ret = Coverflow_init_transition(CF_TRANS_ROTATE_RIGHT, 0, gameCnt, true);
				if (ret > 0) {
					game_select = ret;
					cache_release_all();
					cache_request_before_and_after(game_select, 5, 1);
				}
				suppressWiiPointerChecking = true;
			} else {
				if (grid_page_change(1)) goto restart;
			}
		} else if (Wpad_Held(0) & WPAD_BUTTON_RIGHT) {
			if (gui_style == GUI_STYLE_COVERFLOW) {
				ret = Coverflow_init_transition(CF_TRANS_ROTATE_RIGHT, 100, gameCnt, true);
				if (ret > 0) {
					game_select = ret;
					cache_release_all();
					cache_request_before_and_after(game_select, 5, 1);
				}
				suppressWiiPointerChecking = true;
			}
		}


		//----------------------
		//  dpad LEFT
		//----------------------

		if (buttons & WPAD_BUTTON_LEFT) {
			if (gui_style == GUI_STYLE_COVERFLOW) {
				ret = Coverflow_init_transition(CF_TRANS_ROTATE_LEFT, 0, gameCnt, true);
				if (ret > 0) {
					game_select = ret;
					cache_release_all();
					cache_request_before_and_after(game_select, 5, 1);
				}
				suppressWiiPointerChecking = true;
			} else {
				if (grid_page_change(-1)) goto restart;
			}
		} else if (Wpad_Held(0) & WPAD_BUTTON_LEFT) {
			if (gui_style == GUI_STYLE_COVERFLOW) {
				ret = Coverflow_init_transition(CF_TRANS_ROTATE_LEFT, 100, gameCnt, true);
				if (ret > 0) {
					game_select = ret;
					cache_release_all();
					cache_request_before_and_after(game_select, 5, 1);
				}
				suppressWiiPointerChecking = true;
			}
		}


		//----------------------
		//  dpad UP
		//----------------------
		
		if (buttons & WPAD_BUTTON_UP) {
			if (gui_style == GUI_STYLE_COVERFLOW) {
				game_select = Coverflow_flip_cover_to_back(true, 2);
			} else {
				//grid_change_rows(rows+1);
				if (CFG.gui_lock) {
					if (grid_rows < 4) {
						grid_transit_style(1);
						goto restart;
					}
				} else {
					grid_transit_style(1);
					goto restart;
				}
			}
		}

		
		//----------------------
		//  dpad DOWN
		//----------------------

		if (buttons & WPAD_BUTTON_DOWN) {
		
			if (game_select >= 0) gameSelected = game_select;

			if (CFG.gui_lock) {
				if (gui_style != GUI_STYLE_COVERFLOW && grid_rows > 1) {
					grid_transit_style(-1);
					goto restart;
				}
			} else {
			//This will all be deprecated (sometime)...  ;)
			
			//transition gui style: grid 1 -> flow 1 -> flow-z 1 -> coverflow modes -> grid 1 -> etc
			
			if (gui_style == GUI_STYLE_FLOW_Z && grid_rows == 1) {
				//flip to 3d coverflow
				gui_style = GUI_STYLE_COVERFLOW;
				CFG_cf_global.theme = coverflow3d;
				
				//Coverflow uses full covers ONLY
				// invalidate the cache and reload
				cache_release_all();
				cache_wait_idle();
				prev_cover_style = CFG.cover_style;
				if (CFG_cf_global.covers_3d) {
					if (CFG.cover_style != CFG_COVER_STYLE_FULL) {
						cfg_set_cover_style(CFG_COVER_STYLE_FULL);
						cfg_setup_cover_style();
					}
				} else {
					if (CFG.cover_style != CFG_COVER_STYLE_2D) {
						cfg_set_cover_style(CFG_COVER_STYLE_2D);
						cfg_setup_cover_style();
					}
				}
				Cache_Invalidate();
				Cache_Init();
				Coverflow_Grx_Init();
				gameSelected = Coverflow_initCoverObjects(gameCnt, gameSelected, false);
				//load the covers - alternate right and left
				cache_request_before_and_after(gameSelected, 5, 1);

			
			} else if (gui_style == GUI_STYLE_COVERFLOW) {
			
				//Coverflow_moveAllCoversToCenter();
				switch(CFG_cf_global.theme){
					case(coverflow3d):
						CFG_cf_global.theme = coverflow2d;
						break;
					case(coverflow2d):
						CFG_cf_global.theme = frontrow;
						break;
					case(frontrow):
						CFG_cf_global.theme = vertical;
						break;
					case(vertical):
						CFG_cf_global.theme = carousel;
						break;
					case(carousel):
						//flip to grid mode
						gui_style = GUI_STYLE_FLOW_Z;
						break;
					default:
						CFG_cf_global.theme = coverflow3d;
						break;
				}
				if (gui_style == GUI_STYLE_COVERFLOW) {
					//setup the new coverflow style
					gameSelected = Coverflow_initCoverObjects(gameCnt, game_select, true);
					if (gameSelected == -1) {
						//fatal error - couldn't allocate memory
						gui_style = GUI_STYLE_GRID;
						grid_transit_style(-1);
						goto restart;
					}
					Coverflow_init_transition(CF_TRANS_MOVE_TO_POSITION, 100, gameCnt, false);
				} else {
					//flip to grid mode
					//gui_style = GUI_STYLE_FLOW_Z;
					gui_style = GUI_STYLE_GRID;
					Gui_reset_previous_cover_style();
					Cache_Invalidate();
					Cache_Init();
					grid_init(gameSelected);
					//grid_transit_style(-1);
					goto restart;
				}
				
			} else {
				//grid mode
				//grid_change_rows(rows-1);
				grid_transit_style(-1);
				goto restart;
			}
			} // CFG.gui_lock
		}


		//----------------------
		//  (-) button
		//----------------------

		if (buttons & WPAD_BUTTON_MINUS) {
			//remove game
			if (game_select >= 0) {
				gameSelected = game_select;
				break;
			}
		}


		//----------------------
		//  (+) button
		//----------------------

		if (buttons & WPAD_BUTTON_PLUS) {
			//add game
			if (game_select >= 0) gameSelected = game_select;
			break;
		}


		//----------------------
		//  1 Button
		//----------------------

		if (buttons & WPAD_BUTTON_1) {
			if (game_select >= 0) gameSelected = game_select;
			break;
		}


		//----------------------
		//  2 button
		//----------------------

		if (buttons & WPAD_BUTTON_2) {
			extern void Switch_Favorites(bool enable);
			extern bool enable_favorite;
			
			cache_release_all();
			Switch_Favorites(!enable_favorite);
			ccache.num_game = gameCnt;

			if (gui_style == GUI_STYLE_COVERFLOW) {
				gameSelected = Coverflow_initCoverObjects(gameCnt, 0, true);
				//load the covers - alternate right and left
				cache_request_before_and_after(gameSelected, 5, 1);
			} else {
				//grid_init(page_gi);
				grid_init(0);
			}
		}


		//----------------------
		//  check for other wiimote events
		//----------------------

		if (gui_style == GUI_STYLE_FLOW
				|| gui_style == GUI_STYLE_FLOW_Z) {
			// scroll flow style
			grid_get_scroll(&ir);
		} else if (gui_style == GUI_STYLE_COVERFLOW) {
			if (!suppressWiiPointerChecking)
				game_select = Coverflow_process_wiimote_events(&ir, gameCnt);
			else
				suppressWiiPointerChecking = false;
		}
		
		//if we should load a game then do
		//that instead of drawing the next frame
		if (loadGame || suppressCoverDrawing) {
			suppressCoverDrawing = false;
			goto restart;
		}

		//_CPU_ISR_Disable(level);

		//draw the covers
		if (gui_style == GUI_STYLE_COVERFLOW) {
			//Wpad_getIR(WPAD_CHAN_0, &ir);
			game_select = Coverflow_drawCovers(&ir, CFG_cf_theme[CFG_cf_global.theme].number_of_side_covers, gameCnt, true);
		} else {
			//draw the background
			Gui_draw_background();
			
			grid_calc();
			game_select = grid_update_all(&ir);
			Gui_set_camera(&ir, 1);
			grid_draw(game_select);
			// title
			grid_print_title(game_select);
		}
		
		//wgui_test(&ir, buttons);
		
		//int ms = dbg_time2(NULL);
		//GRRLIB_Printf(20, 20, tx_font, 0xFF00FFFF, 1, "%3d ms: %d", fr_cnt, ms);
		//draw_Cache();
		//GRRLIB_DrawImg(0, 50, tx_font, 0, 1, 1, 0xFFFFFFFF); // entire font

		Gui_draw_pointer(&ir);
		//GRRLIB_Rectangle(fr_cnt % 640, 0, 5, 15, 0x000000FF, 1);

		//_CPU_ISR_Restore(level);
		//GRRLIB_Render();
		Gui_Render();

		fr_cnt++;
	}

	if (gui_style == GUI_STYLE_COVERFLOW) {
		// this is for when transitioning from coverflow to Console...
		if (!exiting) {
			if (prev_cover_style == CFG_COVER_STYLE_3D)
				Coverflow_init_transition(CF_TRANS_MOVE_TO_CONSOLE_3D, 100, gameCnt, false);
			else
				Coverflow_init_transition(CF_TRANS_MOVE_TO_CONSOLE, 100, gameCnt, false);
		}
		//reset to the previous style before leaving
		Gui_reset_previous_cover_style();
	} else {
		// this is for when transitioning from Grid to Console...
		//transition(-1, page_i*grid_covers);
		if (!exiting)
			grid_transition(-1, page_gi);
	}
	
	Repaint_ConBg(exiting);

	// restore original framebuffer
	_Video_SyncFB();
	_GRRLIB_SetFB(0);

	cache_release_all();

	return buttons;
}


void Gui_Close()
{
	if (CFG.gui == 0) return;
	if (!grr_init) return;
	
	Cache_Close();
	__console_flush(0);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	_GRRLIB_Exit(); 
	

	SAFE_FREE(tx_pointer.data);
	SAFE_FREE(tx_font.data);

	Coverflow_Close();

	grr_init = 0;
}


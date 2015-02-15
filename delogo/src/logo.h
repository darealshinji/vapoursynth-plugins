/*====================================================================
*	Logo Pattern			logo.h
* 
* [Stucture of Logo Data File]
* 
* 	"<logo file x.xx>"	// File header: Magic and Version (28 bytes)
* 	+----
* 	|	Number of logos contained in this file (4 bytes, BigEndian)
* 	+----
* 	|	LOGO_HEADER		// Data header
* 	+----
* 	|
* 	:	LOGO_PIXEL[h*w]	// Pixel data. Size can be computed by LOGO_HEADER->w,h
* 	:
* 	+----
* 	|	LOGO_HEADER
* 	+----
* 	|
* 	:	LOGO_PIXEL[h*w]
* 	:
* 
*===================================================================*/
#ifndef ___LOGO_H
#define ___LOGO_H

#include <stdint.h> // for uint32_t etc.

/* Logo Data Magic Text */
#define LOGO_FILE_HEADER_STR "<logo data file ver0.1>\0\0\0\0\0"
#define LOGO_FILE_HEADER_STR_SIZE  28

/*--------------------------------------------------------------------
*	LOGO_FILE_HEADER Struct
*		File header and version
*-------------------------------------------------------------------*/
typedef struct {
	char str[LOGO_FILE_HEADER_STR_SIZE];
	union{
		uint32_t      l;
		unsigned char c[4];
	} logonum;
} LOGO_FILE_HEADER;

#define SWAP_ENDIAN(x) (((x&0xff)<<24)|((x&0xff00)<<8)|((x&0xff0000)>>8)|((x&0xff000000)>>24))

/* Maximum depth (for solid pixel) */
#define LOGO_MAX_DP   1000

/* Maximum length of a logo name (including trailing \0) */
#define LOGO_MAX_NAME 32

/*--------------------------------------------------------------------
*	LOGO_HEADER Struct
*		Storing basic information of logo
*-------------------------------------------------------------------*/
typedef struct {
	char     name[LOGO_MAX_NAME]; 	/* Name                   */
	int16_t  x, y;      			/* Position               */
	int16_t  h, w;      			/* Logo width height      */
	int16_t  fi, fo;    			/* Default FadeIn/Out     */
	int16_t  st, ed;    			/* Default Start/End      */
} LOGO_HEADER;

/*--------------------------------------------------------------------
*	LOGO_PIXEL Struct
*		Storing color information of all pixels
*-------------------------------------------------------------------*/
typedef struct {
	int16_t dp_y;		/* Depth of Y         */
	int16_t y;		/* Y         0 ~ 4096 */
	int16_t dp_cb;	/* Depth of Cb        */
	int16_t cb;		/* Cb    -2048 ~ 2048 */
	int16_t dp_cr;	/* Depth of Cr        */
	int16_t cr;		/* Cr    -2048 ~ 2048 */
} LOGO_PIXEL;

/*--------------------------------------------------------------------
*	Size of Logo Data (without header)
*-------------------------------------------------------------------*/
#define LOGO_PIXELSIZE(ptr)  \
	(((LOGO_HEADER *)ptr)->h*((LOGO_HEADER *)ptr)->w*sizeof(LOGO_PIXEL))

/*--------------------------------------------------------------------
*	Size of Logo Data (including header)
*-------------------------------------------------------------------*/
#define LOGO_DATASIZE(ptr) (sizeof(LOGO_HEADER)+LOGO_PIXELSIZE(ptr))

#endif

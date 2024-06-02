
//? Marius Negrutiu (marius.negrutiu@protonmail.com) :: 2013/07/06

#include "main.h"

#define COBJMACROS
#include <olectl.h>
#include <ocidl.h>

int _fltused = 0;			/// For some reason AlphaBlend is using this. The module won't link without it.

#define ALIGN_LEFT			1
#define ALIGN_TOP			1
#define ALIGN_CENTER		2
#define ALIGN_RIGHT			3
#define ALIGN_BOTTOM		3


//++ ColorsAlmostIdentical
BOOLEAN ColorsAlmostIdentical( __in COLORREF cl1, __in COLORREF cl2 )
{
	#define ABS(x) ((x) >= 0 ? (x) : -(x))

	const int iTolerance = 5;
	return (
		ABS((int)GetRValue(cl1) - (int)GetRValue(cl2)) <= iTolerance &&
		ABS((int)GetGValue(cl1) - (int)GetGValue(cl2)) <= iTolerance &&
		ABS((int)GetBValue(cl1) - (int)GetBValue(cl2)) <= iTolerance
		);
}


//++ PickFillColor
COLORREF PickFillColor(
	__in HDC hDC,
	__in int x, __in int y,
	__in int w, __in int h
	)
{
	const int xCenter = x + w / 2;
	const int yCenter = y + h / 2;
	const int xRight = x + w - 1;
	const int yBottom = y + h - 1;

	POINT SamplePts[9] = {
		{ x, y },					/// left, top
		{ xCenter, y },				/// center, top
		{ xRight, y },				/// right, top
		{ xRight, yCenter },		/// right, center
		{ xRight, yBottom },		/// right, bottom
		{ xCenter, yBottom },		/// center, bottom
		{ x, yBottom },				/// left, bottom
		{ x, yCenter },				/// left, center
		{ xCenter, yCenter }		/// center, center
	};

	struct { COLORREF cl; int hits; } SampleColors[9];

	int i, j, iMaxHits;
	COLORREF cl, clMaxHits;

	// Pick some sample points from the image
	// We'll identify the color with most hits
	iMaxHits = 0;
	clMaxHits = 0;
	for ( i = 0; i < 9; i++ )
	{
		/// Clear this slot first
		SampleColors[i].cl = 0;
		SampleColors[i].hits = 0;

		cl = GetPixel( hDC, SamplePts[i].x, SamplePts[i].y );
		for ( j = 0; j < 9; j++ )
		{
			if ( SampleColors[j].hits == 0 ) {
				/// Uninitialized slot
				SampleColors[j].cl = cl;
				SampleColors[j].hits = 1;
				if ( iMaxHits < 1 ) {
					iMaxHits = 1;
					clMaxHits = cl;
				}
				break;

			} else {

				if ( ColorsAlmostIdentical( cl, SampleColors[j].cl )) {
					SampleColors[j].hits++;
					if ( iMaxHits < SampleColors[j].hits ) {
						if ( SampleColors[j].hits == 5 ) {
							return cl;	/// Enough hits to stop the search...
						}
						iMaxHits = SampleColors[j].hits;
						clMaxHits = cl;
					}
					break;
				}
			}
		}
	}

	return clMaxHits;
}


//++ PrepareBitmapForAlphaBlend
BOOL PrepareBitmapForAlphaBlend( __inout HBITMAP hBmp )
{
	BOOL bHasAlphaChannel = FALSE;
	if ( hBmp ) {

		BITMAP bm;
		if ( GetObject( hBmp, sizeof( bm ), &bm ) > 0 ) {

			// Make sure this is a 32bpp bitmap
			// Otherwise, there's no alpha channel anyway...
			if ( bm.bmBitsPixel == 32 ) {

				int x, y;
				LPRGBQUAD pBits;

				// Make a quick pass and check whether the alpha channel is used at all.
				pBits = (LPRGBQUAD)bm.bmBits;
				for ( y = 0; y < bm.bmHeight && !bHasAlphaChannel; y++ ) {
					for ( x = 0; x < bm.bmWidth; x++ ) {
						if ( pBits->rgbReserved != 0 ) {
							bHasAlphaChannel = TRUE;
							break;
						}
						pBits++;
					}
				}

				// Get direct access to bitmap's bits
				pBits = (LPRGBQUAD)bm.bmBits;
				if (bHasAlphaChannel) {
					// Pre-multiply the color channels the way AlphBlend is expecting
					for (y = 0; y < bm.bmHeight; y++) {
						for (x = 0; x < bm.bmWidth; x++) {
							pBits->rgbRed = (BYTE)(((LONG)pBits->rgbRed * pBits->rgbReserved) >> 8);
							pBits->rgbGreen = (BYTE)(((LONG)pBits->rgbGreen * pBits->rgbReserved) >> 8);
							pBits->rgbBlue = (BYTE)(((LONG)pBits->rgbBlue * pBits->rgbReserved) >> 8);
							pBits++;
						}
					}
				} else {
					// Simply set the alpha channel to 255 (full opacity)
					for (y = 0; y < bm.bmHeight; y++) {
						for (x = 0; x < bm.bmWidth; x++) {
							pBits->rgbReserved = 255;
							pBits++;
						}
					}
				}

			} else {
				//assert(!"not a 32bpp image sent to PrepareBitmapForAlphaBlend");
			}
		}
	}
	return bHasAlphaChannel;
}


//++ ResampleBitmap
DWORD ResampleBitmap(
	__in HBITMAP hBitmap,
	__in int iWidth,
	__in int iHeight,
	__in int iAlignHorz,
	__in int iAlignVert,
	__out HBITMAP *phBitmap
	)
{
	DWORD err = ERROR_SUCCESS;
	if ( hBitmap && phBitmap && (iWidth > 0) && (iHeight > 0)) {

		typedef BOOL( WINAPI *TfnAlphaBlend )(
			_In_ HDC hdcDest, _In_ int xoriginDest, _In_ int yoriginDest, _In_ int wDest, _In_ int hDest,
			_In_ HDC hdcSrc,  _In_ int xoriginSrc,  _In_ int yoriginSrc,  _In_ int wSrc,  _In_ int hSrc,
			_In_ BLENDFUNCTION ftn
			);

		int iSize;
		BITMAPINFO *pBmi;

		HMODULE hImg32;
		TfnAlphaBlend fnAlphaBlend = NULL;
		TCHAR szPath[MAX_PATH];

		// msimg32!AlphaBlend
		// NOTE: msimg32.dll is not available in NT4
		GetSystemDirectory( szPath, ARRAYSIZE( szPath ) );
		lstrcat( szPath, _T( "\\msimg32.dll" ) );
		hImg32 = LoadLibrary( szPath );
		if (hImg32)
			fnAlphaBlend = (TfnAlphaBlend)GetProcAddress( hImg32, "AlphaBlend" );

		// Create a device independent bitmap
		iSize = sizeof(BITMAPINFOHEADER) + 32 * iWidth * iHeight;
		pBmi = (BITMAPINFO*)GlobalAlloc( GPTR, iSize );
		if (pBmi) {

			pBmi->bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
			pBmi->bmiHeader.biWidth = iWidth;
			pBmi->bmiHeader.biHeight = iHeight;
			pBmi->bmiHeader.biPlanes = 1;
			pBmi->bmiHeader.biBitCount = 32;

			*phBitmap = CreateDIBSection( NULL, pBmi, DIB_RGB_COLORS, NULL, NULL, 0 );
			GlobalFree( pBmi );
			if ( *phBitmap ) {

				int x, y;
				int iSrcWidth, iSrcHeight;
				BITMAP bm;

				GetObject( hBitmap, sizeof(bm), &bm );
				iSrcWidth = bm.bmWidth;
				iSrcHeight = bm.bmHeight;

				/// Horizontal alignment
				switch ( iAlignHorz )
				{
				case ALIGN_LEFT:	x = 0;	break;
				case ALIGN_RIGHT:	x = iWidth - iSrcWidth;	break;
				default:			x = ( iWidth - iSrcWidth ) / 2;
				}

				/// Vertical alignment
				switch ( iAlignVert )
				{
				case ALIGN_TOP:		y = 0; break;
				case ALIGN_BOTTOM:	y = iHeight - iSrcHeight; break;
				default:			y = ( iHeight - iSrcHeight ) / 2;
				}

				// Draw
				if ( TRUE )
				{
					HDC hSrcDC = CreateCompatibleDC( NULL );
					HDC hDstDC = CreateCompatibleDC( NULL );
					HBITMAP hOldSrcBitmap = (HBITMAP)SelectObject( hSrcDC, hBitmap );
					HBITMAP hOldDstBitmap = (HBITMAP)SelectObject( hDstDC, *phBitmap );
					BOOL bHasAlphaChannel = FALSE;

					bHasAlphaChannel = PrepareBitmapForAlphaBlend( hBitmap );
					if (bHasAlphaChannel && fnAlphaBlend) {

						BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
						RECT rc = { 0, 0, iWidth, iHeight };

						// White-fill the destination, and paint the (transparent) bitmap over it
						FillRect( hDstDC, &rc, (HBRUSH)GetStockObject( WHITE_BRUSH ));
						fnAlphaBlend( hDstDC, x, y, iSrcWidth, iSrcHeight, hSrcDC, 0, 0, iSrcWidth, iSrcHeight, bf );

					} else {

						// Draw the (opaque) bitmap, then fill the surrounding areas
						BitBlt( hDstDC, x, y, iSrcWidth, iSrcHeight, hSrcDC, 0, 0, SRCCOPY );
						if ( iWidth > iSrcWidth || iHeight > iSrcHeight )
						{
							COLORREF cl = PickFillColor( hSrcDC, 0, 0, iSrcWidth, iSrcHeight );
							HBRUSH br = CreateSolidBrush( cl );
							RECT rc;

							if ( iWidth > iSrcWidth ) {
								SetRect( &rc, 0, 0, x, iHeight );			/// Left side of the image
								FillRect( hDstDC, &rc, br );
								SetRect( &rc, x + iSrcWidth, 0, iWidth, iHeight );	/// Right side of the image
								FillRect( hDstDC, &rc, br );
							}
							if ( iHeight > iSrcHeight ) {
								SetRect( &rc, x, 0, x + iSrcWidth, y );			/// Top side of the image
								FillRect( hDstDC, &rc, br );
								SetRect( &rc, x, y + iSrcHeight, x + iSrcWidth, iHeight );	/// Bottom side of the image
								FillRect( hDstDC, &rc, br );
							}
							DeleteObject( br );
						}
					}

					/// Border
					if ( TRUE ) {
						HBRUSH br = CreateSolidBrush( RGB( 211, 212, 214 ));
						RECT rc = { 0, 0, iWidth, iHeight };
						FrameRect( hDstDC, &rc, br );
						DeleteObject( br );
					}

					/// If the source image has no alpha channel -> force full opacity on destination image
					if (!bHasAlphaChannel)
						PrepareBitmapForAlphaBlend( *phBitmap );

					SelectObject( hSrcDC, hOldSrcBitmap );
					SelectObject( hDstDC, hOldDstBitmap );
					DeleteDC( hSrcDC );
					DeleteDC( hDstDC );
				}

			} else {
				err = GetLastError();		/// CreateDIBSection
			}
		} else {
			err = GetLastError();		/// GlobalAlloc
		}

		if (hImg32)
			FreeLibrary( hImg32 );

	} else {
		err = ERROR_INVALID_PARAMETER;
	}

	return err;
}


//++ LoadImageFileImpl
DWORD LoadImageFileImpl(
	__in LPCTSTR pszFilePath,
	__in int iWidth,
	__in int iHeight,
	__in int iAlignHorz,
	__in int iAlignVert,
	__out HBITMAP *phBitmap
	)
{
	DWORD err = ERROR_SUCCESS;
	if ( pszFilePath && *pszFilePath && (iWidth > 0) && (iHeight > 0) && phBitmap ) {

		// OLE initialize
		HRESULT hrOleInit = OleInitialize( NULL );

		// Load the image in memory
		IPicture *pIPicture = NULL;
#if defined _UNICODE
		LPCWSTR pszW = pszFilePath;
#else
		WCHAR pszW[512];
		pszW[0] = UNICODE_NULL;
		MultiByteToWideChar( CP_ACP, 0, pszFilePath, -1, pszW, ARRAYSIZE(pszW));
#endif
		err = OleLoadPicturePath((LPOLESTR)pszW, NULL, 0, 0, &IID_IPicture, (LPVOID)&pIPicture );
		if ( SUCCEEDED( err )) {

			// Make sure it's stored in memory as HBITMAP (as opposed to HICON, HMETAFILE, HENHMETAFILE, etc.)
			SHORT iPicType;
			err = IPicture_get_Type( pIPicture, &iPicType );
			if ( SUCCEEDED( err )) {

				if ( iPicType == PICTYPE_BITMAP ) {

					// Get the HBITMAP
					OLE_HANDLE hIPictureHandle;
					err = (DWORD)IPicture_get_Handle( pIPicture, &hIPictureHandle );
					if ( SUCCEEDED( err )) {

						// Resize
						err = ResampleBitmap( ULongToHandle( hIPictureHandle ), iWidth, iHeight, iAlignHorz, iAlignVert, phBitmap );
					}

				} else {
					err = ERROR_UNSUPPORTED_TYPE;
				}
			}

			IPicture_Release( pIPicture );
		}

		if ( SUCCEEDED( hrOleInit ))
			OleUninitialize();

	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ [exported] LoadImageFile
//  ----------------------------------------------------------------------
//+ Example:
//    NSutils::LoadImageFile "$PLUGINSDIR\Image.jpg" 640 480 center center
//    Pop $0
//    ${If} $0 <> 0
//      ;Success: $0 contains a valid HBITMAP
//      ; ...
//      ; Destroy the bitmap after no longer needed
//      System::Call 'gdi32::DeleteObject( i $0 )'
//    ${Else}
//      ;Error
//    ${EndIf}

void __declspec(dllexport) LoadImageFile(
	HWND parent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	//	Cache global structures
	EXDLL_INIT();
	EXDLL_VALIDATE();

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store an NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		// TODO:
		// If Width and Height are zero, load the image using its original dimensions
		// Add "stretch" to horizontal and vertical alignments
		// Support transparent images

		TCHAR szPath[MAX_PATH];
		int w, h;
		int iAlignH = ALIGN_CENTER, iAlignV = ALIGN_CENTER;

		///	Param1: Image file path or URL
		*szPath = _T('\0');
		if ( popstring( pszBuf ) == 0 ) {
			lstrcpyn( szPath, pszBuf, ARRAYSIZE( szPath ));
		}
		///	Param2: Image width
		w = popint();

		/// Param3: Image height
		h = popint();

		/// Param4: Horizontal alignment (can be "left", "right" or "center")
		if ( popstring( pszBuf ) == 0 ) {
			if ( lstrcmpi( pszBuf, _T("left")) == 0 ) {
				iAlignH = ALIGN_LEFT;
			} else if ( lstrcmpi( pszBuf, _T("right")) == 0 ) {
				iAlignH = ALIGN_RIGHT;
			} else {
				iAlignH = ALIGN_CENTER;
			}
		}

		/// Param5: Vertical alignment (can be "top", "bottom" or "center")
		if ( popstring( pszBuf ) == 0 ) {
			if ( lstrcmpi( pszBuf, _T("top")) == 0 ) {
				iAlignV = ALIGN_TOP;
			} else if ( lstrcmpi( pszBuf, _T("bottom")) == 0 ) {
				iAlignV = ALIGN_BOTTOM;
			} else {
				iAlignV = ALIGN_CENTER;
			}
		}

		// Validate input
		if ( *szPath && (w > 0) && (h > 0)) {

			HBITMAP hBmp = NULL;
			DWORD err = LoadImageFileImpl( szPath, w, h, iAlignH, iAlignV, &hBmp );
			if ( SUCCEEDED( err )) {

				// Return HBITMAP on the stack
				// It must be destroyed by the caller (gdi32!DeleteObject) when no longer needed
				pushintptr((INT_PTR)hBmp );

			} else {
				pushint( 0 );	/// Load error
			}

		} else {
			pushint( 0 );		/// Invalid parameter
		}

		/// Free memory
		GlobalFree( pszBuf );
	}
}

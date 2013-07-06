#include <windows.h>
#include "nsiswapi\pluginapi.h"

#define COBJMACROS
#include <IImgCtx.h>

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

		// Load the image in memory
		HRESULT hr;
		IImgCtx *pImage = NULL;

		hr = CoCreateInstance( &CLSID_IImgCtx, NULL, CLSCTX_ALL, &IID_IImgCtx, (void**)&pImage );
		if ( SUCCEEDED( hr )) {

			DWORD dwState;
			SIZE ImgSize = {0, 0};
#ifdef _UNICODE
			LPCWSTR pszPathW = pszFilePath;
#else
			WCHAR pszPathW[MAX_PATH];
			MultiByteToWideChar( CP_ACP, 0, pszFilePath, -1, pszPathW, ARRAYSIZE(pszPathW));
#endif
			hr = IImgCtx_Load( pImage, pszPathW, 0 );
			if ( SUCCEEDED( hr )) {

				// IImgCtx_Load is asynchronous. We must wait in loop until the operation gets finished
				while (
					SUCCEEDED( IImgCtx_GetStateInfo( pImage, &dwState, NULL, TRUE )) &&
					(dwState & (IMGLOAD_COMPLETE|IMGLOAD_ERROR)) == 0
					)
					Sleep(0);

				IImgCtx_GetStateInfo( pImage, &dwState, &ImgSize, TRUE );
				if ((( dwState & IMGLOAD_MASK ) == IMGLOAD_COMPLETE ) && ( ImgSize.cx != 0 ) && ( ImgSize.cy != 0 )) {

					HDC hDC;
					HBITMAP hBmpOriginal;

					// Create a new bitmap. Use specified dimensions
					// Copy the content of the temporary bitmap according to alignment flags
					hDC = CreateCompatibleDC( NULL );
					if ( hDC ) {

						// Create a device independent bitmap
						int iSize = sizeof(BITMAPINFOHEADER) + 32 * iWidth * iHeight;
						BITMAPINFO *pBmi = (BITMAPINFO*)GlobalAlloc( GPTR, iSize );
						if ( pBmi ) {

							pBmi->bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
							pBmi->bmiHeader.biWidth = iWidth;
							pBmi->bmiHeader.biHeight = iHeight;
							pBmi->bmiHeader.biPlanes = 1;
							pBmi->bmiHeader.biBitCount = 32;

							*phBitmap = CreateDIBSection( NULL, pBmi, DIB_RGB_COLORS, NULL, NULL, 0 );
							GlobalFree( pBmi );
							if ( *phBitmap ) {

								int x, y, w, h;

								w = min( iWidth, ImgSize.cx );
								h = min( iHeight, ImgSize.cy );

								/// Horizontal alignment
								switch ( iAlignHorz )
								{
								case ALIGN_LEFT:	x = 0;	break;
								case ALIGN_RIGHT:	x = iWidth - w;	break;
								default:			x = ( iWidth - ImgSize.cx ) / 2;
								}

								/// Vertical alignment
								switch ( iAlignVert )
								{
								case ALIGN_TOP:		y = 0; break;
								case ALIGN_BOTTOM:	y = iHeight - h; break;
								default:			y = ( iHeight - ImgSize.cy ) / 2;
								}

								// Select the bitmap into the DC
								hBmpOriginal = (HBITMAP)SelectObject( hDC, *phBitmap );

								// Draw
								if ( TRUE ) {
									RECT bounds = { x, y, x + ImgSize.cx, y + ImgSize.cy };
									hr = IImgCtx_Draw( pImage, hDC, &bounds );
								}

								// Fill empty areas
								if ( iWidth > ImgSize.cx || iHeight > ImgSize.cy )
								{
									int xx = max( 0, x );
									int yy = max( 0, y );
									COLORREF cl = PickFillColor( hDC, xx, yy, w, h );
									HBRUSH br = CreateSolidBrush( cl );
									RECT rc;

									if ( iWidth > ImgSize.cx ) {
										SetRect( &rc, 0, 0, x, iHeight );			/// Left side of the image
										FillRect( hDC, &rc, br );
										SetRect( &rc, x + w, 0, iWidth, iHeight );	/// Right side of the image
										FillRect( hDC, &rc, br );
									}
									if ( iHeight > ImgSize.cy ) {
										SetRect( &rc, xx, 0, xx + w, y );			/// Top side of the image
										FillRect( hDC, &rc, br );
										SetRect( &rc, xx, y + h, xx + w, iHeight );	/// Bottom side of the image
										FillRect( hDC, &rc, br );
									}
									DeleteObject( br );
								}

								// Unselect the bitmap from DC
								SelectObject( hDC, hBmpOriginal );

								// Remove the alpha channel
								if ( TRUE ) {
									BITMAP bm;
									if ( GetObject( *phBitmap, sizeof( bm ), &bm ) > 0 ) {
										LPRGBQUAD pBits = (LPRGBQUAD)bm.bmBits;
										for ( y = 0; y < bm.bmHeight; y++ ) {
											for ( x = 0; x < bm.bmWidth; x++ ) {
												pBits->rgbReserved = 0;		/// No alpha channel
												pBits++;
											}
										}
									}
								}

							} else {
								err = GetLastError();		/// CreateDIBSection
							}
						} else {
							err = GetLastError();		/// GlobalAlloc
						}

						DeleteDC( hDC );

					} else {
						err = ERROR_OUTOFMEMORY;	/// CreateCompatibleDC
					}

				} else {
					err = ERROR_BAD_FILE_TYPE;
				}
			} else {
				err = hr;
			}

			IImgCtx_Disconnect( pImage );
			IImgCtx_Release( pImage );

		} else {
			err = hr;
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}

//
//  [exported] LoadImageFile
//  ----------------------------------------------------------------------
//  Example:
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
//
void __declspec(dllexport) LoadImageFile(
	HWND hWndParent,
	int string_size,
	TCHAR *variables,
	stack_t **stacktop,
	extra_parameters *extra
	)
{
	LPTSTR pszBuf = NULL;

	//	Cache global structures
	EXDLL_INIT();

	//	Check NSIS API compatibility
	if ( !IsCompatibleApiVersion()) {
		/// TODO: display an error message?
		return;
	}

	//	Retrieve NSIS parameters
	/// Allocate memory large enough to store an NSIS string
	pszBuf = (TCHAR*)GlobalAlloc( GPTR, sizeof(TCHAR) * string_size );
	if ( pszBuf ) {

		// TODO:
		// If Width and Height are zero, load the image using its original dimensions
		// Add "stretch" to horizontal and vertical alignments

		TCHAR szPath[MAX_PATH];
		int w, h;
		BOOLEAN bSuccess = FALSE;
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
				pushint((int)hBmp );

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

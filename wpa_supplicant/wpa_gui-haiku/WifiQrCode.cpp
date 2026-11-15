/*
 * WPA Supplicant - Wi-Fi sharing QR code decoding
 * Copyright (c) 2026, Fabio Tomat <f.t.public@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */


#include "WifiQrCode.h"

#include <ctype.h>
#include <string.h>

#include <Bitmap.h>
#include <GraphicsDefs.h>
#include <NetworkDevice.h>

#include "quirc.h"


static void
UnescapeWifiField(BString& field)
{
	BString result;
	int32 length = field.Length();

	for (int32 i = 0; i < length; i++) {
		char c = field[i];
		if (c == '\\' && i + 1 < length)
			c = field[++i];
		result.Append(c, 1);
	}

	field = result;
}


bool
ParseWifiQrText(const BString& text, BString& ssid, uint32& authMode,
	BString& password, bool& hidden)
{
	if (!text.StartsWith("WIFI:"))
		return false;

	ssid = "";
	password = "";
	authMode = B_NETWORK_AUTHENTICATION_NONE;
	hidden = false;

	const char* p = text.String() + strlen("WIFI:");
	bool haveSsid = false;

	while (*p != '\0') {
		// Keys are plain letters ("S", "T", "P", "H", but also multi-letter
		// ones like "PH2", "R" or "K" from the WPA3/ZXing extensions), so
		// the colon must come after a run of letters only.
		const char* colon = p;
		while (isalpha((unsigned char)*colon))
			colon++;
		if (colon == p || *colon != ':')
			break;

		BString key(p, colon - p);

		const char* valueStart = colon + 1;
		const char* valueEnd = NULL;
		for (const char* s = valueStart; *s != '\0'; s++) {
			if (*s == '\\' && *(s + 1) != '\0') {
				s++;
				continue;
			}
			if (*s == ';') {
				valueEnd = s;
				break;
			}
		}
		if (valueEnd == NULL)
			valueEnd = valueStart + strlen(valueStart);

		BString value(valueStart, valueEnd - valueStart);
		UnescapeWifiField(value);

		if (key == "S") {
			ssid = value;
			haveSsid = true;
		} else if (key == "T") {
			// "nopass" (or an empty value) means an open network; "WEP" is
			// WEP; everything else ("WPA", "WPA2", "WPA3", "SAE", ...) is a
			// WPA family network, which the dialog shows as WPA/WPA2.
			if (value.IsEmpty() || value.ICompare("nopass") == 0)
				authMode = B_NETWORK_AUTHENTICATION_NONE;
			else if (value.ICompare("WEP") == 0)
				authMode = B_NETWORK_AUTHENTICATION_WEP;
			else
				authMode = B_NETWORK_AUTHENTICATION_WPA2;
		} else if (key == "P") {
			password = value;
		} else if (key == "H") {
			hidden = value.ICompare("true") == 0;
		}
		// Other keys (PH2, E, A, I, R, K, ...) are not used here.

		p = (*valueEnd == ';') ? valueEnd + 1 : valueEnd;
	}

	return haveSsid;
}


// Approximate luma from Haiku's little-endian B_RGB32/B_RGB24 byte order
// (blue, green, red[, alpha]).
static inline uint8
LumaFromBgr(uint8 b, uint8 g, uint8 r)
{
	return (uint8)((r * 19 + g * 38 + b * 7) >> 6);
}


bool
ScanBitmapForWifiQr(BBitmap* bitmap, BString& ssid, uint32& authMode,
	BString& password, bool& hidden)
{
	if (bitmap == NULL || !bitmap->IsValid())
		return false;

	int32 width = bitmap->Bounds().IntegerWidth() + 1;
	int32 height = bitmap->Bounds().IntegerHeight() + 1;
	if (width <= 0 || height <= 0)
		return false;

	struct quirc* decoder = quirc_new();
	if (decoder == NULL)
		return false;

	if (quirc_resize(decoder, width, height) < 0) {
		quirc_destroy(decoder);
		return false;
	}

	int bufferWidth = 0, bufferHeight = 0;
	uint8_t* qrBuffer = quirc_begin(decoder, &bufferWidth, &bufferHeight);

	const uint8* bits = (const uint8*)bitmap->Bits();
	int32 bytesPerRow = bitmap->BytesPerRow();
	color_space format = bitmap->ColorSpace();
	bool supported = true;

	for (int32 y = 0; y < height && supported; y++) {
		const uint8* row = bits + y * bytesPerRow;
		uint8_t* outRow = qrBuffer + y * bufferWidth;

		for (int32 x = 0; x < width; x++) {
			switch (format) {
				case B_RGB32:
				case B_RGBA32:
				{
					const uint8* pixel = row + x * 4;
					outRow[x] = LumaFromBgr(pixel[0], pixel[1], pixel[2]);
					break;
				}

				case B_RGB24:
				{
					const uint8* pixel = row + x * 3;
					outRow[x] = LumaFromBgr(pixel[0], pixel[1], pixel[2]);
					break;
				}

				case B_GRAY8:
					outRow[x] = row[x];
					break;

				default:
					supported = false;
					break;
			}

			if (!supported)
				break;
		}
	}

	if (!supported) {
		quirc_destroy(decoder);
		return false;
	}

	quirc_end(decoder);

	bool found = false;
	int count = quirc_count(decoder);
	for (int i = 0; i < count && !found; i++) {
		struct quirc_code code;
		quirc_extract(decoder, i, &code);

		struct quirc_data data;
		if (quirc_decode(&code, &data) == QUIRC_SUCCESS) {
			BString text((const char*)data.payload, data.payload_len);
			if (ParseWifiQrText(text, ssid, authMode, password, hidden))
				found = true;
		}
	}

	quirc_destroy(decoder);
	return found;
}

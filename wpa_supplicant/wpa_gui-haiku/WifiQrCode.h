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
#ifndef WIFI_QR_CODE_H
#define WIFI_QR_CODE_H


#include <String.h>
#include <SupportDefs.h>

class BBitmap;


// Parses the standard Wi-Fi sharing QR payload used by Android/iOS:
// "WIFI:S:<ssid>;T:<WPA|WEP|nopass>;P:<password>;H:<true|false>;;"
// authMode is one of the B_NETWORK_AUTHENTICATION_* constants.
bool ParseWifiQrText(const BString& text, BString& ssid, uint32& authMode,
	BString& password, bool& hidden);

// Decodes any QR code found in the bitmap (regardless of surrounding image
// content) and, if it is a Wi-Fi sharing code, fills in the fields. Returns
// false if no Wi-Fi QR code could be found/decoded.
bool ScanBitmapForWifiQr(BBitmap* bitmap, BString& ssid, uint32& authMode,
	BString& password, bool& hidden);


#endif	// WIFI_QR_CODE_H

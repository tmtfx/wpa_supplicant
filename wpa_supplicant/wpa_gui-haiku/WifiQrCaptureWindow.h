/*
 * WPA Supplicant - Wi-Fi QR code capture window
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
#ifndef WIFI_QR_CAPTURE_WINDOW_H
#define WIFI_QR_CAPTURE_WINDOW_H


#include <MediaDefs.h>
#include <MediaNode.h>
#include <Messenger.h>
#include <String.h>
#include <Window.h>

class BMediaRoster;
class BMessage;
class BStringView;
class BView;
class QrVideoConsumer;


// A small window that previews a video capture device and reports the first
// decoded Wi-Fi sharing QR code (or the fact that none was found, if the
// user cancels) to `target` as a message of type `resultWhat` (see
// QrVideoConsumer), then closes itself.
class WifiQrCaptureWindow : public BWindow {
public:
								WifiQrCaptureWindow(BMessenger target,
									uint32 resultWhat);
	virtual						~WifiQrCaptureWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			status_t			_SetUpNodes();
			void				_TearDownNodes();

			BMessenger			fTarget;
			uint32				fResultWhat;

			BView*				fPreviewView;
			BStringView*		fStatusView;

			BMediaRoster*		fMediaRoster;
			media_node			fTimeSourceNode;
			media_node			fProducerNode;
			media_output		fProducerOut;
			media_input			fConsumerIn;
			QrVideoConsumer*	fConsumer;
			bool				fNodesConnected;

			bool				fFound;
			BString				fFoundSsid;
			uint32				fFoundAuthMode;
			BString				fFoundPassword;
			bool				fFoundHidden;
};


#endif	// WIFI_QR_CAPTURE_WINDOW_H

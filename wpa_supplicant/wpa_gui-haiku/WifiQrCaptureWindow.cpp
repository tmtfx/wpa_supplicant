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


#include "WifiQrCaptureWindow.h"

#include <string.h>

#include <scheduler.h>

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <MediaRoster.h>
#include <Message.h>
#include <String.h>
#include <StringView.h>
#include <TimeSource.h>
#include <View.h>

#include "QrVideoConsumer.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "wpa_supplicant"


static const int32 kPreviewWidth = 320;
static const int32 kPreviewHeight = 240;


WifiQrCaptureWindow::WifiQrCaptureWindow(BMessenger target, uint32 resultWhat)
	:
	BWindow(BRect(0, 0, kPreviewWidth, kPreviewHeight),
		B_TRANSLATE("Scan Wi-Fi QR code"), B_TITLED_WINDOW_LOOK,
		B_FLOATING_ALL_WINDOW_FEEL,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target),
	fResultWhat(resultWhat),
	fMediaRoster(NULL),
	fTimeSourceNode(media_node::null),
	fProducerNode(media_node::null),
	fConsumer(NULL),
	fNodesConnected(false),
	fFound(false),
	fFoundAuthMode(0),
	fFoundHidden(false)
{
	fPreviewView = new BView("preview", B_WILL_DRAW);
	fPreviewView->SetExplicitMinSize(BSize(kPreviewWidth, kPreviewHeight));
	fPreviewView->SetExplicitMaxSize(BSize(kPreviewWidth, kPreviewHeight));
	fPreviewView->SetViewColor(0, 0, 0);

	fStatusView = new BStringView("status",
		B_TRANSLATE("Point the camera at a Wi-Fi sharing QR code" B_UTF8_ELLIPSIS));

	BButton* cancelButton = new BButton(B_TRANSLATE("Cancel"),
		new BMessage(B_QUIT_REQUESTED));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(fPreviewView)
		.Add(fStatusView)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancelButton)
		.End();

	CenterOnScreen();

	status_t status = _SetUpNodes();
	if (status != B_OK) {
		BString text;
		text.SetToFormat(B_TRANSLATE("No video capture device could be "
			"used: %s"), strerror(status));
		fStatusView->SetText(text.String());
	}
}


WifiQrCaptureWindow::~WifiQrCaptureWindow()
{
}


void
WifiQrCaptureWindow::MessageReceived(BMessage* message)
{
	if (message->what == fResultWhat) {
		// Forwarded by QrVideoConsumer from the media node thread: a QR
		// code was decoded. Remember it and quit; QuitRequested() reports
		// it back to fTarget.
		fFound = true;
		message->FindString("ssid", &fFoundSsid);
		message->FindUInt32("authMode", &fFoundAuthMode);
		message->FindString("password", &fFoundPassword);
		message->FindBool("hidden", &fFoundHidden);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}

	BWindow::MessageReceived(message);
}


bool
WifiQrCaptureWindow::QuitRequested()
{
	_TearDownNodes();

	BMessage result(fResultWhat);
	result.AddBool("found", fFound);
	if (fFound) {
		result.AddString("ssid", fFoundSsid);
		result.AddUInt32("authMode", fFoundAuthMode);
		result.AddString("password", fFoundPassword);
		result.AddBool("hidden", fFoundHidden);
	}
	fTarget.SendMessage(&result);

	return true;
}


status_t
WifiQrCaptureWindow::_SetUpNodes()
{
	status_t status;
	fMediaRoster = BMediaRoster::Roster(&status);
	if (status != B_OK)
		return status;

	status = fMediaRoster->GetTimeSource(&fTimeSourceNode);
	if (status != B_OK)
		return status;

	status = fMediaRoster->GetVideoInput(&fProducerNode);
	if (status != B_OK)
		return status;

	fConsumer = new QrVideoConsumer("QR Scanner", fPreviewView, BMessenger(this),
		fResultWhat);
	status = fMediaRoster->RegisterNode(fConsumer);
	if (status != B_OK)
		return status;

	int32 count = 0;
	status = fMediaRoster->GetFreeOutputsFor(fProducerNode, &fProducerOut, 1,
		&count, B_MEDIA_RAW_VIDEO);
	if (status != B_OK || count < 1)
		return B_RESOURCE_UNAVAILABLE;

	count = 0;
	status = fMediaRoster->GetFreeInputsFor(fConsumer->Node(), &fConsumerIn, 1,
		&count, B_MEDIA_RAW_VIDEO);
	if (status != B_OK || count < 1)
		return B_RESOURCE_UNAVAILABLE;

	media_format format;
	format.type = B_MEDIA_RAW_VIDEO;
	media_raw_video_format videoFormat = {0, 1, 0, 239, B_VIDEO_TOP_LEFT_RIGHT,
		1, 1, {B_RGB32, kPreviewWidth, kPreviewHeight, kPreviewWidth * 4, 0, 0}};
	format.u.raw_video = videoFormat;

	status = fMediaRoster->Connect(fProducerOut.source, fConsumerIn.destination,
		&format, &fProducerOut, &fConsumerIn);
	if (status != B_OK)
		return status;
	fNodesConnected = true;

	status = fMediaRoster->SetTimeSourceFor(fProducerNode.node,
		fTimeSourceNode.node);
	if (status != B_OK)
		return status;
	status = fMediaRoster->SetTimeSourceFor(fConsumer->ID(),
		fTimeSourceNode.node);
	if (status != B_OK)
		return status;

	bigtime_t latency = 0;
	fMediaRoster->GetLatencyFor(fProducerNode, &latency);
	fMediaRoster->SetProducerRunModeDelay(fProducerNode, latency);

	bigtime_t initLatency = 0;
	status = fMediaRoster->GetInitialLatencyFor(fProducerNode, &initLatency);
	if (status != B_OK)
		return status;
	initLatency += estimate_max_scheduling_latency();

	BTimeSource* timeSource = fMediaRoster->MakeTimeSourceFor(fProducerNode);
	bool running = timeSource->IsRunning();

	bigtime_t real = BTimeSource::RealTime();
	if (!running) {
		status = fMediaRoster->StartTimeSource(fTimeSourceNode, real);
		if (status == B_OK)
			status = fMediaRoster->SeekTimeSource(fTimeSourceNode, 0, real);
		if (status != B_OK) {
			timeSource->Release();
			return status;
		}
	}

	bigtime_t perf = timeSource->PerformanceTimeFor(real + latency
		+ initLatency);
	timeSource->Release();

	status = fMediaRoster->StartNode(fProducerNode, perf);
	if (status != B_OK)
		return status;
	status = fMediaRoster->StartNode(fConsumer->Node(), perf);
	if (status != B_OK)
		return status;

	return B_OK;
}


void
WifiQrCaptureWindow::_TearDownNodes()
{
	if (fMediaRoster == NULL)
		return;

	if (fConsumer != NULL) {
		fMediaRoster->StopNode(fConsumer->Node(), 0, true);

		if (fNodesConnected) {
			fMediaRoster->Disconnect(fProducerOut.node.node, fProducerOut.source,
				fConsumerIn.node.node, fConsumerIn.destination);
		}

		if (fProducerNode != media_node::null) {
			fMediaRoster->ReleaseNode(fProducerNode);
			fProducerNode = media_node::null;
		}

		fMediaRoster->ReleaseNode(fConsumer->Node());
		fConsumer = NULL;
	}
}

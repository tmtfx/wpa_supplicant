/*
 * WPA Supplicant - Wireless Config Dialog - QR video consumer
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
 *
 * Trimmed-down BBufferConsumer for scanning Wi-Fi sharing QR codes from a
 * video capture device, adapted from Haiku's CodyCam sample
 * (src/apps/codycam/VideoConsumer.h/.cpp, MIT licensed): no FTP/translator/
 * save logic, just frame delivery plus a QR decode attempt on every frame.
 */


#include "QrVideoConsumer.h"

#include <string.h>

#include <Buffer.h>
#include <BufferGroup.h>
#include <Message.h>
#include <TimeSource.h>
#include <View.h>
#include <Window.h>

#include "WifiQrCode.h"


static const media_raw_video_format kVideoFormat = {29.97, 1, 0, 239,
	B_VIDEO_TOP_LEFT_RIGHT, 1, 1, {B_RGB32, 320, 240, 320 * 4, 0, 0}};


QrVideoConsumer::QrVideoConsumer(const char* name, BView* view,
	BMessenger target, uint32 resultWhat)
	:
	BMediaNode(name),
	BMediaEventLooper(),
	BBufferConsumer(B_MEDIA_RAW_VIDEO),
	fView(view),
	fTarget(target),
	fResultWhat(resultWhat),
	fFoundCode(false),
	fConnectionActive(false),
	fMyLatency(20000),
	fBuffers(NULL)
{
	AddNodeKind(B_PHYSICAL_OUTPUT);
	SetEventLatency(0);

	for (uint32 i = 0; i < 3; i++) {
		fBitmap[i] = NULL;
		fBufferMap[i] = NULL;
	}

	SetPriority(B_DISPLAY_PRIORITY);
}


QrVideoConsumer::~QrVideoConsumer()
{
	Quit();
	_DeleteBuffers();
}


BMediaAddOn*
QrVideoConsumer::AddOn(int32* cookie) const
{
	*cookie = 0;
	return NULL;
}


void
QrVideoConsumer::Start(bigtime_t performanceTime)
{
	BMediaEventLooper::Start(performanceTime);
}


void
QrVideoConsumer::Stop(bigtime_t performanceTime, bool immediate)
{
	BMediaEventLooper::Stop(performanceTime, immediate);
}


void
QrVideoConsumer::Seek(bigtime_t mediaTime, bigtime_t performanceTime)
{
	BMediaEventLooper::Seek(mediaTime, performanceTime);
}


void
QrVideoConsumer::TimeWarp(bigtime_t atRealTime, bigtime_t toPerformanceTime)
{
	BMediaEventLooper::TimeWarp(atRealTime, toPerformanceTime);
}


status_t
QrVideoConsumer::DeleteHook(BMediaNode* node)
{
	return BMediaEventLooper::DeleteHook(node);
}


void
QrVideoConsumer::NodeRegistered()
{
	fIn.destination.port = ControlPort();
	fIn.destination.id = 0;
	fIn.source = media_source::null;
	fIn.format.type = B_MEDIA_RAW_VIDEO;
	fIn.format.u.raw_video = kVideoFormat;

	Run();
}


status_t
QrVideoConsumer::RequestCompleted(const media_request_info& info)
{
	return B_OK;
}


status_t
QrVideoConsumer::HandleMessage(int32 message, const void* data, size_t size)
{
	return B_OK;
}


void
QrVideoConsumer::BufferReceived(BBuffer* buffer)
{
	if (RunState() == B_STOPPED) {
		buffer->Recycle();
		return;
	}

	media_timed_event event(buffer->Header()->start_time,
		BTimedEventQueue::B_HANDLE_BUFFER, buffer,
		BTimedEventQueue::B_RECYCLE_BUFFER);
	EventQueue()->AddEvent(event);
}


void
QrVideoConsumer::ProducerDataStatus(const media_destination& forWhom,
	int32 status, bigtime_t atMediaTime)
{
}


status_t
QrVideoConsumer::_CreateBuffers(const media_format& withFormat)
{
	_DeleteBuffers();

	uint32 width = withFormat.u.raw_video.display.line_width;
	uint32 height = withFormat.u.raw_video.display.line_count;
	color_space format = withFormat.u.raw_video.display.format;

	fBuffers = new BBufferGroup();
	status_t status = fBuffers->InitCheck();
	if (status != B_OK)
		return status;

	for (uint32 i = 0; i < 3; i++) {
		fBitmap[i] = new BBitmap(BRect(0, 0, width - 1, height - 1), format,
			false, true);
		if (!fBitmap[i]->IsValid())
			return B_ERROR;

		buffer_clone_info info;
		info.area = area_for(fBitmap[i]->Bits());
		if (info.area == B_ERROR)
			return B_ERROR;
		info.offset = 0;
		info.size = (size_t)fBitmap[i]->BitsLength();
		info.flags = i;
		info.buffer = 0;

		status = fBuffers->AddBuffer(info);
		if (status != B_OK)
			return status;
	}

	BBuffer* buffers[3] = {NULL, NULL, NULL};
	status = fBuffers->GetBufferList(3, buffers);
	if (status != B_OK)
		return status;

	for (uint32 i = 0; i < 3; i++) {
		if (buffers[i] == NULL)
			return B_ERROR;
		fBufferMap[i] = buffers[i];
	}

	return B_OK;
}


void
QrVideoConsumer::_DeleteBuffers()
{
	if (fBuffers != NULL) {
		delete fBuffers;
		fBuffers = NULL;

		for (uint32 i = 0; i < 3; i++) {
			delete fBitmap[i];
			fBitmap[i] = NULL;
		}
	}
}


status_t
QrVideoConsumer::Connected(const media_source& producer,
	const media_destination& where, const media_format& withFormat,
	media_input* outInput)
{
	fIn.source = producer;
	fIn.format = withFormat;
	fIn.node = Node();
	strcpy(fIn.name, "QR Video Consumer");
	*outInput = fIn;

	if (_CreateBuffers(withFormat) != B_OK)
		return B_ERROR;

	uint32 userData = 0;
	int32 changeTag = 1;
	BBufferConsumer::SetOutputBuffersFor(producer, fDestination, fBuffers,
		(void*)&userData, &changeTag, true);

	fConnectionActive = true;
	return B_OK;
}


void
QrVideoConsumer::Disconnected(const media_source& producer,
	const media_destination& where)
{
	if (where == fIn.destination && producer == fIn.source) {
		fIn.source = media_source::null;
		fConnectionActive = false;
	}
}


status_t
QrVideoConsumer::AcceptFormat(const media_destination& dest,
	media_format* format)
{
	if (dest != fIn.destination)
		return B_MEDIA_BAD_DESTINATION;

	if (format->type == B_MEDIA_NO_TYPE)
		format->type = B_MEDIA_RAW_VIDEO;

	if (format->type != B_MEDIA_RAW_VIDEO)
		return B_MEDIA_BAD_FORMAT;

	if (format->u.raw_video.display.format != B_RGB32
		&& format->u.raw_video.display.format != B_GRAY8
		&& format->u.raw_video.display.format
			!= media_raw_video_format::wildcard.display.format) {
		return B_MEDIA_BAD_FORMAT;
	}

	if (format->u.raw_video.display.format
		== media_raw_video_format::wildcard.display.format) {
		format->u.raw_video.display.format = B_RGB32;
	}

	return B_OK;
}


status_t
QrVideoConsumer::GetNextInput(int32* cookie, media_input* outInput)
{
	if (*cookie < 1) {
		fIn.node = Node();
		fIn.destination.id = *cookie;
		strcpy(fIn.name, "QR Video Consumer");
		*outInput = fIn;
		(*cookie)++;
		return B_OK;
	}

	return B_MEDIA_BAD_DESTINATION;
}


void
QrVideoConsumer::DisposeInputCookie(int32 cookie)
{
}


status_t
QrVideoConsumer::GetLatencyFor(const media_destination& forWhom,
	bigtime_t* outLatency, media_node_id* outTimeSource)
{
	if (forWhom != fIn.destination)
		return B_MEDIA_BAD_DESTINATION;

	*outLatency = fMyLatency;
	*outTimeSource = TimeSource()->ID();
	return B_OK;
}


status_t
QrVideoConsumer::FormatChanged(const media_source& producer,
	const media_destination& consumer, int32 fromChangeCount,
	const media_format& format)
{
	if (consumer != fIn.destination)
		return B_MEDIA_BAD_DESTINATION;
	if (producer != fIn.source)
		return B_MEDIA_BAD_SOURCE;

	fIn.format = format;
	return _CreateBuffers(format);
}


void
QrVideoConsumer::HandleEvent(const media_timed_event* event,
	bigtime_t lateness, bool realTimeEvent)
{
	switch (event->type) {
		case BTimedEventQueue::B_STOP:
			EventQueue()->FlushEvents(event->event_time,
				BTimedEventQueue::B_ALWAYS, true,
				BTimedEventQueue::B_HANDLE_BUFFER);
			break;

		case BTimedEventQueue::B_HANDLE_BUFFER:
		{
			BBuffer* buffer = (BBuffer*)event->pointer;
			if (RunState() != B_STARTED || !fConnectionActive) {
				buffer->Recycle();
				break;
			}

			uint32 index = 0;
			bool ourBuffer = true;
			while (index < 3) {
				if (buffer == fBufferMap[index])
					break;
				index++;
			}
			if (index == 3) {
				ourBuffer = false;
				index = 0;
			}

			if (!ourBuffer) {
				memcpy(fBitmap[index]->Bits(), buffer->Data(),
					fBitmap[index]->BitsLength());
			}

			if (fView != NULL && fView->Window() != NULL
				&& fView->Window()->Lock()) {
				fView->DrawBitmap(fBitmap[index], fView->Bounds());
				fView->Window()->Unlock();
			}

			if (!fFoundCode) {
				BString ssid, password;
				uint32 authMode;
				bool hidden;
				if (ScanBitmapForWifiQr(fBitmap[index], ssid, authMode,
						password, hidden)) {
					fFoundCode = true;

					BMessage result(fResultWhat);
					result.AddBool("found", true);
					result.AddString("ssid", ssid);
					result.AddUInt32("authMode", authMode);
					result.AddString("password", password);
					result.AddBool("hidden", hidden);
					fTarget.SendMessage(&result);
				}
			}

			buffer->Recycle();
			break;
		}

		default:
			break;
	}
}

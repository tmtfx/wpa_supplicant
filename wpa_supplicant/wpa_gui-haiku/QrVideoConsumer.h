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
 * (src/apps/codycam/VideoConsumer.h/.cpp, MIT licensed).
 */
#ifndef QR_VIDEO_CONSUMER_H
#define QR_VIDEO_CONSUMER_H


#include <Bitmap.h>
#include <BufferConsumer.h>
#include <Messenger.h>
#include <MediaEventLooper.h>
#include <MediaNode.h>

class BView;


class QrVideoConsumer : public BMediaEventLooper, public BBufferConsumer {
public:
								QrVideoConsumer(const char* name, BView* view,
									BMessenger target, uint32 resultWhat);
	virtual						~QrVideoConsumer();

/*	BMediaNode */
public:
	virtual	BMediaAddOn*		AddOn(int32* cookie) const;

protected:
	virtual void				Start(bigtime_t performanceTime);
	virtual void				Stop(bigtime_t performanceTime,
									bool immediate);
	virtual void				Seek(bigtime_t mediaTime,
									bigtime_t performanceTime);
	virtual void				TimeWarp(bigtime_t atRealTime,
									bigtime_t toPerformanceTime);

	virtual void				NodeRegistered();
	virtual	status_t			RequestCompleted(
									const media_request_info& info);

	virtual	status_t			HandleMessage(int32 message, const void* data,
									size_t size);

	virtual status_t			DeleteHook(BMediaNode* node);

/*  BMediaEventLooper */
protected:
	virtual void				HandleEvent(const media_timed_event* event,
									bigtime_t lateness, bool realTimeEvent);

/*	BBufferConsumer */
public:
	virtual	status_t			AcceptFormat(const media_destination& dest,
									media_format* format);

	virtual	status_t			GetNextInput(int32* cookie,
									media_input* outInput);

	virtual	void				DisposeInputCookie(int32 cookie);

protected:
	virtual	void				BufferReceived(BBuffer* buffer);

private:
	virtual	void				ProducerDataStatus(
									const media_destination& forWhom,
									int32 status, bigtime_t atMediaTime);

	virtual	status_t			GetLatencyFor(const media_destination& forWhom,
									bigtime_t* outLatency,
									media_node_id* outId);

	virtual	status_t			Connected(const media_source& producer,
									const media_destination& where,
									const media_format& withFormat,
									media_input* outInput);

	virtual	void				Disconnected(const media_source& producer,
									const media_destination& where);

	virtual	status_t			FormatChanged(const media_source& producer,
									const media_destination& consumer,
									int32 fromChangeCount,
									const media_format& format);

			status_t			_CreateBuffers(const media_format& withFormat);
			void				_DeleteBuffers();

private:
			BView*				fView;
			BMessenger			fTarget;
			uint32				fResultWhat;
			bool				fFoundCode;

			bool				fConnectionActive;
			media_input			fIn;
			media_destination	fDestination;
			bigtime_t			fMyLatency;

			BBitmap*			fBitmap[3];
			BBufferGroup*		fBuffers;
			BBuffer*			fBufferMap[3];
};


#endif	// QR_VIDEO_CONSUMER_H

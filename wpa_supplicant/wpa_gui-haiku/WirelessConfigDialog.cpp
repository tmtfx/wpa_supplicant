/*
 * WPA Supplicant - Wireless Config Dialog
 * Copyright (c) 2011, Michael Lotz <mmlr@mlotz.ch>
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

#include <Alert.h>
#include <Bitmap.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <FilePanel.h>
#include <GridLayout.h>
#include <GridView.h>
#include <LayoutBuilder.h>
#include <GroupView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <NetworkDevice.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslationUtils.h>
#include <Window.h>
#include <View.h>

#include <new>

#include "WifiQrCaptureWindow.h"
#include "WifiQrCode.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "wpa_supplicant"


static const uint32 kMessageCancel = 'btcl';
static const uint32 kMessageOk = 'btok';
static const uint32 kMessageShowPass = 'btsh';
static const uint32 kMessageScanWebcam = 'btwc';
static const uint32 kMessageScanImage = 'btig';
static const uint32 kMessageQrFound = 'btqr';


class WirelessConfigView : public BView {
public:
	WirelessConfigView()
		:
		BView("WirelessConfigView", B_WILL_DRAW),
		fPassword(NULL),
		fFilePanel(NULL),
		fCaptureWindow(NULL)
	{
		fErrorLabel = new BStringView("error label", NULL);
		BFont font(be_plain_font);
		font.SetFace(B_ITALIC_FACE);
		fErrorLabel->SetFont(&font);
		fErrorLabel->Hide();

		BGridView* controls = new(std::nothrow) BGridView();
		if (controls == NULL)
			return;

		BGridLayout* layout = controls->GridLayout();

		fNetworkName = new(std::nothrow) BTextControl(B_TRANSLATE("Network name:"),
			"", NULL);
		if (fNetworkName == NULL)
			return;

		int32 row = 0;
		layout->AddItem(fNetworkName->CreateLabelLayoutItem(), 0, row);
		layout->AddItem(fNetworkName->CreateTextViewLayoutItem(), 1, row++, 2, 1);

		BPopUpMenu* authMenu = new(std::nothrow) BPopUpMenu("authMode");
		if (authMenu == NULL)
			return;

		fAuthOpen = new(std::nothrow) BMenuItem(B_TRANSLATE("Open ‒ no encryption"), NULL);
		authMenu->AddItem(fAuthOpen);
		fAuthWEP = new(std::nothrow) BMenuItem(B_TRANSLATE("WEP ‒ insecure encryption"), NULL);
		authMenu->AddItem(fAuthWEP);
		fAuthWPA = new(std::nothrow) BMenuItem(B_TRANSLATE("WPA/WPA2"), NULL);
		authMenu->AddItem(fAuthWPA);

		BMenuField* authMenuField = new(std::nothrow) BMenuField(
			B_TRANSLATE("Authentication:"), authMenu);
		if (authMenuField == NULL)
			return;

		layout->AddItem(authMenuField->CreateLabelLayoutItem(), 0, row);
		layout->AddItem(authMenuField->CreateMenuBarLayoutItem(), 1, row++, 2, 1);

		fPassword = new(std::nothrow) BTextControl(B_TRANSLATE("Password:"),
			"", NULL);
		if (fPassword == NULL)
			return;

		fPassword->TextView()->HideTyping(true);

		BLayoutItem* layoutItem = fPassword->CreateTextViewLayoutItem();
		layoutItem->SetExplicitMinSize(BSize((285 / 12) * be_plain_font->Size(),
			B_SIZE_UNSET));

		layout->AddItem(fPassword->CreateLabelLayoutItem(), 0, row);
		layout->AddItem(layoutItem, 1, row);

		fShowPassButton = new(std::nothrow) BButton("👁", new BMessage(kMessageShowPass));
		fShowPassButton->SetToolTip(B_TRANSLATE("Show password"));
		fShowPassButton->SetBehavior(BButton::B_TOGGLE_BEHAVIOR);
		const float height = fPassword->Bounds().Height();
		fShowPassButton->SetExplicitSize(BSize(height, height));

		fWebcamButton = new(std::nothrow) BButton("📷",
			new BMessage(kMessageScanWebcam));
		fWebcamButton->SetToolTip(B_TRANSLATE("Scan Wi-Fi QR code from webcam"));
		fWebcamButton->SetExplicitSize(BSize(height, height));

		fImageButton = new(std::nothrow) BButton("🖼",
			new BMessage(kMessageScanImage));
		fImageButton->SetToolTip(
			B_TRANSLATE("Scan Wi-Fi QR code from image file"));
		fImageButton->SetExplicitSize(BSize(height, height));

		BGroupView* passwordButtons = new(std::nothrow) BGroupView(B_HORIZONTAL,
			B_USE_SMALL_SPACING);
		passwordButtons->GroupLayout()->AddView(fShowPassButton);
		passwordButtons->GroupLayout()->AddView(fWebcamButton);
		passwordButtons->GroupLayout()->AddView(fImageButton);
		layout->AddView(passwordButtons, 2, row++);

		fPersist = new(std::nothrow) BCheckBox(B_TRANSLATE("Store this configuration"));
		layout->AddItem(BSpaceLayoutItem::CreateGlue(), 0, row);
		layout->AddView(fPersist, 1, row++);

		fCancelButton = new(std::nothrow) BButton(B_TRANSLATE("Cancel"),
			new BMessage(kMessageCancel));

		fOkButton = new(std::nothrow) BButton(B_TRANSLATE("OK"),
			new BMessage(kMessageOk));

		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.SetInsets(B_USE_WINDOW_INSETS)
			.Add(fErrorLabel)
			.Add(controls)
			.AddGroup(B_HORIZONTAL)
				.Add(fCancelButton)
				.AddGlue()
				.Add(fOkButton)
			.End()
		.End();
	}

	virtual
	~WirelessConfigView()
	{
		delete fFilePanel;
	}

	virtual void
	AttachedToWindow()
	{
		fShowPassButton->SetTarget(this);
		fWebcamButton->SetTarget(this);
		fImageButton->SetTarget(this);
		fCancelButton->SetTarget(Window());
		fOkButton->SetTarget(Window());
		fOkButton->MakeDefault(true);
		fPassword->MakeFocus(true);
	}

	virtual void
	MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMessageShowPass:
			{
				bool state = fShowPassButton->Value() == B_CONTROL_OFF;

				BString temp(fPassword->Text()); // HideTyping() will clear the text
				fPassword->TextView()->HideTyping(state);
				fPassword->SetText(temp);
				temp = "";

				if (state)
					fShowPassButton->SetToolTip(B_TRANSLATE("Show password"));
				else
					fShowPassButton->SetToolTip(B_TRANSLATE("Hide password"));
				return;
			}

			case kMessageScanWebcam:
			{
				if (fCaptureWindow == NULL) {
					fCaptureWindow = new(std::nothrow) WifiQrCaptureWindow(
						BMessenger(this), kMessageQrFound);
					fCaptureWindow->Show();
				}
				return;
			}

			case kMessageScanImage:
			{
				/*if (fFilePanel == NULL) {
					fFilePanel = new(std::nothrow) BFilePanel(B_OPEN_PANEL,
						new BMessenger(this));

					// WirelessConfigWindow uses B_FLOATING_ALL_WINDOW_FEEL,
					// i.e. it always stays on top of every other window on
					// the system. The file panel's own window defaults to a
					// normal feel, so it would never be able to appear in
					// front of it - give it the same feel so the two can
					// actually coexist and stack normally against each
					// other.
					if (fFilePanel != NULL) {
						BWindow* panelWindow = fFilePanel->Window();
						if (panelWindow != NULL && panelWindow->Lock()) {
							panelWindow->SetFeel(B_FLOATING_ALL_WINDOW_FEEL);//
							panelWindow->Unlock();
						}
					}
				}*/
				if (fFilePanel == NULL) {
                    // Creiamo il file panel senza forzare messenger complessi. 
                    // Di default invierà B_REFS_RECEIVED alla finestra (Window()) che possiede questa vista.
                    fFilePanel = new(std::nothrow) BFilePanel(B_OPEN_PANEL);
                }
				if (fFilePanel != NULL && !fFilePanel->IsShowing()) {
                    fFilePanel->Show();
                }
				return;
			}

			case B_REFS_RECEIVED:
			{
				entry_ref ref;
				if (message->FindRef("refs", &ref) == B_OK) {
					BBitmap* bitmap = BTranslationUtils::GetBitmap(&ref);
					BString ssid, password;
					uint32 authMode;
					bool hidden;
					bool found = bitmap != NULL
						&& ScanBitmapForWifiQr(bitmap, ssid, authMode,
							password, hidden);
					delete bitmap;

					if (found) {
						_ApplyScanResult(ssid, authMode, password);
					} else {
						BAlert* alert = new BAlert(
							B_TRANSLATE("No QR code found"),
							B_TRANSLATE("No Wi-Fi QR code could be found in "
								"that image."),
							B_TRANSLATE("OK"));
						alert->Go();
					}
				}
				return;
			}

			case kMessageQrFound:
			{
				fCaptureWindow = NULL;
					// It is in the middle of quitting itself.

				bool found = false;
				message->FindBool("found", &found);
				if (found) {
					BString ssid, password;
					uint32 authMode = B_NETWORK_AUTHENTICATION_NONE;
					message->FindString("ssid", &ssid);
					message->FindUInt32("authMode", &authMode);
					message->FindString("password", &password);
					_ApplyScanResult(ssid, authMode, password);
				}
				return;
			}
		}

		BView::MessageReceived(message);
	}

	void
	_ApplyScanResult(const BString& ssid, uint32 authMode,
		const BString& password)
	{
		if (!ssid.IsEmpty() && ssid != fNetworkName->Text()) {
			BString text;
			text.SetToFormat(B_TRANSLATE("The scanned QR code is for "
				"network '%s', not '%s'. The password will be used "
				"anyway."), ssid.String(), fNetworkName->Text());
			BAlert* alert = new BAlert(B_TRANSLATE("Different network"),
				text.String(), B_TRANSLATE("OK"));
			alert->Go();
		}

		switch (authMode) {
			default:
			case B_NETWORK_AUTHENTICATION_NONE:
				fAuthOpen->SetMarked(true);
				break;
			case B_NETWORK_AUTHENTICATION_WEP:
				fAuthWEP->SetMarked(true);
				break;
			case B_NETWORK_AUTHENTICATION_WPA:
			case B_NETWORK_AUTHENTICATION_WPA2:
				fAuthWPA->SetMarked(true);
				break;
		}

		fPassword->SetText(password.String());
	}

	void
	SetUp(const BMessage& message)
	{
		BString error;
		if (message.FindString("error", &error) == B_OK) {
			fErrorLabel->SetText(error);
			fErrorLabel->Show();
		}

		BString networkName;
		if (message.FindString("name", &networkName) == B_OK)
			fNetworkName->SetText(networkName);

		uint32 authMode;
		if (message.FindUInt32("authentication", &authMode) != B_OK)
			authMode = B_NETWORK_AUTHENTICATION_NONE;

		switch (authMode) {
			default:
			case B_NETWORK_AUTHENTICATION_NONE:
				fAuthOpen->SetMarked(true);
				break;
			case B_NETWORK_AUTHENTICATION_WEP:
				fAuthWEP->SetMarked(true);
				break;
			case B_NETWORK_AUTHENTICATION_WPA:
			case B_NETWORK_AUTHENTICATION_WPA2:
				fAuthWPA->SetMarked(true);
				break;
		}

		BString password;
		if (message.FindString("password", &password) == B_OK)
			fPassword->SetText(password);
	}

	void
	Complete(BMessage& message)
	{
		message.RemoveName("name");
		message.AddString("name", fNetworkName->Text());

		uint32 authMode = B_NETWORK_AUTHENTICATION_NONE;
		if (fAuthWEP->IsMarked())
			authMode = B_NETWORK_AUTHENTICATION_WEP;
		else if (fAuthWPA->IsMarked())
			authMode = B_NETWORK_AUTHENTICATION_WPA2;

		message.RemoveName("authentication");
		message.AddUInt32("authentication", authMode);

		message.RemoveName("password");
		message.AddString("password", fPassword->Text());

		message.RemoveName("persistent");
		message.AddBool("persistent", fPersist->Value() != 0);
	}

private:
	BStringView* fErrorLabel;
	BTextControl* fNetworkName;
	BMenuItem* fAuthOpen;
	BMenuItem* fAuthWEP;
	BMenuItem* fAuthWPA;
	BTextControl* fPassword;
	BCheckBox* fPersist;
	BButton* fCancelButton;
	BButton* fOkButton;
	BButton* fShowPassButton;
	BButton* fWebcamButton;
	BButton* fImageButton;
	BFilePanel* fFilePanel;
	BWindow* fCaptureWindow;
};


class WirelessConfigWindow : public BWindow {
public:
	WirelessConfigWindow()
		:
		BWindow(BRect(50, 50, 269, 302), B_TRANSLATE("Connect to a WiFi network"),
			B_TITLED_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL, B_NOT_CLOSABLE | B_NOT_RESIZABLE |
				B_ASYNCHRONOUS_CONTROLS	| B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
		fConfigView(NULL),
		fDoneSem(-1),
		fResult(B_ERROR)
	{
		fDoneSem = create_sem(0, "wireless config done");
		if (fDoneSem < 0)
			return;

		fConfigView = new(std::nothrow) WirelessConfigView();
		if (fConfigView == NULL)
			return;

		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.Add(fConfigView)
		.End();
	}

	virtual
	~WirelessConfigWindow()
	{
		if (fDoneSem >= 0)
			delete_sem(fDoneSem);
	}

	virtual void
	DispatchMessage(BMessage* message, BHandler* handler)
	{
		int8 key;
		if (message->what == B_KEY_DOWN
			&& message->FindInt8("byte", 0, &key) == B_OK
			&& key == B_ESCAPE) {
			PostMessage(kMessageCancel);
		}

		BWindow::DispatchMessage(message, handler);
	}

	virtual void
	MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMessageCancel:
			case kMessageOk:
				fResult = message->what == kMessageCancel ? B_CANCELED : B_OK;
				release_sem(fDoneSem);
				return;
		}

		BWindow::MessageReceived(message);
	}

	status_t
	WaitForDialog(BMessage& message)
	{
		fConfigView->SetUp(message);

		CenterOnScreen();
		Show();

		while (acquire_sem(fDoneSem) == B_INTERRUPTED)
			;

		status_t result = fResult;
		fConfigView->Complete(message);

		LockLooper();
		Quit();
		return result;
	}

private:
	WirelessConfigView* fConfigView;
	sem_id fDoneSem;
	status_t fResult;
};


status_t
wireless_config_dialog(BMessage& message)
{
	WirelessConfigWindow* configWindow
		= new(std::nothrow) WirelessConfigWindow;
	if (configWindow == NULL)
		return B_NO_MEMORY;

	return configWindow->WaitForDialog(message);
}

/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#include "boxes/connection_box.h"

#include "lang.h"
#include "storage/localstorage.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/input_fields.h"
#include "history/history_location_manager.h"
#include "styles/style_boxes.h"

ConnectionBox::ConnectionBox(QWidget *parent)
: _hostInput(this, st::connectionHostInputField, lang(lng_connection_host_ph), Global::ConnectionProxy().host)
, _portInput(this, st::connectionPortInputField, lang(lng_connection_port_ph), QString::number(Global::ConnectionProxy().port))
, _userInput(this, st::connectionUserInputField, lang(lng_connection_user_ph), Global::ConnectionProxy().user)
, _passwordInput(this, st::connectionPasswordInputField, lang(lng_connection_password_ph), Global::ConnectionProxy().password)
, _typeGroup(std::make_shared<Ui::RadioenumGroup<DBIConnectionType>>(Global::ConnectionType()))
, _autoRadio(this, _typeGroup, dbictAuto, lang(lng_connection_auto_rb), st::defaultBoxCheckbox)
, _httpProxyRadio(this, _typeGroup, dbictHttpProxy, lang(lng_connection_http_proxy_rb), st::defaultBoxCheckbox)
, _tcpProxyRadio(this, _typeGroup, dbictTcpProxy, lang(lng_connection_tcp_proxy_rb), st::defaultBoxCheckbox)
, _tryIPv6(this, lang(lng_connection_try_ipv6), Global::TryIPv6(), st::defaultBoxCheckbox) {
}

void ConnectionBox::prepare() {
	setTitle(lang(lng_connection_header));

	addButton(lang(lng_connection_save), [this] { onSave(); });
	addButton(lang(lng_cancel), [this] { closeBox(); });

	_typeGroup->setChangedCallback([this](DBIConnectionType value) { typeChanged(value); });

	connect(_hostInput, SIGNAL(submitted(bool)), this, SLOT(onSubmit()));
	connect(_portInput, SIGNAL(submitted(bool)), this, SLOT(onSubmit()));
	connect(_userInput, SIGNAL(submitted(bool)), this, SLOT(onSubmit()));
	connect(_passwordInput, SIGNAL(submitted(bool)), this, SLOT(onSubmit()));

	updateControlsVisibility();
}

void ConnectionBox::updateControlsVisibility() {
	auto newHeight = st::boxOptionListPadding.top() + _autoRadio->heightNoMargins() + st::boxOptionListSkip + _httpProxyRadio->heightNoMargins() + st::boxOptionListSkip + _tcpProxyRadio->heightNoMargins() + st::boxOptionListSkip + st::connectionIPv6Skip + _tryIPv6->heightNoMargins() + st::boxOptionListPadding.bottom() + st::boxPadding.bottom();
	if (_typeGroup->value() == dbictAuto) {
		_hostInput->hide();
		_portInput->hide();
		_userInput->hide();
		_passwordInput->hide();
	} else {
		newHeight += 2 * st::boxOptionInputSkip + 2 * _hostInput->height();
		_hostInput->show();
		_portInput->show();
		_userInput->show();
		_passwordInput->show();
	}

	setDimensions(st::boxWidth, newHeight);
	updateControlsPosition();
}

void ConnectionBox::setInnerFocus() {
	if (_hostInput->isHidden()) {
		setFocus();
	} else {
		_hostInput->setFocusFast();
	}
}

void ConnectionBox::resizeEvent(QResizeEvent *e) {
	BoxContent::resizeEvent(e);

	updateControlsPosition();
}

void ConnectionBox::updateControlsPosition() {
	auto type = _typeGroup->value();
	_autoRadio->moveToLeft(st::boxPadding.left() + st::boxOptionListPadding.left(), st::boxOptionListPadding.top());
	_httpProxyRadio->moveToLeft(st::boxPadding.left() + st::boxOptionListPadding.left(), _autoRadio->bottomNoMargins() + st::boxOptionListSkip);

	auto inputy = 0;
	if (type == dbictHttpProxy) {
		inputy = _httpProxyRadio->bottomNoMargins() + st::boxOptionInputSkip;
		_tcpProxyRadio->moveToLeft(st::boxPadding.left() + st::boxOptionListPadding.left(), inputy + st::boxOptionInputSkip + 2 * _hostInput->height() + st::boxOptionListSkip);
	} else {
		_tcpProxyRadio->moveToLeft(st::boxPadding.left() + st::boxOptionListPadding.left(), _httpProxyRadio->bottomNoMargins() + st::boxOptionListSkip);
		if (type == dbictTcpProxy) {
			inputy = _tcpProxyRadio->bottomNoMargins() + st::boxOptionInputSkip;
		}
	}

	if (inputy) {
		_hostInput->moveToLeft(st::boxPadding.left() + st::boxOptionListPadding.left() + st::defaultBoxCheckbox.textPosition.x() - st::defaultInputField.textMargins.left(), inputy);
		_portInput->moveToRight(st::boxPadding.right(), inputy);
		_userInput->moveToLeft(st::boxPadding.left() + st::boxOptionListPadding.left() + st::defaultBoxCheckbox.textPosition.x() - st::defaultInputField.textMargins.left(), _hostInput->y() + _hostInput->height() + st::boxOptionInputSkip);
		_passwordInput->moveToRight(st::boxPadding.right(), _userInput->y());
	}

	auto tryipv6y = ((type == dbictTcpProxy) ? _userInput->bottomNoMargins() : _tcpProxyRadio->bottomNoMargins()) + st::boxOptionListSkip + st::connectionIPv6Skip;
	_tryIPv6->moveToLeft(st::boxPadding.left() + st::boxOptionListPadding.left(), tryipv6y);
}

void ConnectionBox::typeChanged(DBIConnectionType type) {
	updateControlsVisibility();
	if (type != dbictAuto) {
		if (!_hostInput->hasFocus() && !_portInput->hasFocus() && !_userInput->hasFocus() && !_passwordInput->hasFocus()) {
			_hostInput->setFocusFast();
		}
		if ((type == dbictHttpProxy) && !_portInput->getLastText().toInt()) {
			_portInput->setText(qsl("80"));
			_portInput->finishAnimations();
		}
	}
	update();
}

void ConnectionBox::onSubmit() {
	if (_hostInput->hasFocus()) {
		if (!_hostInput->getLastText().trimmed().isEmpty()) {
			_portInput->setFocus();
		} else {
			_hostInput->showError();
		}
	} else if (_portInput->hasFocus()) {
		if (_portInput->getLastText().trimmed().toInt() > 0) {
			_userInput->setFocus();
		} else {
			_portInput->showError();
		}
	} else if (_userInput->hasFocus()) {
		_passwordInput->setFocus();
	} else if (_passwordInput->hasFocus()) {
		if (_hostInput->getLastText().trimmed().isEmpty()) {
			_hostInput->setFocus();
			_hostInput->showError();
		} else if (_portInput->getLastText().trimmed().toInt() <= 0) {
			_portInput->setFocus();
			_portInput->showError();
		} else {
			onSave();
		}
	}
}

void ConnectionBox::onSave() {
	auto type = _typeGroup->value();
	if (type == dbictAuto) {
		Global::SetConnectionType(type);
		Global::SetConnectionProxy(ProxyData());
#ifndef TDESKTOP_DISABLE_NETWORK_PROXY
		QNetworkProxyFactory::setUseSystemConfiguration(false);
		QNetworkProxyFactory::setUseSystemConfiguration(true);
#endif // !TDESKTOP_DISABLE_NETWORK_PROXY
	} else {
		ProxyData p;
		p.host = _hostInput->getLastText().trimmed();
		p.user = _userInput->getLastText().trimmed();
		p.password = _passwordInput->getLastText().trimmed();
		p.port = _portInput->getLastText().toInt();
		if (p.host.isEmpty()) {
			_hostInput->setFocus();
			return;
		} else if (!p.port) {
			_portInput->setFocus();
			return;
		}
		Global::SetConnectionType(type);
		Global::SetConnectionProxy(p);
	}
	if (cPlatform() == dbipWindows && Global::TryIPv6() != _tryIPv6->checked()) {
		Global::SetTryIPv6(_tryIPv6->checked());
		Local::writeSettings();
		Global::RefConnectionTypeChanged().notify();

		App::restart();
	} else {
		Global::SetTryIPv6(_tryIPv6->checked());
		Local::writeSettings();
		Global::RefConnectionTypeChanged().notify();

		MTP::restart();
		reinitLocationManager();
		reinitWebLoadManager();
		closeBox();
	}
}

AutoDownloadBox::AutoDownloadBox(QWidget *parent)
: _photoPrivate(this, lang(lng_media_auto_private_chats), !(cAutoDownloadPhoto() & dbiadNoPrivate), st::defaultBoxCheckbox)
, _photoGroups(this,  lang(lng_media_auto_groups), !(cAutoDownloadPhoto() & dbiadNoGroups), st::defaultBoxCheckbox)
, _audioPrivate(this, lang(lng_media_auto_private_chats), !(cAutoDownloadAudio() & dbiadNoPrivate), st::defaultBoxCheckbox)
, _audioGroups(this, lang(lng_media_auto_groups), !(cAutoDownloadAudio() & dbiadNoGroups), st::defaultBoxCheckbox)
, _gifPrivate(this, lang(lng_media_auto_private_chats), !(cAutoDownloadGif() & dbiadNoPrivate), st::defaultBoxCheckbox)
, _gifGroups(this, lang(lng_media_auto_groups), !(cAutoDownloadGif() & dbiadNoGroups), st::defaultBoxCheckbox)
, _gifPlay(this, lang(lng_media_auto_play), cAutoPlayGif(), st::defaultBoxCheckbox)
, _sectionHeight(st::boxTitleHeight + 2 * (st::defaultBoxCheckbox.height + st::setLittleSkip)) {
}

void AutoDownloadBox::prepare() {
	addButton(lang(lng_connection_save), [this] { onSave(); });
	addButton(lang(lng_cancel), [this] { closeBox(); });

	setDimensions(st::boxWidth, 3 * _sectionHeight - st::autoDownloadTopDelta + st::setLittleSkip + _gifPlay->heightNoMargins() + st::setLittleSkip);
}

void AutoDownloadBox::paintEvent(QPaintEvent *e) {
	BoxContent::paintEvent(e);

	Painter p(this);

	p.setPen(st::boxTitleFg);
	p.setFont(st::autoDownloadTitleFont);
	p.drawTextLeft(st::autoDownloadTitlePosition.x(), st::autoDownloadTitlePosition.y(), width(), lang(lng_media_auto_photo));
	p.drawTextLeft(st::autoDownloadTitlePosition.x(), _sectionHeight + st::autoDownloadTitlePosition.y(), width(), lang(lng_media_auto_audio));
	p.drawTextLeft(st::autoDownloadTitlePosition.x(), 2 * _sectionHeight + st::autoDownloadTitlePosition.y(), width(), lang(lng_media_auto_gif));
}

void AutoDownloadBox::resizeEvent(QResizeEvent *e) {
	BoxContent::resizeEvent(e);

	auto top = st::boxTitleHeight - st::autoDownloadTopDelta;
	_photoPrivate->moveToLeft(st::boxTitlePosition.x(), top + st::setLittleSkip);
	_photoGroups->moveToLeft(st::boxTitlePosition.x(), _photoPrivate->bottomNoMargins() + st::setLittleSkip);

	_audioPrivate->moveToLeft(st::boxTitlePosition.x(), _sectionHeight + top + st::setLittleSkip);
	_audioGroups->moveToLeft(st::boxTitlePosition.x(), _audioPrivate->bottomNoMargins() + st::setLittleSkip);

	_gifPrivate->moveToLeft(st::boxTitlePosition.x(), 2 * _sectionHeight + top + st::setLittleSkip);
	_gifGroups->moveToLeft(st::boxTitlePosition.x(), _gifPrivate->bottomNoMargins() + st::setLittleSkip);
	_gifPlay->moveToLeft(st::boxTitlePosition.x(), _gifGroups->bottomNoMargins() + st::setLittleSkip);
}

void AutoDownloadBox::onSave() {
	bool changed = false;
	int32 autoDownloadPhoto = (_photoPrivate->checked() ? 0 : dbiadNoPrivate) | (_photoGroups->checked() ? 0 : dbiadNoGroups);
	if (cAutoDownloadPhoto() != autoDownloadPhoto) {
		bool enabledPrivate = ((cAutoDownloadPhoto() & dbiadNoPrivate) && !(autoDownloadPhoto & dbiadNoPrivate));
		bool enabledGroups = ((cAutoDownloadPhoto() & dbiadNoGroups) && !(autoDownloadPhoto & dbiadNoGroups));
		cSetAutoDownloadPhoto(autoDownloadPhoto);
		if (enabledPrivate || enabledGroups) {
			const PhotosData &data(App::photosData());
			for (PhotosData::const_iterator i = data.cbegin(), e = data.cend(); i != e; ++i) {
				i.value()->automaticLoadSettingsChanged();
			}
		}
		changed = true;
	}
	int32 autoDownloadAudio = (_audioPrivate->checked() ? 0 : dbiadNoPrivate) | (_audioGroups->checked() ? 0 : dbiadNoGroups);
	if (cAutoDownloadAudio() != autoDownloadAudio) {
		bool enabledPrivate = ((cAutoDownloadAudio() & dbiadNoPrivate) && !(autoDownloadAudio & dbiadNoPrivate));
		bool enabledGroups = ((cAutoDownloadAudio() & dbiadNoGroups) && !(autoDownloadAudio & dbiadNoGroups));
		cSetAutoDownloadAudio(autoDownloadAudio);
		if (enabledPrivate || enabledGroups) {
			for (auto document : App::documentsData()) {
				if (document->voice()) {
					document->automaticLoadSettingsChanged();
				}
			}
		}
		changed = true;
	}
	int32 autoDownloadGif = (_gifPrivate->checked() ? 0 : dbiadNoPrivate) | (_gifGroups->checked() ? 0 : dbiadNoGroups);
	if (cAutoDownloadGif() != autoDownloadGif) {
		bool enabledPrivate = ((cAutoDownloadGif() & dbiadNoPrivate) && !(autoDownloadGif & dbiadNoPrivate));
		bool enabledGroups = ((cAutoDownloadGif() & dbiadNoGroups) && !(autoDownloadGif & dbiadNoGroups));
		cSetAutoDownloadGif(autoDownloadGif);
		if (enabledPrivate || enabledGroups) {
			for (auto document : App::documentsData()) {
				if (document->isAnimation()) {
					document->automaticLoadSettingsChanged();
				}
			}
		}
		changed = true;
	}
	if (cAutoPlayGif() != _gifPlay->checked()) {
		cSetAutoPlayGif(_gifPlay->checked());
		if (!cAutoPlayGif()) {
			App::stopGifItems();
		}
		changed = true;
	}
	if (changed) Local::writeUserSettings();
	closeBox();
}

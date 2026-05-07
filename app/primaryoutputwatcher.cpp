/*
    SPDX-FileCopyrightText: 2013 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "primaryoutputwatcher.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_event.h>

namespace {

xcb_connection_t *x11Connection()
{
    auto *x11 = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    return x11 ? x11->connection() : nullptr;
}

}

PrimaryOutputWatcher::PrimaryOutputWatcher(QObject *parent)
    : QObject(parent)
{
    if (qGuiApp->primaryScreen()) {
        m_primaryOutputName = qGuiApp->primaryScreen()->name();
    }

    xcb_connection_t *connection = x11Connection();
    if (connection) {
        qGuiApp->installNativeEventFilter(this);

        const xcb_query_extension_reply_t *reply = xcb_get_extension_data(connection, &xcb_randr_id);
        if (reply && reply->present) {
            m_xrandrExtensionOffset = reply->first_event;
        }
    }

    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this](QScreen *newPrimary) {
        if (newPrimary) {
            setPrimaryOutputName(newPrimary->name());
        }
    });
}

void PrimaryOutputWatcher::setPrimaryOutputName(const QString &newOutputName)
{
    if (newOutputName != m_primaryOutputName) {
        const QString oldOutputName = m_primaryOutputName;
        m_primaryOutputName = newOutputName;
        Q_EMIT primaryOutputNameChanged(oldOutputName, newOutputName);
    }
}

bool PrimaryOutputWatcher::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result);

    if (m_xrandrExtensionOffset == 0 || eventType.isEmpty() || eventType[0] != 'x') {
        return false;
    }

    xcb_generic_event_t *event = static_cast<xcb_generic_event_t *>(message);
    const auto responseType = XCB_EVENT_RESPONSE_TYPE(event);

    if (responseType == m_xrandrExtensionOffset + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
        QTimer::singleShot(0, this, [this]() {
            if (qGuiApp->primaryScreen()) {
                setPrimaryOutputName(qGuiApp->primaryScreen()->name());
            }
        });
    }

    return false;
}

QScreen *PrimaryOutputWatcher::screenForName(const QString &outputName) const
{
    const auto screens = qGuiApp->screens();
    for (auto screen : screens) {
        if (screen->name() == outputName) {
            return screen;
        }
    }
    return nullptr;
}

QScreen *PrimaryOutputWatcher::primaryScreen() const
{
    auto screen = screenForName(m_primaryOutputName);
    if (!screen) {
        qDebug() << "PrimaryOutputWatcher: Could not find primary screen:" << m_primaryOutputName;
        return qGuiApp->primaryScreen();
    }
    return screen;
}

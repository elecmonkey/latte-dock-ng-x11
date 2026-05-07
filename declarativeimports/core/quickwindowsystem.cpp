/*
    SPDX-FileCopyrightText: 2016 Smith AR <audoban@openmailbox.org>
    SPDX-FileCopyrightText: 2016 Michail Vourlakos <mvourlakos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "quickwindowsystem.h"

// Qt
#include <QDebug>

// KDE
#include <KWindowSystem>
#include <KX11Extras>

namespace Latte {

QuickWindowSystem::QuickWindowSystem(QObject *parent)
    : QObject(parent)
{
    m_compositing = KX11Extras::compositingActive();
    connect(KX11Extras::self(), &KX11Extras::compositingChanged, this, [this](bool enabled) {
        if (m_compositing == enabled) {
            return;
        }

        m_compositing = enabled;
        Q_EMIT compositingChanged();
    });
}

QuickWindowSystem::~QuickWindowSystem()
{
    qDebug() << staticMetaObject.className() << "destructed";
}

bool QuickWindowSystem::compositingActive() const
{
    return m_compositing;
}

bool QuickWindowSystem::isPlatformWayland() const
{
    return KWindowSystem::isPlatformWayland();
}

bool QuickWindowSystem::isPlatformX11() const
{
    return KWindowSystem::isPlatformX11();
}

} //end of namespace

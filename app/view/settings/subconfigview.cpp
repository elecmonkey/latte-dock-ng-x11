/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "subconfigview.h"

//local
#include <config-latte.h>
#include "../../apptypes.h"
#include "../view.h"
#include "../../lattecorona.h"
#include "../../layouts/manager.h"
#include "../../plasma/extended/theme.h"
#include "../../settings/universalsettings.h"
#include "../../shortcuts/globalshortcuts.h"
#include "../../shortcuts/shortcutstracker.h"
#include "../../wm/abstractwindowinterface.h"

// KDE
#include <KLocalizedContext>
#include <KDeclarative/KDeclarative>

namespace Latte {
namespace ViewPart {

SubConfigView::SubConfigView(Latte::View *view, const QString &title, const bool &isNormalWindow)
    : QQuickView(nullptr),
      m_isNormalWindow(isNormalWindow)
{
    m_corona = qobject_cast<Latte::Corona *>(view->containment()->corona());

    m_validTitle = title;
    setTitle(m_validTitle);

    setScreen(view->screen());
    setIcon(qGuiApp->windowIcon());

    if (!m_isNormalWindow) {
        setFlags(wFlags());
        m_corona->wm()->setViewExtraFlags(this, true);
    }

    m_screenSyncTimer.setSingleShot(true);
    m_screenSyncTimer.setInterval(100);

    connections << connect(&m_screenSyncTimer, &QTimer::timeout, this, [this]() {
        if (!m_latteView) {
            return;
        }

        setScreen(m_latteView->screen());

        syncGeometry();
    });

    m_showTimer.setSingleShot(true);
    m_showTimer.setInterval(0);

    connections << connect(&m_showTimer, &QTimer::timeout, this, [this]() {
        syncSlideEffect();
        show();
    });
}

SubConfigView::~SubConfigView()
{
    qDebug() << validTitle() << " deleting...";

    m_corona->dialogShadows()->removeWindow(this);

    if (!m_trackedWindowId.isNull()) {
        m_corona->wm()->unregisterIgnoredWindow(m_trackedWindowId);
    }

    for (const auto &var : connections) {
        QObject::disconnect(var);
    }

    for (const auto &var : viewconnections) {
        QObject::disconnect(var);
    }
}

void SubConfigView::init()
{
    qDebug() << validTitle() << " : initialization started...";

    setDefaultAlphaBuffer(true);
    setColor(Qt::transparent);

    rootContext()->setContextProperty(QStringLiteral("viewConfig"), this);
    rootContext()->setContextProperty(QStringLiteral("shortcutsEngine"), m_corona->globalShortcuts()->shortcutsTracker());

    if (m_corona) {
        rootContext()->setContextProperty(QStringLiteral("universalSettings"), m_corona->universalSettings());
        rootContext()->setContextProperty(QStringLiteral("layoutsManager"), m_corona->layoutsManager());
        rootContext()->setContextProperty(QStringLiteral("themeExtended"), m_corona->themeExtended());
    }

    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setTranslationDomain(QString::fromLatin1(App::TRANSLATIONDOMAIN));
    kdeclarative.setupContext();
    kdeclarative.setupEngine(engine());

}

Qt::WindowFlags SubConfigView::wFlags() const
{
    return (flags() | Qt::FramelessWindowHint) & ~Qt::WindowDoesNotAcceptFocus;
}

QString SubConfigView::validTitle() const
{
    return m_validTitle;
}

Latte::WindowSystem::WindowId SubConfigView::trackedWindowId()
{
    if (m_trackedWindowId.isEmpty()) {
        m_trackedWindowId = QByteArray::number(static_cast<qulonglong>(winId()));
        m_corona->wm()->registerIgnoredWindow(m_trackedWindowId);
    }

    return m_trackedWindowId;
}

Latte::Corona *SubConfigView::corona() const
{
    return m_corona;
}

Latte::View *SubConfigView::parentView() const
{
    return m_latteView;
}

void SubConfigView::setParentView(Latte::View *view, const bool &immediate)
{
    if (m_latteView == view) {
        return;
    }

    initParentView(view);
}

void SubConfigView::initParentView(Latte::View *view)
{
    for (const auto &var : viewconnections) {
        QObject::disconnect(var);
    }

    m_latteView = view;

    viewconnections << connect(m_latteView->positioner(), &ViewPart::Positioner::canvasGeometryChanged, this, &SubConfigView::syncGeometry);

    //! Assign app interfaces in be accessible through containment graphic item
    QQuickItem *containmentGraphicItem = qobject_cast<QQuickItem *>(m_latteView->containment()->property("_plasma_graphicObject").value<QObject *>());
    rootContext()->setContextProperty(QStringLiteral("plasmoid"), containmentGraphicItem);
    rootContext()->setContextProperty(QStringLiteral("latteView"), m_latteView);
}

void SubConfigView::requestActivate()
{
    m_corona->wm()->requestActivate(trackedWindowId());
}

void SubConfigView::showAfter(int msecs)
{
    if (isVisible()) {
        return;
    }

    m_showTimer.setInterval(msecs);
    m_showTimer.start();

}

void SubConfigView::syncSlideEffect()
{
    if (!m_latteView || !m_latteView->containment()) {
        return;
    }

    auto slideLocation = WindowSystem::AbstractWindowInterface::Slide::None;

    switch (m_latteView->containment()->location()) {
    case Plasma::Types::TopEdge:
        slideLocation = WindowSystem::AbstractWindowInterface::Slide::Top;
        break;

    case Plasma::Types::RightEdge:
        slideLocation = WindowSystem::AbstractWindowInterface::Slide::Right;
        break;

    case Plasma::Types::BottomEdge:
        slideLocation = WindowSystem::AbstractWindowInterface::Slide::Bottom;
        break;

    case Plasma::Types::LeftEdge:
        slideLocation = WindowSystem::AbstractWindowInterface::Slide::Left;
        break;

    default:
        qDebug() << staticMetaObject.className() << "wrong location";
        break;
    }

    m_corona->wm()->slideWindow(*this, slideLocation);
}

void SubConfigView::showEvent(QShowEvent *ev)
{
    QQuickView::showEvent(ev);

    m_corona->dialogShadows()->addWindow(this, m_enabledBorders);
}

bool SubConfigView::event(QEvent *e)
{
    if (e->type() == QEvent::PlatformSurface) {
        if (auto pe = dynamic_cast<QPlatformSurfaceEvent *>(e)) {
            switch (pe->surfaceEventType()) {
            case QPlatformSurfaceEvent::SurfaceCreated:
                break;

            case QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed:
                break;
            }
        }
    }

    return QQuickView::event(e);
}

Plasma::FrameSvg::EnabledBorders SubConfigView::enabledBorders() const
{
    return m_enabledBorders;
}

}
}

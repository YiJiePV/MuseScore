//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2019 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#include "palettewidget.h"

#include "palette/paletteworkspace.h"
#include "plugin/qmliconview.h"
#include "preferences.h"

#include <QQmlContext>

namespace Ms {

//---------------------------------------------------------
//   PaletteQmlInterface::setPaletteBackground
//---------------------------------------------------------

void PaletteQmlInterface::setPaletteBackground(const QColor& val)
      {
      if (_paletteBackground != val) {
            _paletteBackground = val;
            emit paletteBackgroundChanged();
            }
      }

//---------------------------------------------------------
//   PaletteWidget
//---------------------------------------------------------

PaletteWidget::PaletteWidget(PaletteWorkspace* w, QQmlEngine* e, QWidget* parent, Qt::WindowFlags flags)
   : QmlDockWidget(e, qApp->translate("Ms::PaletteBox", "Palettes"), parent, flags)
      {
      registerQmlTypes();

      const bool useSinglePalette = preferences.getBool(PREF_APP_USESINGLEPALETTE);

      QQmlContext* ctx = rootContext();
      Q_ASSERT(ctx);

      qmlInterface = new PaletteQmlInterface(w, this);
      setupStyle();
      ctx->setContextProperty("mscore", qmlInterface);

      setSource(QUrl("qrc:/qml/palettes/PalettesWidget.qml"));

      singlePaletteAction = new QAction(this);
      singlePaletteAction->setCheckable(true);
      singlePaletteAction->setChecked(useSinglePalette);
      addAction(singlePaletteAction);
      connect(singlePaletteAction, &QAction::toggled, this, &PaletteWidget::setSinglePalette);

      setContextMenuPolicy(Qt::ActionsContextMenu);
      setObjectName("palette-widget");
      setAllowedAreas(Qt::DockWidgetAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea));

      retranslate();
      }

PaletteWidget::PaletteWidget(PaletteWorkspace* w, QWidget* parent, Qt::WindowFlags flags)
   : PaletteWidget(w, nullptr, parent, flags)
      {}

//---------------------------------------------------------
//   PaletteWidget::setSinglePalette
//---------------------------------------------------------

void PaletteWidget::setSinglePalette(bool val)
      {
      preferences.setPreference(PREF_APP_USESINGLEPALETTE, val);
      }

//---------------------------------------------------------
//   retranslate
//---------------------------------------------------------

void PaletteWidget::retranslate()
      {
      setWindowTitle(qApp->translate("Ms::PaletteBox", "Palettes"));
      singlePaletteAction->setText(qApp->translate("Ms::PaletteBox", "Single Palette"));
      }

//---------------------------------------------------------
//   setupStyle
//---------------------------------------------------------

void PaletteWidget::setupStyle()
      {
      if (preferences.getBool(PREF_UI_CANVAS_FG_USECOLOR) && preferences.getBool(PREF_UI_CANVAS_FG_USECOLOR_IN_PALETTES))
            qmlInterface->setPaletteBackground(preferences.getColor(PREF_UI_CANVAS_FG_COLOR));
      else
            qmlInterface->setPaletteBackground(QColor("#f9f9f9"));
      }

//---------------------------------------------------------
//   changeEvent
//---------------------------------------------------------

void PaletteWidget::changeEvent(QEvent* evt)
      {
      QmlDockWidget::changeEvent(evt);
      switch (evt->type()) {
            case QEvent::LanguageChange:
                  retranslate();
                  break;
            case QEvent::StyleChange:
                  setupStyle();
                  break;
            default:
                  break;
            }
      }

//---------------------------------------------------------
//   registerQmlTypes
//---------------------------------------------------------

void PaletteWidget::registerQmlTypes()
      {
      static bool registered = false;
      if (registered)
            return;

      qmlRegisterUncreatableType<PaletteWorkspace>("MuseScore.Palette", 3, 3, "PaletteWorkspace", "Cannot create palette workspace from QML");
      qmlRegisterUncreatableType<AbstractPaletteController>("MuseScore.Palette", 3, 3, "PaletteController", "Cannot create palette controller from QML");

      qmlRegisterUncreatableType<PaletteTreeModel>("MuseScore.Palette", 3, 3, "PaletteTreeModel", "Cannot create palette model from QML");
      qmlRegisterUncreatableType<FilterPaletteTreeModel>("MuseScore.Palette", 3, 3, "FilterPaletteTreeModel", "Cannot create palette model from QML");

      qmlRegisterType<QmlIconView>("MuseScore.Views", 3, 3, "QmlIconView");

      registered = true;
      }

}

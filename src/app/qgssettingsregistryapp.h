/***************************************************************************
                         qgssettingsregistryapp.cpp
                         ----------------------
    begin                : January 2022
    copyright            : (C) 2022 by Denis Rouzaud
    email                : denis@opengis.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSSETTINGSREGISTRYAPP_H
#define QGSSETTINGSREGISTRYAPP_H

#include "qgis_app.h"
#include "qgssettingsregistry.h"

class QgsSettingsEntryBool;

Q_NOWARN_DEPRECATED_PUSH
class APP_EXPORT QgsSettingsRegistryApp : public QgsSettingsRegistry
{
    Q_NOWARN_DEPRECATED_POP
    // TODO QGIS 4 do not inherit QgsSettingsRegistry
  public:
    QgsSettingsRegistryApp();
    ~QgsSettingsRegistryApp();
};

#endif // QGSSETTINGSREGISTRYAPP_H

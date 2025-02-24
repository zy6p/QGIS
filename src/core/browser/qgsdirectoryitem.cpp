/***************************************************************************
                             qgsddirectoryitem.cpp
                             -------------------
    begin                : 2011-04-01
    copyright            : (C) 2011 Radim Blazek
    email                : radim dot blazek at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsdirectoryitem.h"
#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsdataitemprovider.h"
#include "qgsdataitemproviderregistry.h"
#include "qgsdataprovider.h"
#include "qgszipitem.h"
#include "qgsprojectitem.h"
#include <QFileSystemWatcher>
#include <QDir>
#include <QMouseEvent>
#include <QTimer>
#include <QMenu>
#include <QAction>

//
// QgsDirectoryItem
//

QgsDirectoryItem::QgsDirectoryItem( QgsDataItem *parent, const QString &name, const QString &path )
  : QgsDataCollectionItem( parent, QDir::toNativeSeparators( name ), path )
  , mDirPath( path )
  , mRefreshLater( false )
{
  mType = Directory;
  init();
}

QgsDirectoryItem::QgsDirectoryItem( QgsDataItem *parent, const QString &name,
                                    const QString &dirPath, const QString &path,
                                    const QString &providerKey )
  : QgsDataCollectionItem( parent, QDir::toNativeSeparators( name ), path, providerKey )
  , mDirPath( dirPath )
  , mRefreshLater( false )
{
  QgsSettings settings;
  settings.beginGroup( QStringLiteral( "qgis/browserPathColors" ) );
  QString settingKey = mDirPath;
  settingKey.replace( '/', QStringLiteral( "|||" ) );
  if ( settings.childKeys().contains( settingKey ) )
  {
    const QString colorString = settings.value( settingKey ).toString();
    mIconColor = QColor( colorString );
  }
  settings.endGroup();

  mType = Directory;
  init();
}

void QgsDirectoryItem::init()
{
  setToolTip( QDir::toNativeSeparators( mDirPath ) );
}

QColor QgsDirectoryItem::iconColor() const
{
  return mIconColor;
}

void QgsDirectoryItem::setIconColor( const QColor &color )
{
  if ( color == mIconColor )
    return;

  mIconColor = color;
  emit dataChanged( this );
}

void QgsDirectoryItem::setCustomColor( const QString &directory, const QColor &color )
{
  QgsSettings settings;
  settings.beginGroup( QStringLiteral( "qgis/browserPathColors" ) );
  QString settingKey = directory;
  settingKey.replace( '/', QStringLiteral( "|||" ) );
  if ( color.isValid() )
    settings.setValue( settingKey, color.name( QColor::HexArgb ) );
  else
    settings.remove( settingKey );
  settings.endGroup();
}

QIcon QgsDirectoryItem::icon()
{
  if ( mDirPath == QDir::homePath() )
    return homeDirIcon( mIconColor, mIconColor.darker() );

  // still loading? show the spinner
  if ( state() == Populating )
    return QgsDataItem::icon();

  // symbolic link? use link icon
  QFileInfo fi( mDirPath );
  if ( fi.isDir() && fi.isSymLink() )
  {
    return mIconColor.isValid()
           ? QgsApplication::getThemeIcon( QStringLiteral( "/mIconFolderLinkParams.svg" ), mIconColor, mIconColor.darker() )
           : QgsApplication::getThemeIcon( QStringLiteral( "/mIconFolderLink.svg" ) );
  }

  // loaded? show the open dir icon
  if ( state() == Populated )
    return openDirIcon( mIconColor, mIconColor.darker() );

  // show the closed dir icon
  return iconDir( mIconColor, mIconColor.darker() );
}


QVector<QgsDataItem *> QgsDirectoryItem::createChildren()
{
  QVector<QgsDataItem *> children;
  QDir dir( mDirPath );

  const QList<QgsDataItemProvider *> providers = QgsApplication::dataItemProviderRegistry()->providers();

  QStringList entries = dir.entryList( QDir::AllDirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase );
  const auto constEntries = entries;
  for ( const QString &subdir : constEntries )
  {
    if ( mRefreshLater )
    {
      deleteLater( children );
      return children;
    }

    QString subdirPath = dir.absoluteFilePath( subdir );

    QgsDebugMsgLevel( QStringLiteral( "creating subdir: %1" ).arg( subdirPath ), 2 );

    QString path = mPath + '/' + subdir; // may differ from subdirPath
    if ( QgsDirectoryItem::hiddenPath( path ) )
      continue;

    bool handledByProvider = false;
    for ( QgsDataItemProvider *provider : providers )
    {
      if ( provider->handlesDirectoryPath( path ) )
      {
        handledByProvider = true;
        break;
      }
    }
    if ( handledByProvider )
      continue;

    QgsDirectoryItem *item = new QgsDirectoryItem( this, subdir, subdirPath, path );

    // we want directories shown before files
    item->setSortKey( QStringLiteral( "  %1" ).arg( subdir ) );

    // propagate signals up to top

    children.append( item );
  }

  QStringList fileEntries = dir.entryList( QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files, QDir::Name );
  const auto constFileEntries = fileEntries;
  for ( const QString &name : constFileEntries )
  {
    if ( mRefreshLater )
    {
      deleteLater( children );
      return children;
    }

    QString path = dir.absoluteFilePath( name );
    QFileInfo fileInfo( path );

    if ( fileInfo.suffix().compare( QLatin1String( "zip" ), Qt::CaseInsensitive ) == 0 ||
         fileInfo.suffix().compare( QLatin1String( "tar" ), Qt::CaseInsensitive ) == 0 )
    {
      QgsDataItem *item = QgsZipItem::itemFromPath( this, path, name, mPath + '/' + name );
      if ( item )
      {
        children.append( item );
        continue;
      }
    }

    bool createdItem = false;
    for ( QgsDataItemProvider *provider : providers )
    {
      int capabilities = provider->capabilities();

      if ( !( ( fileInfo.isFile() && ( capabilities & QgsDataProvider::File ) ) ||
              ( fileInfo.isDir() && ( capabilities & QgsDataProvider::Dir ) ) ) )
      {
        continue;
      }

      QgsDataItem *item = provider->createDataItem( path, this );
      if ( item )
      {
        children.append( item );
        createdItem = true;
      }
    }

    if ( !createdItem )
    {
      // if item is a QGIS project, and no specific item provider has overridden handling of
      // project items, then use the default project item behavior
      if ( fileInfo.suffix().compare( QLatin1String( "qgs" ), Qt::CaseInsensitive ) == 0 ||
           fileInfo.suffix().compare( QLatin1String( "qgz" ), Qt::CaseInsensitive ) == 0 )
      {
        QgsDataItem *item = new QgsProjectItem( this, fileInfo.completeBaseName(), path );
        children.append( item );
        continue;
      }
    }

  }
  return children;
}

void QgsDirectoryItem::setState( State state )
{
  QgsDataCollectionItem::setState( state );

  if ( state == Populated )
  {
    if ( !mFileSystemWatcher )
    {
      mFileSystemWatcher = new QFileSystemWatcher( this );
      mFileSystemWatcher->addPath( mDirPath );
      connect( mFileSystemWatcher, &QFileSystemWatcher::directoryChanged, this, &QgsDirectoryItem::directoryChanged );
    }
    mLastScan = QDateTime::currentDateTime();
  }
  else if ( state == NotPopulated )
  {
    if ( mFileSystemWatcher )
    {
      delete mFileSystemWatcher;
      mFileSystemWatcher = nullptr;
    }
  }
}

void QgsDirectoryItem::directoryChanged()
{
  // If the last scan was less than 10 seconds ago, skip this
  if ( mLastScan.msecsTo( QDateTime::currentDateTime() ) < QgsSettings().value( QStringLiteral( "browser/minscaninterval" ), 10000 ).toInt() )
  {
    return;
  }
  if ( state() == Populating )
  {
    // schedule to refresh later, because refresh() simply returns if Populating
    mRefreshLater = true;
  }
  else
  {
    // We definintely don't want the temporary files created by sqlite
    // to re-trigger a refresh in an infinite loop.
    disconnect( mFileSystemWatcher, &QFileSystemWatcher::directoryChanged, this, &QgsDirectoryItem::directoryChanged );
    // QFileSystemWhatcher::directoryChanged is emitted when a
    // file is created and not when it is closed/flushed.
    //
    // Delay to give to OS the time to complete writing the file
    // this happens when a new file appears in the directory and
    // the item's children thread will try to open the file with
    // GDAL or OGR even if it is still being written.
    QTimer::singleShot( 100, this, [ = ] { refresh(); } );
  }
}

bool QgsDirectoryItem::hiddenPath( const QString &path )
{
  QgsSettings settings;
  QStringList hiddenItems = settings.value( QStringLiteral( "browser/hiddenPaths" ),
                            QStringList() ).toStringList();
  int idx = hiddenItems.indexOf( path );
  return ( idx > -1 );
}

void QgsDirectoryItem::childrenCreated()
{
  QgsDebugMsgLevel( QStringLiteral( "mRefreshLater = %1" ).arg( mRefreshLater ), 3 );

  if ( mRefreshLater )
  {
    QgsDebugMsgLevel( QStringLiteral( "directory changed during createChidren() -> refresh() again" ), 3 );
    mRefreshLater = false;
    setState( Populated );
    refresh();
  }
  else
  {
    QgsDataCollectionItem::childrenCreated();
  }
  // Re-connect the file watcher after all children have been created
  connect( mFileSystemWatcher, &QFileSystemWatcher::directoryChanged, this, &QgsDirectoryItem::directoryChanged );
}

bool QgsDirectoryItem::equal( const QgsDataItem *other )
{
  //QgsDebugMsg ( mPath + " x " + other->mPath );
  if ( type() != other->type() )
  {
    return false;
  }
  return ( path() == other->path() );
}

QWidget *QgsDirectoryItem::paramWidget()
{
  return new QgsDirectoryParamWidget( mPath );
}

QgsMimeDataUtils::UriList QgsDirectoryItem::mimeUris() const
{
  QgsMimeDataUtils::Uri u;
  u.layerType = QStringLiteral( "directory" );
  u.name = mName;
  u.uri = mDirPath;
  return { u };
}

//
// QgsDirectoryParamWidget
//
QgsDirectoryParamWidget::QgsDirectoryParamWidget( const QString &path, QWidget *parent )
  : QTreeWidget( parent )
{
  setRootIsDecorated( false );

  // name, size, date, permissions, owner, group, type
  setColumnCount( 7 );
  QStringList labels;
  labels << tr( "Name" ) << tr( "Size" ) << tr( "Date" ) << tr( "Permissions" ) << tr( "Owner" ) << tr( "Group" ) << tr( "Type" );
  setHeaderLabels( labels );

  QIcon iconDirectory = QgsApplication::getThemeIcon( QStringLiteral( "mIconFolder.svg" ) );
  QIcon iconFile = QgsApplication::getThemeIcon( QStringLiteral( "mIconFile.svg" ) );
  QIcon iconDirLink = QgsApplication::getThemeIcon( QStringLiteral( "mIconFolderLink.svg" ) );
  QIcon iconFileLink = QgsApplication::getThemeIcon( QStringLiteral( "mIconFileLink.svg" ) );

  QList<QTreeWidgetItem *> items;

  QDir dir( path );
  QStringList entries = dir.entryList( QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase );
  const auto constEntries = entries;
  for ( const QString &name : constEntries )
  {
    QFileInfo fi( dir.absoluteFilePath( name ) );
    QStringList texts;
    texts << name;
    QString size;
    if ( fi.size() > 1024 )
    {
      size = QStringLiteral( "%1 KiB" ).arg( QString::number( fi.size() / 1024.0, 'f', 1 ) );
    }
    else if ( fi.size() > 1.048576e6 )
    {
      size = QStringLiteral( "%1 MiB" ).arg( QString::number( fi.size() / 1.048576e6, 'f', 1 ) );
    }
    else
    {
      size = QStringLiteral( "%1 B" ).arg( fi.size() );
    }
    texts << size;
    texts << QLocale().toString( fi.lastModified(), QLocale::ShortFormat );
    QString perm;
    perm += fi.permission( QFile::ReadOwner ) ? 'r' : '-';
    perm += fi.permission( QFile::WriteOwner ) ? 'w' : '-';
    perm += fi.permission( QFile::ExeOwner ) ? 'x' : '-';
    // QFile::ReadUser, QFile::WriteUser, QFile::ExeUser
    perm += fi.permission( QFile::ReadGroup ) ? 'r' : '-';
    perm += fi.permission( QFile::WriteGroup ) ? 'w' : '-';
    perm += fi.permission( QFile::ExeGroup ) ? 'x' : '-';
    perm += fi.permission( QFile::ReadOther ) ? 'r' : '-';
    perm += fi.permission( QFile::WriteOther ) ? 'w' : '-';
    perm += fi.permission( QFile::ExeOther ) ? 'x' : '-';
    texts << perm;

    texts << fi.owner();
    texts << fi.group();

    QString type;
    QIcon icon;
    if ( fi.isDir() && fi.isSymLink() )
    {
      type = tr( "folder" );
      icon = iconDirLink;
    }
    else if ( fi.isDir() )
    {
      type = tr( "folder" );
      icon = iconDirectory;
    }
    else if ( fi.isFile() && fi.isSymLink() )
    {
      type = tr( "file" );
      icon = iconFileLink;
    }
    else if ( fi.isFile() )
    {
      type = tr( "file" );
      icon = iconFile;
    }

    texts << type;

    QTreeWidgetItem *item = new QTreeWidgetItem( texts );
    item->setIcon( 0, icon );
    items << item;
  }

  addTopLevelItems( items );

  // hide columns that are not requested
  QgsSettings settings;
  QList<QVariant> lst = settings.value( QStringLiteral( "dataitem/directoryHiddenColumns" ) ).toList();
  const auto constLst = lst;
  for ( const QVariant &colVariant : constLst )
  {
    setColumnHidden( colVariant.toInt(), true );
  }
}

void QgsDirectoryParamWidget::mousePressEvent( QMouseEvent *event )
{
  if ( event->button() == Qt::RightButton )
  {
    // show the popup menu
    QMenu popupMenu;

    QStringList labels;
    labels << tr( "Name" ) << tr( "Size" ) << tr( "Date" ) << tr( "Permissions" ) << tr( "Owner" ) << tr( "Group" ) << tr( "Type" );
    for ( int i = 0; i < labels.count(); i++ )
    {
      QAction *action = popupMenu.addAction( labels[i], this, &QgsDirectoryParamWidget::showHideColumn );
      action->setObjectName( QString::number( i ) );
      action->setCheckable( true );
      action->setChecked( !isColumnHidden( i ) );
    }

    popupMenu.exec( event->globalPos() );
  }
}

void QgsDirectoryParamWidget::showHideColumn()
{
  QAction *action = qobject_cast<QAction *>( sender() );
  if ( !action )
    return; // something is wrong

  int columnIndex = action->objectName().toInt();
  setColumnHidden( columnIndex, !isColumnHidden( columnIndex ) );

  // save in settings
  QgsSettings settings;
  QList<QVariant> lst;
  for ( int i = 0; i < columnCount(); i++ )
  {
    if ( isColumnHidden( i ) )
      lst.append( QVariant( i ) );
  }
  settings.setValue( QStringLiteral( "dataitem/directoryHiddenColumns" ), lst );
}

//
// QgsProjectHomeItem
//

QgsProjectHomeItem::QgsProjectHomeItem( QgsDataItem *parent, const QString &name, const QString &dirPath, const QString &path )
  : QgsDirectoryItem( parent, name, dirPath, path, QStringLiteral( "special:ProjectHome" ) )
{
}

QIcon QgsProjectHomeItem::icon()
{
  if ( state() == Populating )
    return QgsDirectoryItem::icon();
  return QgsApplication::getThemeIcon( QStringLiteral( "mIconFolderProject.svg" ) );
}

QVariant QgsProjectHomeItem::sortKey() const
{
  return QStringLiteral( " 1" );
}



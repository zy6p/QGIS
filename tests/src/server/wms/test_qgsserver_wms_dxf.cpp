/***************************************************************************
     testqgswmsparameters.cpp
     --------------------------------------
    Date                 : 20 Mar 2019
    Copyright            : (C) 2019 by Paul Blottiere
    Email                : paul dot blottiere @ oslandia.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstest.h"
#include "qgsproject.h"
#include "qgsvectorlayer.h"
#include "qgsserverinterfaceimpl.h"
#include "qgswmsparameters.h"
#include "qgswmsrenderer.h"
#include "qgswmsrendercontext.h"
#include "qgsdxfexport.h"

/**
 * \ingroup UnitTests
 * This is a unit test for the WMS parameters parsing
 */
class TestQgsServerWmsDxf : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void use_title_as_layername_true();
    void use_title_as_layername_false();
};

void TestQgsServerWmsDxf::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
}

void TestQgsServerWmsDxf::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsServerWmsDxf::use_title_as_layername_true()
{
  const QString key = "FORMAT_OPTIONS";
  const QString value = "MODE:SYMBOLLAYERSYMBOLOGY;SCALE:250;USE_TITLE_AS_LAYERNAME:FALSE;LAYERATTRIBUTES:name;CODEC:my_codec_name";

  QUrlQuery query;
  query.addQueryItem( key, value );
  query.addQueryItem( "LAYERS", "testlayer èé" );

  const QgsWms::QgsWmsParameters parameters( query );

  QCOMPARE( int( parameters.dxfScale() ), 250 );
  QCOMPARE( parameters.dxfCodec(), QString( "my_codec_name" ) );
  QCOMPARE( parameters.dxfUseLayerTitleAsName(), false );
  QCOMPARE( parameters.dxfMode(), Qgis::FeatureSymbologyExport::PerSymbolLayer );
  QCOMPARE( int( parameters.dxfLayerAttributes().size() ), 1 );
  QCOMPARE( parameters.dxfLayerAttributes()[0], QString( "name" ) );

  const QString filename = QString( "%1/qgis_server/test_project.qgs" ).arg( TEST_DATA_DIR );
  QgsProject project;
  project.read( filename );

  QgsMapLayer *layer = project.layerStore()->mapLayersByName( "testlayer èé" )[0];
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( layer );

  QgsServiceRegistry registry;
  QgsServerSettings settings;
  QgsCapabilitiesCache cache( settings.capabilitiesCacheSize() );
  QgsServerInterfaceImpl interface( &cache, &registry, &settings );

  QgsWms::QgsWmsRenderContext context( &project, &interface );
  context.setFlag( QgsWms::QgsWmsRenderContext::UseWfsLayersOnly );
  context.setFlag( QgsWms::QgsWmsRenderContext::UseOpacity );
  context.setFlag( QgsWms::QgsWmsRenderContext::UseFilter );
  context.setFlag( QgsWms::QgsWmsRenderContext::SetAccessControl );
  context.setParameters( parameters );

  QgsWms::QgsRenderer renderer( context );
  std::unique_ptr<QgsDxfExport> exporter = renderer.getDxf();

  QCOMPARE( exporter->layerName( vl ), QString( "testlayer \u00E8\u00E9" ) );

  const QgsFeature ft = vl->getFeature( 1 );
  QCOMPARE( exporter->layerName( vl->id(), ft ), QString( "two" ) );
}

void TestQgsServerWmsDxf::use_title_as_layername_false()
{
  const QString key = "FORMAT_OPTIONS";
  const QString value = "MODE:SYMBOLLAYERSYMBOLOGY;SCALE:250;USE_TITLE_AS_LAYERNAME:TRUE;LAYERATTRIBUTES:pif,paf,pouf;CODEC:my_codec_name";

  QUrlQuery query;
  query.addQueryItem( key, value );
  query.addQueryItem( "LAYERS", "testlayer èé" );

  const QgsWms::QgsWmsParameters parameters( query );

  QCOMPARE( int( parameters.dxfScale() ), 250 );
  QCOMPARE( parameters.dxfCodec(), QString( "my_codec_name" ) );
  QCOMPARE( parameters.dxfUseLayerTitleAsName(), true );
  QCOMPARE( parameters.dxfMode(), Qgis::FeatureSymbologyExport::PerSymbolLayer );
  QCOMPARE( int( parameters.dxfLayerAttributes().size() ), 3 );
  QCOMPARE( parameters.dxfLayerAttributes()[0], QString( "pif" ) );
  QCOMPARE( parameters.dxfLayerAttributes()[1], QString( "paf" ) );
  QCOMPARE( parameters.dxfLayerAttributes()[2], QString( "pouf" ) );

  const QString filename = QString( "%1/qgis_server/test_project.qgs" ).arg( TEST_DATA_DIR );
  QgsProject project;
  project.read( filename );

  QgsMapLayer *layer = project.layerStore()->mapLayersByName( "testlayer èé" )[0];
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( layer );

  QgsServiceRegistry registry;
  QgsServerSettings settings;
  QgsCapabilitiesCache cache( settings.capabilitiesCacheSize() );
  QgsServerInterfaceImpl interface( &cache, &registry, &settings );

  QgsWms::QgsWmsRenderContext context( &project, &interface );
  context.setFlag( QgsWms::QgsWmsRenderContext::UseWfsLayersOnly );
  context.setFlag( QgsWms::QgsWmsRenderContext::UseOpacity );
  context.setFlag( QgsWms::QgsWmsRenderContext::UseFilter );
  context.setFlag( QgsWms::QgsWmsRenderContext::SetAccessControl );
  context.setParameters( parameters );

  QgsWms::QgsRenderer renderer( context );
  std::unique_ptr<QgsDxfExport> exporter = renderer.getDxf();

  QCOMPARE( exporter->layerName( vl ), QString( "A test vector layer" ) );

  const QgsFeature ft = vl->getFeature( 1 );
  QCOMPARE( exporter->layerName( vl->id(), ft ), QString( "A test vector layer" ) );
}

QGSTEST_MAIN( TestQgsServerWmsDxf )
#include "test_qgsserver_wms_dxf.moc"

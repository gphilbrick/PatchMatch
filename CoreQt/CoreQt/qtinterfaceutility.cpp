#include <qtinterfaceutility.h>

#include <Core/exceptions/runtimeerror.h>
#include <Core/image/imageutility.h>
#include <Core/utility/intcoord.h>
#include <Core/utility/twodarray.h>
#include <Core/utility/vector3.h>
#include <Core/utility/vector4.h>

#include <QColor>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QPoint>
#include <QRect>
#include <QRgb>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>

#include <omp.h>

#include <string>

namespace coreQt {
	
using namespace core;

void fixSizeIssuesWithStackedWidget( QWidget& container, QStackedWidget& stackedWidget )
{
    if( stackedWidget.count() == 0 ) {
        return;
    }

    // I'm sick of sizes not adjusting correctly when I either switch between items in the
    // stacked widget or when I change the size of one of the widgets _in_ the stacked widget.
    // I found a guy complaining about the same issue at https://forum.qt.io/topic/104109/how-to-auto-size-dialog-depending-on-which-stack-is-front/9
    // and I'm copying what worked for him.

    for( int i = 0; i < stackedWidget.count(); i++ ) {
        auto* const widget = stackedWidget.widget( i );
        widget->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Ignored );
    }
    auto* const widget = stackedWidget.currentWidget();
    widget->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );

    stackedWidget.adjustSize();
    container.adjustSize();
}

void forceSetSpinBox( QDoubleSpinBox* spinBox, double f )
{
    if( spinBox->maximum() < f ) {
        spinBox->setMaximum( f );
    }
    if( spinBox->minimum() > f ) {
        spinBox->setMinimum( f );
    }
    spinBox->setValue( f );
}

void forceSetSpinBox( QSpinBox* spinBox, int f )
{
    if( spinBox->maximum() < f ) {
        spinBox->setMaximum( f );
    }
    if( spinBox->minimum() > f ) {
        spinBox->setMinimum( f );
    }
    spinBox->setValue( f );
}

double fFromSlider(const QSlider* slider)
{
    return ((double)(slider->value()-slider->minimum()))/
           ((double)(slider->maximum()-slider->minimum()));
}

void setSliderToF(QSlider* slider, double f)
{
    slider->setValue(
                slider->minimum() +
                (double)(slider->maximum()-slider->minimum())*f
                );
}

IntCoord qPointToIntCoord(const QPoint &point)
{
    return IntCoord( point.x(), point.y() );
}

Vector2 qPointFToVector2( const QPointF& point )
{
    return Vector2( point.x(), point.y() );
}

QPointF vectorToQPointF(const Vector2& point)
{
    return QPointF(point.x(),point.y());
}

QPoint intCoordToQPoint(const IntCoord& coord)
{
    return QPoint(coord.x(),coord.y());
}

void qImageFromRgbaImage(QImage& store,const core::ImageRGBA& rgbaImage, bool alphaChannel )
{
    QImage::Format desiredFormat = (alphaChannel ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    if( store.width() != rgbaImage.width() 
     || store.height() != rgbaImage.height() 
     || store.format() != desiredFormat )
    {
        store = QImage(
            rgbaImage.width(),
            rgbaImage.height(),
            desiredFormat );
    }

    #pragma omp parallel
    {
        #pragma omp for
        for (int y = 0; y < rgbaImage.height(); y++)
        {
            // http://stackoverflow.com/questions/2095039/qt-qimage-pixel-manipulation
            QRgb* const rowData = (QRgb*)store.scanLine(y);
            for (int x = 0; x < rgbaImage.width(); x++)
            {
                if (alphaChannel)
                {
                    QColor color = rgbaToQColor(rgbaImage.get(x, y));
                    rowData[x] = color.rgba();
                } else
                {
                    QColor color = rgbToQColor(rgbaImage.get(x, y).xyz());
                    rowData[x] = color.rgb();
                }
            }
        }
    }
}

void qImageFromRgbImage(QImage& store,const core::ImageRGB& rgbImage)
{
    const QImage::Format desiredFormat = QImage::Format_RGB32;
    if( store.width() != rgbImage.width() 
     || store.height() != rgbImage.height() 
     || store.format() != desiredFormat )
    {
        store = QImage(
            rgbImage.width(),
            rgbImage.height(),
            desiredFormat);
    }

    #pragma omp parallel 
    {
        #pragma omp for
        for (int y = 0; y < rgbImage.height(); y++)
        {
            // http://stackoverflow.com/questions/2095039/qt-qimage-pixel-manipulation
            QRgb* const rowData = (QRgb*)store.scanLine(y);
            for (int x = 0; x < rgbImage.width(); x++)
            {
                QColor color = rgbToQColor(rgbImage.get(x, y));
                rowData[x] = color.rgb();
            }
        }
    }
}

bool saveRgbImageToFile(const core::ImageRGB& rgb, const std::string& filepath )
{
    QImage image;
    qImageFromRgbImage( image, rgb );
    return image.save( QString::fromStdString( filepath ) );
}

bool loadRgbImageFromFile( core::ImageRGB& rgbImage, const std::string& filePath, QSize resizeDims )
{    
    QImage image( QString::fromUtf8( filePath.c_str() ) );
    if( image.isNull() ) {
        return false;
    }
    if( resizeDims.width() > 0 && resizeDims.height() > 0 )
    {
        image = image.scaled(resizeDims,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    }
    return rgbImageFromQImage( rgbImage, image );
}

bool rgbImageFromQImage( core::ImageRGB& rgbImage, const QImage& image )
{
    if( image.isNull() )
    {
        return false;
    }

    rgbImage.recreate( image.width(), image.height() );
    for(int x = 0; x < image.width(); x++ )
    {
        for(int y = 0; y < image.height(); y++ )
        {
            QRgb pixel = image.pixel( x, y );
            QColor color( pixel );
            rgbImage.set( x, y, qColorToRgb( color ) );
        }
    }
    return true;
}

void resizeRgbImage(const core::ImageRGB& originalSize, core::ImageRGB& store, QSize resizeDims)
{
    QImage image;
    qImageFromRgbImage(image,originalSize);
    image = image.scaled(resizeDims,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

    store.recreate(image.width(),image.height());
    for(int x=0; x<image.width(); x++)
    {
        for(int y=0; y<image.height(); y++)
        {
            QRgb pixel = image.pixel(x,y);
            QColor color(pixel);
            store.set(x,y,qColorToRgb(color));
        }
    }
}

bool loadRgbaImageFromFile( core::ImageRGBA& rgbaImage, const std::string& filePath, QSize resizeDims)
{
    QImage image(QString::fromUtf8(filePath.c_str()));
    if(image.isNull())
    {
        return false;
    }
    if(resizeDims.width()>0 && resizeDims.height()>0)
    {
        image =image.scaled(resizeDims,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    }

    rgbaImage.recreate(image.width(),image.height());
    for(int x=0; x<image.width(); x++)
    {
        for(int y=0; y<image.height(); y++)
        {
            QRgb pixel = image.pixel(x,y);
            QColor color(pixel);
            rgbaImage.set(x,y,qColorToRgba(color));
        }
    }
    return true;
}

Vector3 qColorToRgb(const QColor& color)
{
    double r = ((double)color.red())/255.0;
    double g = ((double)color.green())/255.0;
    double b = ((double)color.blue())/255.0;
    return Vector3(r,g,b);
}

Vector4 qColorToRgba(const QColor& color)
{
    Vector3 rgb = qColorToRgb(color);
    double a = ((double)color.alpha())/255.0;
    return Vector4(rgb,a);
}

QColor rgbToQColor(const Vector3& rgb)
{
    int r= (int)(rgb.x()*255.0);
    int g= (int)(rgb.y()*255.0);
    int b= (int)(rgb.z()*255.0);
    return QColor(r,g,b);
}

QColor rgbaToQColor(const Vector4& rgba)
{
    int r= (int)(rgba.x()*255.0);
    int g= (int)(rgba.y()*255.0);
    int b= (int)(rgba.z()*255.0);
    int a= (int)(rgba.w()*255.0);
    return QColor(r,g,b,a);
}

} // coreQt

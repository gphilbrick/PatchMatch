#include <CoreQt/imageeditlabel.h>
#include <CoreQt/qtinterfaceutility.h>

#include <Core/exceptions/runtimeerror.h>
#include <Core/utility/mathutility.h>
#include <Core/utility/intcoord.h>
#include <Core/utility/twodarray.h>
#include <Core/utility/vector3.h>
#include <Core/utility/vector4.h>

#include <QObject>
#include <QWidget>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QRect>
#include <QMargins>
#include <QScrollArea>
#include <QScrollBar>
#include <sstream>

namespace coreQt {
	
using namespace core;

ImageEditLabel::ImageEditLabel(QWidget* parent) : QLabel(parent)
{
    _lastClickPosLabelSpace= QPoint(0,0);
    _rubberBandBeingMade=false;
    _zoom=1.0;

    //This means the pixel map contents will automatically scale to the label's size
    setScaledContents(true);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setMouseTracking(true);
}

void ImageEditLabel::removeLayer(int layerHandle)
{
    auto it = _layers.find(layerHandle);
    if (it == _layers.end()) return;
    _layers.erase(it);

    //delete entry in the visibility map
    std::map<int,bool>::iterator itVisibility = _layersVisible.find(layerHandle);
    if(itVisibility != _layersVisible.end())
    {
        _layersVisible.erase(itVisibility);
    }
}

bool ImageEditLabel::layerVisible(int layerHandle) const
{
    std::map<int,bool>::const_iterator it = _layersVisible.find(layerHandle);
    if(it==_layersVisible.cend()) return true;
    return it->second;
}


ImageEditLabel::~ImageEditLabel()
{
    std::vector<int> layerHandles;
    for( auto it = _layers.cbegin(); it!=_layers.cend(); it++)
    {
        layerHandles.push_back(it->first);
    }
    for(std::vector<int>::const_iterator it = layerHandles.cbegin(); it!=layerHandles.cend(); it++)
    {
        removeLayer(*it);
    }
}

void ImageEditLabel::setLayerVisible(int layerHandle, bool visible)
{
    if (_layers.find(layerHandle) == _layers.end()) {
        return;
    }

    std::map<int,bool>::iterator it = _layersVisible.find(layerHandle);
    if(it!=_layersVisible.end())
    {
        bool current = it->second;
        if(current==visible) return;
        _layersVisible[layerHandle]=visible;
        update();
    }
    else
    {
        _layersVisible[layerHandle] = visible;
        if(!visible)
        {
            update();
        }
    }
}

QImage* ImageEditLabel::getLayerPointer(int layer, bool initIfNull )
{
    QImage* ret = nullptr;
    const auto it = _layers.find(layer);
    if (it != _layers.end()) {
        ret = it->second.get();
    }
    if (!ret && initIfNull) {
        _layers[layer] = std::make_unique< QImage >();
        ret = _layers[layer].get();
    }
    return ret;
}

const QImage* ImageEditLabel::getLayerPointerConst(int layerHandle) const
{
    const auto it = _layers.find(layerHandle);
    if(it==_layers.cend()) return 0;
    return it->second.get();
}

bool ImageEditLabel::layerExists(int handle) const
{
    return getLayerPointerConst(handle)!=0;
}


//What is the size of our label when zoom=1?  It is actually the image
//size of the first layer (usually layer 0)
QSize ImageEditLabel::zoomBaseSize() const
{
    if(_layers.size()==0) return QSize(0,0);
    const auto it = _layers.cbegin();
    return QSize(it->second->width(),it->second->height());
}


QSize ImageEditLabel::labelDisplayArea() const
{
    QSize base = zoomBaseSize();
    double width = _zoom*((double)(base.width()));
    double height = _zoom*((double)(base.height()));
    return QSize(width,height);
}

bool ImageEditLabel::isValidCoord(const IntCoord& coord, int layerHandle) const
{
    return isValidCoord(coord.x(),coord.y(),layerHandle);
}

bool ImageEditLabel::isValidCoord(int x, int y, int layerHandle) const
{
    const QImage* layer = getLayerPointerConst(layerHandle);
    if (!layer) {
        return false;
    }
    return x>=0 && y>=0 && x<layer->width()-1 && y<layer->height()-1;
}

void ImageEditLabel::resetZoomSlot()
{
    changeZoom(1.0);
}

void ImageEditLabel::zoomInSlot()
{
    changeZoom(_zoom*1.25);
}

void ImageEditLabel::zoomOutSlot()
{
    changeZoom(_zoom*0.75);
}

int ImageEditLabel::layerWidth(int layerHandle) const
{
    const QImage* layer = getLayerPointerConst(layerHandle);
    if (!layer) {
        return 0;
    }
    return layer->width();
}

int ImageEditLabel::layerHeight(int layerHandle) const
{
    const QImage* layer = getLayerPointerConst(layerHandle);
    if (!layer) {
        return 0;
    }
    return layer->height();
}

QPointF ImageEditLabel::labelSpaceToLayerSpaceF(QPointF labelPoint, int layerHandle) const
{
    const QImage* layer = getLayerPointerConst(layerHandle);
    if (!layer) {
        return QPointF{};
    }

    double labelX = labelPoint.x();
    double labelY = labelPoint.y();
    QSize labelSize = labelDisplayArea();
    double labelWidth = labelSize.width();
    double labelHeight = labelSize.height();
    double imageWidth = layer->width();
    double imageHeight = layer->height();
    double imageX = (labelX/labelWidth)*imageWidth;
    double imageY = (labelY/labelHeight)*imageHeight;
    return QPointF(imageX,imageY);
}


QPointF ImageEditLabel::layerSpaceToLabelSpaceF(QPointF imagePoint, int layerHandle) const
{
    const QImage* layer = getLayerPointerConst(layerHandle);
    if (!layer) {
        return QPointF{};
    }

    double imageX = imagePoint.x();
    double imageY = imagePoint.y();
    double imageWidth = layer->width();
    double imageHeight = layer->height();
    QSize labelSize = labelDisplayArea();
    double labelWidth = labelSize.width();
    double labelHeight = labelSize.height();
    double labelX = (imageX/imageWidth)*labelWidth;
    double labelY = (imageY/imageHeight)*labelHeight;
    return QPointF(labelX,labelY);
}

QPoint ImageEditLabel::labelSpaceToLayerSpace(QPoint labelPoint, int layerHandle) const
{
    QPointF floatingPoint = labelSpaceToLayerSpaceF(labelPoint,layerHandle);
    return QPoint((int)floatingPoint.x(),(int)floatingPoint.y());
}

QPoint ImageEditLabel::layerSpaceToLabelSpace(QPoint imagePoint, int layerHandle) const
{
    QPointF floatingPoint = layerSpaceToLabelSpaceF(imagePoint,layerHandle);
    return QPoint((int)floatingPoint.x(),(int)floatingPoint.y());
}

QRectF ImageEditLabel::layerSpaceToLabelSpaceF(QRectF imageRect, int layerHandle) const
{
    return QRectF(layerSpaceToLabelSpaceF(imageRect.topLeft(),layerHandle),
                  layerSpaceToLabelSpaceF(imageRect.bottomRight(),layerHandle));

}

QRectF ImageEditLabel::labelSpaceToLayerSpaceF(QRectF labelRect, int layerHandle) const
{
    return QRectF( labelSpaceToLayerSpaceF(labelRect.topLeft(),layerHandle),
                  labelSpaceToLayerSpaceF(labelRect.bottomRight(),layerHandle));
}

QRect ImageEditLabel::layerSpaceToLabelSpace(QRect imageRect, int layerHandle) const
{
    return QRect( layerSpaceToLabelSpace(imageRect.topLeft(),layerHandle),
                  layerSpaceToLabelSpace(imageRect.bottomRight(),layerHandle));
}

QRect ImageEditLabel::labelSpaceToLayerSpace(QRect labelRect,int layerHandle) const
{
    return QRect( labelSpaceToLayerSpace(labelRect.topLeft(),layerHandle),
                  labelSpaceToLayerSpace(labelRect.bottomRight(),layerHandle));
}

void ImageEditLabel::loadLayer(const core::ImageRGB& image,int layerHandle, bool preserveZoomLevel)
{
    QImage* const layer = getLayerPointer(layerHandle,true);
    qImageFromRgbImage(*layer,image);

    //If the user did not ask for the image not to be rezoomed, then rezoom
    if(!preserveZoomLevel) {
        const int minInitialDisplayDimension=100;
        double zoomValue=1.0;
        if(image.width()<minInitialDisplayDimension ||
                image.height()<minInitialDisplayDimension)
        {
            zoomValue = std::max(((double)minInitialDisplayDimension)/((double)image.width()),
                                 ((double)minInitialDisplayDimension)/((double)image.height()));
        }
        changeZoom(zoomValue);
    } else {
        changeZoom(_zoom);
    }

    loadEvent(layerHandle);
    update();
}

void ImageEditLabel::loadLayer(const core::ImageRGBA& image,int layerHandle, bool preserveZoomLevel)
{
    QImage* const layer = getLayerPointer(layerHandle,true);
    qImageFromRgbaImage(*layer,image,true);
    update();
    if(!preserveZoomLevel) {
        changeZoom(1.0);
    }
    loadEvent(layerHandle);
}

void ImageEditLabel::loadLayer(const QImage& image, int layerHandle,bool preserveZoomLevel)
{
    QImage* const layer = getLayerPointer(layerHandle, true);
    *layer = image;
    update();
    if(!preserveZoomLevel) {
        changeZoom(1.0);
    }
    loadEvent(layerHandle);
}

void ImageEditLabel::changeZoom(double newZoom)
{
    //decide whether to ignore the indicated zoom
    QSize baseSize = zoomBaseSize();
    QSize zoomedSize = baseSize*newZoom;
    const int minDim=100;
    const double maxDisplayPixelWidth=40;
    //the first clause in the following if statement is for when the image is already very small on
    //one or two of its dimensions.
    if( zoomedSize.width() < minDim 
     || zoomedSize.height()<minDim ) {
        if(baseSize.width()<baseSize.height()) {
            newZoom = ((double)minDim)/((double)baseSize.width());
        } else {
            newZoom = ((double)minDim)/((double)baseSize.height());
        }
    }
    if(newZoom>maxDisplayPixelWidth) {
        newZoom=maxDisplayPixelWidth;
    }

    _zoom = newZoom;
    zoomedSize = baseSize*_zoom;
    resize(zoomedSize);

    changeZoomBehavior();
}

void ImageEditLabel::paintEvent( QPaintEvent *event )
{
    QPainter painter(this);

    //in label space
    QRectF targetRect(event->rect());

    //draw all the layers
    for(auto it = _layers.cbegin(); it != _layers.cend(); it++)
    {
        if(!layerVisible(it->first)) continue;

        //in image space
        QRectF sourceRect = labelSpaceToLayerSpaceF(targetRect,it->first);
        const auto& layer = it->second;
        painter.drawImage(targetRect,*layer,sourceRect);
    }

    customDrawBehavior(painter);

    //draw the rubber band
    if(_rubberBandBeingMade) {
        QPen pen(Qt::blue);
        painter.setPen(pen);
        painter.drawRect(_rubberBand);
    }
}

void ImageEditLabel::mousePressEvent(QMouseEvent *ev)
{
    _lastClickPosLabelSpace = ev->pos();
    if(ev->button() == Qt::LeftButton)
    {
        _rubberBandBeingMade=true;
        _rubberBand.setTopLeft(ev->pos());
        _rubberBand.setBottomRight(ev->pos());
        update();
    }
}

QSize ImageEditLabel::layerSize(int layerHandle) const
{
    return QSize(layerWidth(layerHandle),layerHeight(layerHandle));
}

void ImageEditLabel::mouseReleaseEvent(QMouseEvent *ev)
{
    if(_rubberBandBeingMade)
    {
        _rubberBand.setBottomRight(ev->pos());

        //The rubber band is in label coordinates.  We need to conver to
        //image coordinates.
        _rubberBand = _rubberBand.normalized();
        const int minDim = 2;
        if(_rubberBand.width()>minDim && _rubberBand.height()>minDim)
        {
            //constrain the rectangle to be inside the label
            QRect labelSpace = _rubberBand.intersected(QRect(QPoint(0,0),labelDisplayArea()));
            rubberBandEvent(labelSpace);
        }
        _rubberBandBeingMade=false;
        update();
    }
}

void ImageEditLabel::mouseMoveEvent(QMouseEvent *ev)
{
    if(_rubberBandBeingMade)
    {
        if(ev->buttons() & Qt::LeftButton)
        {
            _rubberBand.setBottomRight(ev->pos());
            update();
        }
    }
}

void ImageEditLabel::wheelEvent(QWheelEvent *event)
{
    // Had to convert to angleDelta() from delta() on switch to Qt 6.
    int numDegrees = event->angleDelta().y() / 8;

    int numTicks = numDegrees/15;

    const auto mouseCoord = event->position();
    double fX = mouseCoord.x() / ( (double)width() );
    double fY = mouseCoord.y() / ( (double)height() );

    const double scalePerTick = 1.2;
    if( numTicks != 0 )
    {
        double newZoom = _zoom*pow(scalePerTick,numTicks);
        changeZoom(newZoom);
    }    

    // Is my parent/grandparent a scrollarea?  If so, let's adjust where
    // we are located in it.  
    QScrollArea* parentAsScrollArea = qobject_cast<QScrollArea*>(parent());
    if(!parentAsScrollArea)
    {
        if(parent())
        {
            parentAsScrollArea = qobject_cast<QScrollArea*>(parent()->parent());
        }
    }
    if(parentAsScrollArea)
    {
        QScrollBar* horScrollBar = parentAsScrollArea->horizontalScrollBar();
        horScrollBar->setSliderPosition(horScrollBar->minimum() +
                 (int)(fX*((double)(horScrollBar->maximum()-horScrollBar->minimum())))
                                        );
        QScrollBar* vertScrollBar = parentAsScrollArea->verticalScrollBar();
        vertScrollBar->setSliderPosition(vertScrollBar->minimum() +
                 (int)(fY*((double)(vertScrollBar->maximum()-vertScrollBar->minimum())))
                                        );
    }
}

} // coreQt

#ifndef COREQT_IMAGEEDITLABEL_H
#define COREQT_IMAGEEDITLABEL_H

#include <Core/image/imagetypes.h>

#include <map>
#include <memory>

#include <QLabel>
#include <QRect>
#include <QPoint>
#include <QImage>

class QWidget;

namespace core {
class IntCoord;
} // core	

namespace coreQt {

class ImageEditLabel : public QLabel
{
    Q_OBJECT

public:
    ImageEditLabel(QWidget* parent=0);
    virtual ~ImageEditLabel();
    void changeZoom(double newZoom);

    //These replace the image being displayed and invoke loadEvent().  Unless specified,
    //the zoom level is reset to 1.0.  If layer is 0, the base image is affected.  If layer is greater than 0,
    //either a new layer gets made or the existing layer with that id gets its contents replaced.  Note that different
    //layers' images do not have to be the same size.  If layer<0, exception.
    void loadLayer( const core::ImageRGB& image, int layerHandle, bool preserveZoomLevel=false );
    void loadLayer( const core::ImageRGBA& image, int layerHandle, bool preserveZoomLevel=false );
    void loadLayer( const QImage& image, int layerHandle, bool preserveZoomLevel=false);
    void removeLayer(int layerHandle);
    bool layerVisible(int layerHandle) const;
    double getZoom() const { return _zoom; }
    int layerWidth(int layerHandle) const;
    int layerHeight(int layerHandle) const;
    bool isValidCoord(const core::IntCoord& coord, int layerHandle) const;
    bool isValidCoord(int x, int y, int layerHandle) const;
    void setLayerVisible(int layerHandle, bool visible);
    bool layerExists(int) const;
public slots:
    void zoomInSlot();
    void zoomOutSlot();
    void resetZoomSlot();
protected:
    //Qt events
    virtual void paintEvent(QPaintEvent *) override;
    virtual void mouseMoveEvent(QMouseEvent *ev) override;
    virtual void mousePressEvent(QMouseEvent *ev) override;
    virtual void mouseReleaseEvent(QMouseEvent *ev) override;
    virtual void wheelEvent(QWheelEvent *event) override;

    virtual void changeZoomBehavior() {}
    virtual void customDrawBehavior(QPainter&) {}
    /// The rect is in label space and is constrained to not contain pixels outside
    /// valid label space.
    virtual void rubberBandEvent( QRect ) {}
    virtual void loadEvent(int) {} 

    //If no layers exist, returns (0,0).  Else returns sizeof first layer * zoom.
    QSize labelDisplayArea() const;

    QSize layerSize(int layerHandle) const;

    QPoint labelSpaceToLayerSpace(QPoint, int layerHandle) const;
    QRect labelSpaceToLayerSpace(QRect, int layerHandle) const;
    QPoint layerSpaceToLabelSpace(QPoint, int layerHandle) const;
    QRect layerSpaceToLabelSpace(QRect, int layerHandle) const;

    QPointF labelSpaceToLayerSpaceF(QPointF, int layerHandle) const;
    QRectF labelSpaceToLayerSpaceF(QRectF, int layerHandle) const;
    QPointF layerSpaceToLabelSpaceF(QPointF, int layerHandle) const;
    QRectF layerSpaceToLabelSpaceF(QRectF, int layerHandle) const;

    const QImage* getLayerPointerConst(int) const;

    //What is the size of our label when zoom=1?  It is actually the image
    //size of the first layer (usually layer 0)
    QSize zoomBaseSize() const;

private:
    QImage* getLayerPointer( int, bool initIfNull = false );

    //The contents to display
    std::map<int, std::unique_ptr< QImage > > _layers;

    //Says whether or not each layer is visible.  If a layer does not have an entry in here, it is assumed to be visible
    std::map<int,bool> _layersVisible;

    double _zoom;
    bool _rubberBandBeingMade;

    //We want to know what area is being rubber-banded.  This rectangle is in label space, not image space(s).
    QRect _rubberBand;
    //This is in _label space_, not image space(s)
    QPoint _lastClickPosLabelSpace;
};

} // coreQt

#endif // #include 

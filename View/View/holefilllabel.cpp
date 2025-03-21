#include <holefilllabel.h>

#include <Core/utility/vector3.h>
#include <CoreQt/qtinterfaceutility.h>

#include <QMouseEvent>

namespace view {

HoleFillLabel::HoleFillLabel(QWidget* parent) : coreQt::ImageEditLabel(parent)
{
    core::ImageRGB initialImage(500,500,core::Vector3(1,1,1));
    loadLayer(initialImage,0);

    _holeMode = HoleMode::Visible;
    _brushWidth=20;
    _brushDown=false;
}

void HoleFillLabel::toggleHoleVisibility( bool vis )
{
    const auto oldMode = _holeMode;
    _holeMode = vis ? HoleMode::Visible : HoleMode::Hidden;
    if (layerExists( 1 ) ) {
        setLayerVisible( 1, _holeMode == HoleMode::Visible );
    }
    if (_holeMode != oldMode) {
        update();
    }
}

void HoleFillLabel::brieflyHideHole()
{
    const auto oldMode = _holeMode;
    _holeMode = HoleMode::HiddenBriefly;
    if (layerExists(1)) {
        setLayerVisible(1, false);
    }
    if (oldMode == HoleMode::Visible) {
        update();
    }
}

void HoleFillLabel::clearHole()
{
    _hole.recreate(_hole.width(),_hole.height(),false);
    _holeDisplay.recreate(_hole.width(),_hole.height(),core::Vector4(0,0,0,0));
    loadLayer(_holeDisplay,1,true);
    update();
}

void HoleFillLabel::clearHoleSlot()
{
    clearHole();
}

void HoleFillLabel::brushWidthSlot(int width)
{
    _brushWidth=width;
}

void HoleFillLabel::mouseMoveEvent(QMouseEvent *ev)
{
    if (_holeMode == HoleMode::Visible) {
        if (_brushDown)
        {
            QPoint labelSpace = ev->pos();
            QPoint imageSpace = labelSpaceToLayerSpace(labelSpace, 0);
            applyBrush(coreQt::qPointToIntCoord(imageSpace));
        }
    }
}

void HoleFillLabel::wheelEvent(QWheelEvent* event)
{
    if (_holeMode == HoleMode::HiddenBriefly) {
        toggleHoleVisibility(true);
    }
    ImageEditLabel::wheelEvent( event );
}

void HoleFillLabel::mousePressEvent( QMouseEvent *ev )
{
    switch ( _holeMode ) {
    case HoleMode::Visible:
    {
        if ( ev->button() == Qt::LeftButton ) {
            _brushDown = true;
            QPoint labelPos = ev->pos();
            QPoint canvasPos = labelSpaceToLayerSpace(labelPos, 0);
            applyBrush(coreQt::qPointToIntCoord(canvasPos));
        }
        return;
    }
    case HoleMode::HiddenBriefly:
    {
        toggleHoleVisibility(true);
        return;
    }
    default:
    {
        break;
    }
    }
    return;
}

void HoleFillLabel::mouseReleaseEvent(QMouseEvent*)
{
    if( _holeMode == HoleMode::Visible ) {
        _brushDown = false;
    }
}

void HoleFillLabel::applyBrush(const core::IntCoord &imageCoord)
{
    for(int xOff=-_brushWidth/2; xOff<=_brushWidth/2; xOff++)
    {
        for(int yOff=-_brushWidth/2; yOff<=_brushWidth/2; yOff++)
        {
            //I want a circular brush this time round.
            if(sqrt( (double)( xOff*xOff + yOff*yOff ))>((double)_brushWidth)*0.5) continue;

            int x = imageCoord.x()+xOff, y=imageCoord.y()+yOff;
            if(x<0 || y<0 || x>_hole.width()-1 || y>_hole.height()-1) continue;

            _hole.set(x,y,true);
            _holeDisplay.set(x,y,core::Vector4(0,0,1,0.5));
        }
    }
    loadLayer(_holeDisplay,1,true);
}

void HoleFillLabel::setHole(const core::ImageBinary& hole)
{
    core::ImageBinary::clone(hole,_hole);
    _holeDisplay.recreate(_hole.width(),_hole.height());
    for(int x=0; x<_hole.width(); x++) {
        for(int y=0; y<_hole.height(); y++) {
            _holeDisplay.set(x,y,core::Vector4(0,0,1,_hole.get(x,y)? 0.5 : 0));
        }
    }
    loadLayer(_holeDisplay,1,true);
}

void HoleFillLabel::loadEvent( int layer )
{          
    if( layer == 0 ) {
        QSize newSize = layerSize(layer);
        if (_hole.width() != newSize.width() || _hole.height() != newSize.height()) {
            _hole.recreate(newSize.width(), newSize.height(), false);
            _holeDisplay.recreate(newSize.width(), newSize.height(), core::Vector4(0, 0, 0, 0));
            loadLayer(_holeDisplay, 1, true);
        }
    }
}

const core::ImageBinary& HoleFillLabel::hole() const
{
    return _hole;
}

} // view



#ifndef IEC_QTINTERFACEUTILITY_H
#define IEC_QTINTERFACEUTILITY_H

#include <Core/image/imagetypes.h>
#include <Core/utility/twodarray.h>
#include <Core/utility/vector4.h>

#include <string>
#include <QSize>
#include <QImage>

class QColor;
class QCursor;
class QDoubleSpinBox;
class QPoint;
class QPointF;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QString;

namespace core {
class Vector2;
class Vector3;
} // core

namespace coreQt {

double fFromSlider(const QSlider* slider);
void setSliderToF(QSlider* slider, double f);

/// Set the spin-box to the given value, adjusting the minimum or maximum if necessary.
void forceSetSpinBox( QDoubleSpinBox*, double );
void forceSetSpinBox( QSpinBox*, int );

core::IntCoord qPointToIntCoord(const QPoint& point);
core::Vector2 qPointFToVector2( const QPointF& point );
QPoint intCoordToQPoint(const core::IntCoord& coord);
QPointF vectorToQPointF(const core::Vector2& point);

void qImageFromRgbImage(QImage& store,const core::ImageRGB& rgbImage);
void qImageFromRgbaImage(QImage& store,const core::ImageRGBA& rgbaImage, bool alphaChannel=true);

/// Given 'container' which contains 'contained', correct sizing oddities caused by
/// a switch in the current widget of 'contained', etc.
void fixSizeIssuesWithStackedWidget( QWidget& container, QStackedWidget& contained );

bool saveRgbImageToFile(const core::ImageRGB&, const std::string& filepath );
bool loadRgbImageFromFile(core::ImageRGB& rgbImage, const std::string& string,QSize resizeDims=QSize(0,0));
bool loadRgbaImageFromFile(core::ImageRGBA& rgbImage, const std::string& string,QSize resizeDims=QSize(0,0));
bool rgbImageFromQImage( core::ImageRGB& rgbImage, const QImage& source );

void resizeRgbImage(
	const core::ImageRGB& originalSize, 
	core::ImageRGB& store, 
	QSize resizeDims);

core::Vector3 qColorToRgb(const QColor& color);
core::Vector4 qColorToRgba(const QColor& color);
QColor rgbToQColor(const core::Vector3& rgb);
QColor rgbaToQColor(const core::Vector4& rgba);

} // coreQt

#endif // #include 
